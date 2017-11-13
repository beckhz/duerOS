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
 * File: lightduer_dcs_voice_input.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: apply APIs to support DCS voice input interface
 */

#include "lightduer_dcs.h"
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"
#include "lightduer_connagent.h"
#include "lightduer_log.h"
#include "lightduer_timers.h"
#include "lightduer_mutex.h"

#ifndef __VAD__
#define ENABLE_LISTEN_TIMEOUT_TIMER
#endif

#ifdef ENABLE_LISTEN_TIMEOUT_TIMER
// If no listen stop directive received in 20s, close the recorder
static const int MAX_LISTEN_TIME = 20000;
static duer_timer_handler s_listen_timer;
static bool s_is_listening;
static duer_mutex_t s_mutex;
#endif

static volatile bool s_is_multiple_dialog = false;

int duer_dcs_on_listen_started(void)
{
    baidu_json *event = NULL;
    baidu_json *header = NULL;
    baidu_json *payload = NULL;
    baidu_json *data = NULL;
    baidu_json *client_context = NULL;
    char buf[10];
    int rs = DUER_OK;

    duer_user_activity_internal();
    s_is_multiple_dialog = false;
    duer_speech_on_stop_internal();
    // Pause audio player
    duer_pause_audio_internal();

#ifdef ENABLE_LISTEN_TIMEOUT_TIMER
    duer_mutex_lock(s_mutex);
    if (!s_is_listening) {
        s_is_listening = true;
        duer_timer_start(s_listen_timer, MAX_LISTEN_TIME);
    }
    duer_mutex_unlock(s_mutex);
#endif

    data = baidu_json_CreateObject();
    if (data == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    client_context = duer_get_client_context_internal();
    if (client_context) {
        baidu_json_AddItemToObject(data, "clientContext", client_context);
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.voice_input",
                                  "ListenStarted",
                                  NULL);
    if (event == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    baidu_json_AddItemToObject(data, "event", event);

    header = baidu_json_GetObjectItem(event, "header");
    snprintf(buf, sizeof(buf), "%d", duer_get_request_id_internal());
    baidu_json_AddStringToObject(header, "dialogRequestId", buf);

    payload = baidu_json_GetObjectItem(event, "payload");
    baidu_json_AddStringToObject(payload, "format", "AUDIO_L16_RATE_16000_CHANNELS_1");

    rs = duer_data_report(data);
RET:
    if (data) {
        baidu_json_Delete(data);
    }

    return rs;
}

int duer_dcs_report_listen_timeout_event(void)
{
    baidu_json *event = NULL;
    baidu_json *data = NULL;
    int rs = DUER_OK;

    data = baidu_json_CreateObject();
    if (data == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.voice_input",
                                  "ListenTimedOut",
                                  NULL);
    if (event == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    baidu_json_AddItemToObject(data, "event", event);
    duer_data_report(data);
RET:
    if (data) {
        baidu_json_Delete(data);
    }

    return rs;
}

static duer_status_t duer_listen_start_cb(const baidu_json *directive)
{
    s_is_multiple_dialog = true;
    // Currently, don't open microphtone automatically
    // duer_dcs_listen_handler();

    return DUER_OK;
}

static duer_status_t duer_listen_stop_cb(const baidu_json *directive)
{
#ifdef ENABLE_LISTEN_TIMEOUT_TIMER
    duer_mutex_lock(s_mutex);
    if (s_is_listening) {
        s_is_listening = false;
        duer_timer_stop(s_listen_timer);
    }
    duer_mutex_unlock(s_mutex);
#endif

    duer_dcs_stop_listen_handler();

    return DUER_OK;
}

#ifdef ENABLE_LISTEN_TIMEOUT_TIMER
static void duer_listen_timeout(void *param)
{
    duer_mutex_lock(s_mutex);
    if (s_is_listening) {
        s_is_listening = false;
        duer_dcs_stop_listen_handler();
        duer_dcs_report_listen_timeout_event();
    }
    duer_mutex_unlock(s_mutex);
}
#endif

bool duer_is_multiple_round_dialogue()
{
    return s_is_multiple_dialog;
}

void duer_dcs_voice_input_init(void)
{
    static bool is_first_time = true;

    if (!is_first_time) {
        return;
    }

    is_first_time = false;

#ifdef ENABLE_LISTEN_TIMEOUT_TIMER
        s_listen_timer = duer_timer_acquire(duer_listen_timeout, NULL, DUER_TIMER_ONCE);
        s_mutex = duer_mutex_create();
#endif

    duer_directive_list res[] = {
        {"Listen", duer_listen_start_cb},
        {"StopListen", duer_listen_stop_cb},
    };

    duer_add_dcs_directive(res, sizeof(res) / sizeof(res[0]));
}

