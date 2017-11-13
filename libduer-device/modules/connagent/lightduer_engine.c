/**
 * Copyright (2017) Baidu Inc. All rights reserveed.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 * File: lightduer_engine.c
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Light duer IoT CA engine.
 */

#include "lightduer_engine.h"
#include "lightduer_log.h"
#include "lightduer_timers.h"
#include "lightduer_queue_cache.h"
#include "lightduer_connagent.h"
#include "lightduer_ca.h"
#include "lightduer_memory.h"
#include "lightduer_mutex.h"

extern int duer_data_send(void);
static void duer_engine_notify(int event, int status, int what, void *object);
static void duer_engine_clear_data(void);

#define DUER_NOTIFY_ONLY(_evt, _status) duer_engine_notify(_evt, _status, 0, NULL)

static duer_handler g_handler = NULL;
static duer_engine_notify_func g_notify_func = NULL;
static duer_timer_handler g_timer = NULL;

static duer_qcache_handler g_qcache_handler = NULL;
static duer_mutex_t g_qcache_mutex = NULL;

#define DUER_KEEPALIVE_INTERVAL (55 * 1000)
#define DUER_START_TIMEOUT (1 * 1000)

static void duer_timer_expired(void *param)
{
    //TODO(leliang) should move the complicated operations out here.
    // the default timer stack size is only 200 bytes.
    duer_bool is_started = baidu_ca_is_started(g_handler);
    if (is_started) {
        baidu_json *alive = baidu_json_CreateObject();
        if (alive) {
            baidu_json_AddNumberToObject(alive, "alive", 0);
            duer_data_report(alive);
            baidu_json_Delete(alive);
        } else {
            DUER_LOGW("create alive fail(maybe memory not available now)!");
        }
    } else {
        DUER_NOTIFY_ONLY(DUER_EVT_START, DUER_ERR_CONNECT_TIMEOUT);// trigger reconnection
    }
}

static void duer_engine_notify(int event, int status, int what, void *object)
{
    if (g_notify_func) {
        g_notify_func(event, status, what, object);
    } else {
        DUER_LOGI("g_notify_func is not set");
    }
}

static void duer_transport_notify(duer_transevt_e event)
{
    switch (event) {
    case DUER_TEVT_RECV_RDY:
        duer_data_available();
        break;
    case DUER_TEVT_SEND_RDY:
    {
        duer_bool is_started = baidu_ca_is_started(g_handler);
        if (is_started) {
            duer_data_send();
        } else {
            DUER_NOTIFY_ONLY(DUER_EVT_START, DUER_ERR_CONNECT_TIMEOUT);// trigger reconnection
        }
        break;
    }
    case DUER_TEVT_SEND_TIMEOUT:
        DUER_LOGI("duer_transport_notify send timeout!");
        DUER_NOTIFY_ONLY(DUER_EVT_SEND_DATA, DUER_ERR_TRANS_TIMEOUT);
        duer_engine_clear_data();
        break;
    default:
        DUER_LOGI("duer_transport_notify unknow event:%d", event);
        break;
    }
}

void duer_engine_register_notify(duer_engine_notify_func func)
{
    if (g_notify_func != func) {
        g_notify_func = func;
    } else {
        DUER_LOGI("g_notify_func is update to func:%p", func);
    }
}

void duer_engine_create(int what, void *object)
{
    if (g_handler == NULL) {
        g_handler = baidu_ca_acquire(duer_transport_notify);
    }
    if (g_qcache_handler == NULL) {
        g_qcache_handler = duer_qcache_create();
    }
    if (g_qcache_mutex == NULL) {
        if ((g_qcache_mutex = duer_mutex_create()) == NULL) {
            DUER_LOGW("Create mutex failed!!");
        }
    }
    if (g_timer == NULL) {
        g_timer = duer_timer_acquire(duer_timer_expired, NULL, DUER_TIMER_ONCE);
    }
    // now this message will notity twice, see duer_initialize and duer_engine_start
    // TODO need fix this
    DUER_NOTIFY_ONLY(DUER_EVT_CREATE, DUER_OK);
}

void duer_engine_start(int what, void *object)
{
    static int start_timeout;
    duer_status_t rs = DUER_OK;
    int status = DUER_OK;
    duer_bool is_started = baidu_ca_is_started(g_handler);
    if (is_started) {
        DUER_LOGI("already started, what:%d, object:%p", what, object);
        return;
    }
    do {
        DUER_LOGI("duer_engine_start, g_handler:%p, what:%d, object:%p", g_handler, what, object);
        // add this check. so duer_engine_start become re-enterable without the profile param.
        if (what > 0 && object != NULL) {
            start_timeout = DUER_START_TIMEOUT;
            duer_engine_create(0, NULL);
            // object contains the profile content, and what is the length of the profile.
            DUER_LOGI("the length of profile: %p is %d", (const char *)object, (size_t)what);
            rs = baidu_ca_load_configuration(g_handler, (const void *)object, (size_t)what);
            if (rs < DUER_OK) {
                status = DUER_ERR_PROFILE_INVALID;
                break;
            }
        }

        rs = baidu_ca_start(g_handler);
        if (rs == DUER_ERR_TRANS_WOULD_BLOCK) {
            status = DUER_ERR_TRANS_WOULD_BLOCK;
        } else if (rs != DUER_OK) {
            DUER_LOGI("start fail, status:%d, rs:%d", status, rs);
            status = DUER_ERR_FAILED;
        } else {
            // do nothing
        }
    } while (0);


    if (g_timer != NULL) {
        if (status == DUER_OK) {
            duer_timer_start(g_timer, DUER_KEEPALIVE_INTERVAL);
        } else if (status == DUER_ERR_TRANS_WOULD_BLOCK) {
            duer_timer_start(g_timer, start_timeout);
            start_timeout <<= 1;
        } else {
            // do nothing
        }
    } else {
        DUER_LOGW("g_timer is NULL");
    }


    duer_engine_notify(DUER_EVT_START, status, 0, object);
}

void duer_engine_add_resources(int what, void *object)
{
    duer_status_t rs = baidu_ca_add_resources(g_handler, (const duer_res_t *)object, (size_t)what);

    duer_engine_notify(DUER_EVT_ADD_RESOURCES, rs < DUER_OK ? DUER_ERR_FAILED : DUER_OK, what, object);
}

void duer_engine_data_available(int what, void *object)
{
    duer_status_t rs = DUER_OK;
    int status = DUER_OK;
    duer_bool is_started;
    DUER_LOGD("duer_engine_data_available");
    do {
        is_started = baidu_ca_is_stopped(g_handler);
        if (is_started == DUER_TRUE) {
            DUER_LOGW("duer_engine_data_available has stopped");
            break;
        }

        is_started = baidu_ca_is_started(g_handler);

        rs = baidu_ca_data_available(g_handler, NULL);
        if (rs == DUER_ERR_TRANS_WOULD_BLOCK) {
            status = DUER_ERR_TRANS_WOULD_BLOCK;
        } else if (rs < DUER_OK) {
            status = DUER_ERR_FAILED;
        }
    } while (0);

    if (is_started == DUER_TRUE) {
        DUER_NOTIFY_ONLY(DUER_EVT_DATA_AVAILABLE, status);
    } else {
        if (status == DUER_OK && g_timer != NULL) {
            // maybe started to handle the connect timeout
            //duer_timer_stop(g_timer); // stop the timer before start
            duer_timer_start(g_timer, DUER_KEEPALIVE_INTERVAL);
        }
        DUER_NOTIFY_ONLY(DUER_EVT_START, status);
    }
}

void duer_engine_release_data(duer_msg_t *msg)
{
    if (msg) {
        if (msg->payload != NULL) {
            if (DUER_MESSAGE_IS_RESPONSE(msg->msg_code)) {
                DUER_FREE(msg->payload);
            } else {
                baidu_json_release(msg->payload);
            }
            msg->payload = NULL;
        }
        if (g_handler == NULL) {
            DUER_LOGE("release data failed!");
        } else {
            baidu_ca_release_message(g_handler, msg);
        }
    }
}

void duer_engine_clear_data()
{
    duer_msg_t *msg;

    duer_mutex_lock(g_qcache_mutex);
    while ((msg = duer_qcache_pop(g_qcache_handler)) != NULL) {
        duer_engine_release_data(msg);
    }
    duer_mutex_unlock(g_qcache_mutex);
}

int duer_engine_enqueue_report_data(const baidu_json *data)
{
    int rs = DUER_ERR_INVALID_PARAMETER;
    char *content = NULL;
    baidu_json *payload = NULL;
    duer_msg_t *msg = NULL;

    do {
        if (data == NULL || g_qcache_handler == NULL || g_handler == NULL) {
            break;
        }

        payload = baidu_json_CreateObject();
        if (payload == NULL) {
            rs = DUER_ERR_MEMORY_OVERLOW;
            break;
        }

        baidu_json_AddItemReferenceToObject(payload, "data", (baidu_json *)data);
        content = baidu_json_PrintUnformatted(payload);
        DUER_LOGD("enqueue content:%s", content);
        baidu_json_Delete(payload);

        msg = baidu_ca_build_report_message(g_handler, DUER_FALSE);
        if (msg == NULL) {
            rs = DUER_ERR_MEMORY_OVERLOW;
            break;
        }

        msg->payload = (duer_u8_t*)content;
        msg->payload_len = DUER_STRLEN(content);

        duer_mutex_lock(g_qcache_mutex);
        rs = duer_qcache_push(g_qcache_handler, msg);
        duer_mutex_unlock(g_qcache_mutex);
    } while (0);

    if (rs < 0) {
        DUER_LOGE("Report failed: rs = %d", rs);
        if (msg) {
            duer_engine_release_data(msg);
        } else if (content) {
            baidu_json_release(content);
        }
    }

    return rs;
}

int duer_engine_enqueue_response(const duer_msg_t *req, int msg_code, const void *data, duer_size_t size)
{
    int rs = DUER_ERR_INVALID_PARAMETER;
    char *content = NULL;
    duer_msg_t *msg = NULL;

    do {
        if (req == NULL || g_handler == NULL) {
            break;
        }

        msg = baidu_ca_build_response_message(g_handler, req, msg_code);
        if (msg == NULL) {
            rs = DUER_ERR_MEMORY_OVERLOW;
            break;
        }

        if (size > 0) {
            content = (char *)DUER_MALLOC(size);
            if (content == NULL) {
                rs = DUER_ERR_MEMORY_OVERLOW;
                break;
            }
            DUER_MEMCPY(content, data, size);
        }

        msg->payload = (duer_u8_t *)content;
        msg->payload_len = size;

        duer_mutex_lock(g_qcache_mutex);
        rs = duer_qcache_push(g_qcache_handler, msg);
        duer_mutex_unlock(g_qcache_mutex);
    } while (0);

    if (rs < 0) {
        DUER_LOGE("Response failed: rs = %d", rs);
        if (msg) {
            duer_engine_release_data(msg);
        } else if (content) {
            DUER_FREE(content);
        }
    }

    return rs;
}

void duer_engine_send(int what, void *object)
{
    duer_status_t rs = DUER_OK;
    int status = DUER_OK;
    duer_msg_t *msg = NULL;

    do {
        if (baidu_ca_is_stopped(g_handler)) {
            DUER_LOGW("duer_engine_send has stopped");
            break;
        }

        if (g_timer != NULL) {
            duer_timer_stop(g_timer);
        }

        duer_mutex_lock(g_qcache_mutex);
        msg = duer_qcache_top(g_qcache_handler);
        duer_mutex_unlock(g_qcache_mutex);
        if (msg == NULL) {
            break;
        }

        rs = baidu_ca_send_data(g_handler, msg, NULL);
        if (rs == DUER_ERR_TRANS_WOULD_BLOCK) {
            status = DUER_ERR_TRANS_WOULD_BLOCK;
        } else if (rs < DUER_OK) {
            status = DUER_ERR_FAILED;
        } else {
            if (g_timer != NULL) {
                duer_timer_start(g_timer, DUER_KEEPALIVE_INTERVAL);
            }
            DUER_LOGI("data sent: %s", DUER_STRING_OUTPUT((const char *)msg->payload));
            duer_engine_release_data(msg);
            duer_mutex_lock(g_qcache_mutex);
            duer_qcache_pop(g_qcache_handler);
            duer_mutex_unlock(g_qcache_mutex);
        }
    } while (0);

    duer_engine_notify(DUER_EVT_SEND_DATA, status, what, object);
}

void duer_engine_stop(int what, void *object)
{
    duer_status_t rs = DUER_OK;

    do {
        if (baidu_ca_is_stopped(g_handler)) {
            DUER_LOGW("duer_engine_stop has stopped");
            break;
        }

        rs = baidu_ca_stop(g_handler);
        if (rs == DUER_ERR_TRANS_WOULD_BLOCK) {
            DUER_LOGW("baidu_ca_stop DUER_ERR_TRANS_WOULD_BLOCK");
        } else if (rs < DUER_OK) {
            DUER_LOGW("baidu_ca_stop rs:%d", rs);
        } else {
            // do nothing
        }
    } while (0);

    DUER_NOTIFY_ONLY(DUER_EVT_STOP, what);
}

void duer_engine_destroy(int what, void *object)
{
    if (g_timer != NULL) {
        duer_timer_release(g_timer);
        g_timer = NULL;
    }
    if (g_qcache_handler != NULL) {
        duer_engine_clear_data();
        duer_mutex_lock(g_qcache_mutex);
        duer_qcache_destroy(g_qcache_handler);
        duer_mutex_unlock(g_qcache_mutex);
        g_qcache_handler = NULL;
    }
    if (g_qcache_mutex != NULL) {
        duer_mutex_destroy(g_qcache_mutex);
        g_qcache_mutex = NULL;
    }
    if (g_handler != NULL) {
        baidu_ca_release(g_handler);
        g_handler = NULL;
    }

    DUER_NOTIFY_ONLY(DUER_EVT_DESTROY, DUER_OK);
}

int duer_engine_qcache_length()
{
    int len = 0;

    if (g_qcache_handler == NULL) {
        return 0;
    }

    duer_mutex_lock(g_qcache_mutex);
    len =  duer_qcache_length(g_qcache_handler);
    duer_mutex_unlock(g_qcache_mutex);

    return len;
}

duer_bool duer_engine_is_started()
{
    return baidu_ca_is_started(g_handler);
}
