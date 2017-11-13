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
 * File: lightduer.c
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Light duer APIS.
 */

#include <inttypes.h>
#include "lightduer_connagent.h"
#include "lightduer_log.h"
#include "lightduer_lib.h"
#include "lightduer_adapter.h"
#include "lightduer_memory.h"
#include "lightduer_events.h"
#include "lightduer_engine.h"

#define QUEUE_LENGTH    (10)

#define TASK_STACK_SIZE (1024 * 6)

extern void duer_voice_initialize(void);
extern void duer_voice_finalize(void);
extern int duer_data_send(void);
extern int duer_engine_enqueue_response(
        const duer_msg_t *req, int msg_code, const void *data, duer_size_t size);

static duer_event_callback g_event_callback = NULL;
static duer_events_handler g_events_handler = NULL;

static int duer_events_call_internal(duer_events_func func, int what, void *object);

static char* g_event_name[] = {
    "DUER_EVT_CREATE",
    "DUER_EVT_START",
    "DUER_EVT_ADD_RESOURCES",
    "DUER_EVT_SEND_DATA",
    "DUER_EVT_DATA_AVAILABLE",
    "DUER_EVT_STOP",
    "DUER_EVT_DESTROY",
    "DUER_EVENTS_TOTAL"
};

static void duer_event_notify(int event_id, int event_code, void *object)
{
    if (g_event_callback) {
        duer_event_t event;
        event._event = event_id;
        event._code = event_code;
        event._object = object;
        g_event_callback(&event);
    }
}

static void duer_engine_notify_callback(int event, int status, int what, void *object)
{
    if (status != DUER_OK
            && status != DUER_ERR_TRANS_WOULD_BLOCK
            && status != DUER_ERR_CONNECT_TIMEOUT) {
        DUER_LOGI("=====> event: %d[%s], status: %d", event, g_event_name[event], status);
    }
    switch (event) {
    case DUER_EVT_CREATE:
        break;
    case DUER_EVT_START:
        if (object) {
            DUER_FREE(object);
        }
        if (status == DUER_OK) {
        #if !defined(__TARGET_TRACKER__)
            duer_voice_initialize();
        #endif
            duer_event_notify(DUER_EVENT_STARTED, status, NULL);
        } else if (status == DUER_ERR_TRANS_WOULD_BLOCK) {
            DUER_LOGI("will start latter(DUER_ERR_TRANS_WOULD_BLOCK)");
        } else if (status == DUER_ERR_CONNECT_TIMEOUT) {
            int rs = duer_events_call_internal(duer_engine_start, 0, NULL);
            if (rs != DUER_OK) {
                DUER_LOGW("retry connect fail!!");
            }
        } else {
            DUER_LOGE("start fail! status:%d", status);
        }
        break;
    case DUER_EVT_ADD_RESOURCES:
        if (status == DUER_OK) {
            DUER_LOGI("add resource successfully!!");
        }
        if (object) {
            duer_res_t *resources = (duer_res_t *)object;
            for (int i = 0; i < what; ++i) {
                if (resources[i].path) {
                    DUER_FREE(resources[i].path);// more info in duer_add_resources
                    resources[i].path = NULL;
                }
            }
            DUER_FREE(object);
        }
        break;
    case DUER_EVT_SEND_DATA:
        if (object) {
            DUER_LOGI("data sent : %s", (const char *)object);
            baidu_json_release(object);
        }
        if (status >= 0 ) {
            if (duer_engine_qcache_length() > 0) {
                duer_data_send();
            }
        } else if (status == DUER_ERR_TRANS_WOULD_BLOCK) {
            DUER_LOGD("!!!Send WOULD_BLOCK");
        } else {
            //do nothing
        }
        break;
    case DUER_EVT_DATA_AVAILABLE:
        break;
    case DUER_EVT_STOP:
        DUER_LOGI("connect stopped, status:%d!!", status);
    #if !defined(__TARGET_TRACKER__)
        duer_voice_finalize();
    #endif

        duer_event_notify(DUER_EVENT_STOPPED, status, NULL);
        break;
    case DUER_EVT_DESTROY:
        if (g_events_handler) {
            duer_events_destroy(g_events_handler);
            g_events_handler = NULL;
        }
        break;
    default:
        DUER_LOGW("unknown event!!");
        break;
    }

    if (event < DUER_EVT_STOP
            && status != DUER_OK
            && status != DUER_ERR_TRANS_WOULD_BLOCK
            && status != DUER_ERR_CONNECT_TIMEOUT) {
        DUER_LOGE("Action failed: event: %d, status: %d", event, status);
        duer_engine_stop(status, NULL);
    }
}

static int duer_events_call_internal(duer_events_func func, int what, void *object)
{
    if (g_events_handler) {
        return duer_events_call(g_events_handler, func, what, object);
    }
    return DUER_ERR_FAILED;
}

void duer_initialize()
{
    baidu_ca_adapter_initialize();

    if (g_events_handler == NULL) {
        g_events_handler = duer_events_create("lightduer_ca", TASK_STACK_SIZE, QUEUE_LENGTH);
    }

    duer_engine_register_notify(duer_engine_notify_callback);

    int res = duer_events_call_internal(duer_engine_create, 0, NULL);
    if (res != DUER_OK) {
        DUER_LOGE("duer_initialize fail! res:%d", res);
    }
}

void duer_set_event_callback(duer_event_callback callback)
{
    if (g_event_callback != callback) {
        g_event_callback = callback;
    }
}

int duer_start(const void *data, size_t size)
{
    void *profile = DUER_MALLOC(size);
    if (profile == NULL) {
        DUER_LOGE("memory alloc fail!");
        return DUER_ERR_MEMORY_OVERLOW;
    }
    DUER_MEMCPY(profile, data, size);
    DUER_LOGD("use the profile to start engine");
    int rs = duer_events_call_internal(duer_engine_start, (int)size, profile);
    if (rs != DUER_OK) {
        DUER_FREE(profile);
    }
    return rs;
}

int duer_add_resources(const duer_res_t *res, size_t length)
{
    if (duer_engine_is_started() != DUER_TRUE) {
        DUER_LOGW("please add control point after the CA started");
        return DUER_ERR_FAILED;
    }
    if (length == 0 || res == NULL) {
        DUER_LOGW("no resource will be added, length is 0");
        return DUER_ERR_FAILED;
    }

    duer_bool malloc_failed = DUER_FALSE;
    duer_res_t *list = (duer_res_t *)DUER_MALLOC(length * sizeof(duer_res_t));
    if (list == NULL) {
        malloc_failed = DUER_TRUE;
        goto malloc_fail;
    }
    DUER_MEMSET(list, 0, length * sizeof(duer_res_t));
    for (int i = 0; i < length; ++i) {
        list[i].mode = res[i].mode;
        list[i].allowed = res[i].allowed;
        size_t path_len = strlen(res[i].path);
        if (list[i].mode == DUER_RES_MODE_STATIC) {
            // malloc the path&static data once, Note only need free once
            list[i].path = DUER_MALLOC(path_len + res[i].res.s_res.size + 1);
            if (list[i].path == NULL) {
                malloc_failed = DUER_TRUE;
                goto malloc_fail;
            }
            DUER_MEMCPY(list[i].path, res[i].path, path_len);
            list[i].path[path_len] = '\0';
            list[i].res.s_res.size = res[i].res.s_res.size;
            list[i].res.s_res.data = list[i].path + 1 + path_len;
            DUER_MEMCPY(list[i].res.s_res.data, res[i].res.s_res.data, list[i].res.s_res.size);
        } else {
            list[i].path = DUER_MALLOC(path_len + 1);
            if (list[i].path == NULL) {
                malloc_failed = DUER_TRUE;
                goto malloc_fail;
            }
            DUER_MEMCPY(list[i].path, res[i].path, path_len);
            list[i].path[path_len] = '\0';
            list[i].res.f_res = res[i].res.f_res;
        }
    }

    if (malloc_failed == DUER_FALSE) {
        int rs = duer_events_call_internal(duer_engine_add_resources, (int)length, (void *)list);
        if (rs == DUER_OK) {
            return DUER_OK;
        } else {
            DUER_LOGW("add resource fail!!");
            goto add_fail;
        }
    }
malloc_fail:
    DUER_LOGW("memory malloc fail!!");
add_fail:
    if (list) {
        for (int i = 0; i < length; ++i) {
            if (list[i].path) {
                DUER_FREE(list[i].path);
                list[i].path = NULL;
            }
        }
        DUER_FREE(list);
        list = NULL;
    }
    return DUER_ERR_FAILED;
}

int duer_data_send(void)
{
    return duer_events_call_internal(duer_engine_send, 0, NULL);
}

int duer_data_report(const baidu_json *data)
{
    int rs = duer_engine_enqueue_report_data(data);
    if (rs == DUER_OK && duer_engine_qcache_length() == 1) {
        duer_data_send();
    }
    return rs;
}

int duer_response(const duer_msg_t *msg, int msg_code, const void *data, duer_size_t size)
{
    int rs = duer_engine_enqueue_response(msg, msg_code, data, size);
    if (rs == DUER_OK) {
        duer_data_send();
    }
    return rs;
}

int duer_data_available()
{
    return duer_events_call_internal(duer_engine_data_available, 0, NULL);
}

int duer_stop()
{
    return duer_events_call_internal(duer_engine_stop, 0, NULL);
}

int duer_finalize(void)
{
    return duer_events_call_internal(duer_engine_destroy, 0, NULL);
}
