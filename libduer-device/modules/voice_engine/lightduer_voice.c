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
 * File: lightduer_voice.c
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Light duer voice procedure.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_mutex.h"
#include "lightduer_events.h"
#include "lightduer_connagent.h"
#include "lightduer_voice.h"
#include "lightduer_random.h"
#include "lightduer_speex.h"
#include "lightduer_lib.h"

#include "mbedtls/base64.h"

//unused static void *_encoder_buffer;

#define O_BUFFER_LEN        (800)
#define I_BUFFER_LEN        ((O_BUFFER_LEN - 1) / 4 * 3)

#define DISABLE_TOPIC_MASK  (0x80000000)

static char     _data[I_BUFFER_LEN];
static size_t   _size;
static size_t   _capacity = I_BUFFER_LEN;
static size_t   _samplerate = 8000;
volatile static size_t        _topic_id = 0;
volatile static duer_mutex_t   _lock_topic_id = NULL;
static duer_speex_handler     _speex = NULL;

static duer_voice_result_func g_duer_voice_result = NULL;

#ifdef DUER_VOICE_SEND_ASYNC
static duer_events_handler g_events_handler = NULL;
#endif

static duer_voice_mode s_voice_mode = DUER_VOICE_MODE_DEFAULT;

static int duer_send_content(const void *data, size_t size, int eof)
{
    static int  sid = 0;
    baidu_json *voice = NULL;
    baidu_json *value = NULL;

    int rs = DUER_ERR_FAILED;;
    size_t olen;
    char buff[O_BUFFER_LEN];

    eof = eof || data == NULL || size == 0;

    DUER_LOGV("duer_send_content ==>");

    if (data != NULL && size > 0) {
        rs = mbedtls_base64_encode((unsigned char *)buff, O_BUFFER_LEN, &olen, (const unsigned char *)data, size);
        if (rs < 0) {
            DUER_LOGE("Encode the speex data failed: rs = %d", rs);
            goto exit;
        }
    }

    voice = baidu_json_CreateObject();
    baidu_json_AddNumberToObject(voice, "id", _topic_id);
    baidu_json_AddNumberToObject(voice, "segment", sid++);
    baidu_json_AddNumberToObject(voice, "rate", _samplerate);
    baidu_json_AddNumberToObject(voice, "channel", 1);
    baidu_json_AddNumberToObject(voice, "eof", eof ? 1 : 0);
    if (s_voice_mode == DUER_VOICE_MODE_CHINESE_TO_ENGLISH) {
        baidu_json_AddStringToObject(voice, "translate", "c2e");
    } else if (s_voice_mode == DUER_VOICE_MODE_ENGLISH_TO_CHINESE) {
        baidu_json_AddStringToObject(voice, "translate", "e2c");
    } else {
        // do nothing
    }

    if (data != NULL && size > 0) {
        baidu_json_AddStringToObject(voice, "voice_data", buff);
    }

    value = baidu_json_CreateObject();
    baidu_json_AddItemToObject(value, "duer_voice", voice);

    duer_data_report(value);

    baidu_json_Delete(value);

exit:

    if (eof) {
        sid = 0;
    }

    DUER_LOGV("duer_send_content <== rs = %d", rs);

    return rs;
}

static void duer_speex_encoded_callback(const void *data, size_t size)
{
    DUER_LOGD("_size + size:%d, _capacity:%d", _size + size, _capacity);
    if (_size + size > _capacity) {
        duer_send_content(_data, _size, 0);
        _size = 0;
    }

    DUER_MEMCPY(_data + _size, data, size);
    _size += size;
}

size_t duer_increase_topic_id(void)
{
    duer_status_t ret;

    ret = duer_mutex_lock(_lock_topic_id);
    if (ret != DUER_OK) {
        DUER_LOGE("Voice: Lock topic_id failed");

        return 0;
    }

    if (_topic_id == 0) {
        duer_u32_t random = (duer_u32_t)duer_random();
        _topic_id = 10000 + random % 100000;
    }

    _topic_id &= (~DISABLE_TOPIC_MASK);
    _topic_id++;

    ret = duer_mutex_unlock(_lock_topic_id);
    if (ret != DUER_OK) {
        DUER_LOGE("Voice: Lock topic_id failed");

        return 0;
    }

    return _topic_id;
}

static void duer_voice_start_internal(int what, void *object)
{
    size_t ret = 0;
    _samplerate = what;

    if (_speex) {
        duer_speex_destroy(_speex);
    }

    _speex = duer_speex_create(_samplerate);

    ret = duer_increase_topic_id();
    if (!ret || !_speex) {
        DUER_LOGE("Voice start fail, ret:%d, _speex:%p", ret, _speex);
    }
}

static void duer_voice_send_internal(int what, void *object)
{
    if (object) {
        DUER_LOGD("duer_voice_send_internal");
        duer_speex_encode(_speex, object, (size_t)what, duer_speex_encoded_callback);
#ifdef DUER_VOICE_SEND_ASYNC
        DUER_FREE(object);
#endif
    }
}

static void duer_voice_stop_internal(int what, void *object)
{
    if (_speex) {
        duer_speex_encode(_speex, NULL, 0, duer_speex_encoded_callback);

        if(_size > 0){
            duer_send_content(_data, _size, 0);
        }
        duer_send_content(NULL, 0, 1);
        _size = 0;

        duer_speex_destroy(_speex);
        _speex = NULL;
    }
}

static int duer_events_call_internal(duer_events_func func, int what, void *object)
{
#ifdef DUER_VOICE_SEND_ASYNC
    if (g_events_handler) {
        return duer_events_call(g_events_handler, func, what, object);
    } else {
        DUER_LOGE("duer_events_call_internal, handler not initialied...");
    }
    return DUER_ERR_FAILED;
#else
    if (func) {
        func(what, object);
    }
    return DUER_OK;
#endif
}

int duer_voice_initialize(void)
{
#ifdef DUER_VOICE_SEND_ASYNC
    if (g_events_handler == NULL) {
        g_events_handler = duer_events_create("lightduer_voice", 1024 * 4, 10);
    }
#endif

    if (!_lock_topic_id) {
        _lock_topic_id = duer_mutex_create();
        if (!_lock_topic_id) {
            DUER_LOGE("Voice: Create lock failed");

            return DUER_ERR_FAILED;
        }
    }

    return DUER_OK;
}

void duer_voice_finalize(void)
{
#ifdef DUER_VOICE_SEND_ASYNC
    if (g_events_handler) {
        duer_events_destroy(g_events_handler);
        g_events_handler = NULL;
    }
#endif
}

void duer_voice_set_result_callback(duer_voice_result_func callback)
{
    if (g_duer_voice_result != callback) {
        g_duer_voice_result = callback;
    }
}

void duer_voice_set_mode(duer_voice_mode mode)
{
    s_voice_mode = mode;
}

int duer_voice_start(int samplerate)
{
    return duer_events_call_internal(duer_voice_start_internal, samplerate, NULL);
}

int duer_voice_send(const void *data, size_t size)
{
    int rs = DUER_ERR_FAILED;

#ifdef DUER_VOICE_SEND_ASYNC
    void *buff = NULL;
    do {
        buff = DUER_MALLOC(size);
        if (!buff) {
            DUER_LOGE("Alloc temp memory failed!");
            break;
        }

        DUER_MEMCPY(buff, data, size);

        rs = duer_events_call_internal(duer_voice_send_internal, (int)size, buff);
        if (rs != DUER_OK) {
            DUER_FREE(buff);
        }
    } while (0);
#else
    DUER_LOGD("duer_voice_send");
    rs = duer_events_call_internal(duer_voice_send_internal, (int)size, (void *)data);
#endif

    return rs;
}

int duer_voice_stop(void)
{
    return duer_events_call_internal(duer_voice_stop_internal, 0, NULL);
}
