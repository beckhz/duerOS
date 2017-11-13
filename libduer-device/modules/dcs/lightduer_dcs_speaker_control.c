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
 * File: lightduer_dcs_speaker_control.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: apply APIs to support DCS speaker controler interface
 */

#include "lightduer_dcs.h"
#include "stdbool.h"
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"
#include "lightduer_connagent.h"
#include "lightduer_memory.h"
#include "lightduer_log.h"

static bool s_is_controler_supported = false;

static int duer_report_speaker_event(const char *name)
{
    baidu_json *event = NULL;
    baidu_json *payload = NULL;
    baidu_json *data = NULL;
    int volume = 0;
    bool is_mute = false;
    int rs = DUER_OK;

    data = baidu_json_CreateObject();
    if (data == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.speaker_controller",
                                  name,
                                  NULL);
    if (event == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    baidu_json_AddItemToObject(data, "event", event);

    payload = baidu_json_GetObjectItem(event, "payload");
    if (!payload) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    duer_dcs_get_speaker_state(&volume, &is_mute);
    baidu_json_AddNumberToObject(payload, "volume", volume);
    baidu_json_AddBoolToObject(payload, "mute", is_mute);

    rs = duer_data_report(data);

RET:
    if (data) {
        baidu_json_Delete(data);
    }

    return rs;
}

int duer_dcs_on_volume_changed()
{
    return duer_report_speaker_event("VolumeChanged");
}

int duer_dcs_on_mute()
{
    return duer_report_speaker_event("MuteChanged");
}

static baidu_json *duer_parse_volume_directive(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *volume = NULL;

    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        return NULL;
    }

    volume = baidu_json_GetObjectItem(payload, "volume");
    return volume;
}

static duer_status_t duer_volume_set_cb(const baidu_json *directive)
{
    baidu_json *volume = NULL;

    volume = duer_parse_volume_directive(directive);
    if (!volume) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    duer_dcs_volume_set_handler(volume->valueint);
    return DUER_OK;
}

static duer_status_t duer_volume_adjust_cb(const baidu_json *directive)
{
    baidu_json *volume = NULL;

    volume = duer_parse_volume_directive(directive);
    if (!volume) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    duer_dcs_volume_adjust_handler(volume->valueint);
    return DUER_OK;
}

static duer_status_t duer_mute_set_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *mute = NULL;

    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    mute = baidu_json_GetObjectItem(payload, "mute");
    if (!mute) {
        return DUER_MSG_RSP_BAD_REQUEST;
    }

    duer_dcs_mute_handler(mute->type == baidu_json_True);

    return DUER_OK;
}

baidu_json *duer_get_speak_control_state_internal()
{
    baidu_json *state = NULL;
    baidu_json *payload = NULL;
    int volume = 0;
    bool is_mute = false;

    if (!s_is_controler_supported) {
        DUER_LOGV("speaker controler is not supported");
        goto error_out;
    }

    state = duer_create_dcs_event("ai.dueros.device_interface.speaker_controller",
                                  "VolumeState",
                                  NULL);
    if (!state) {
        DUER_LOGE("Filed to create speaker controler state event");
        goto error_out;
    }

    payload = baidu_json_GetObjectItem(state, "payload");
    if (!payload) {
        DUER_LOGE("Filed to get payload item");
        goto error_out;
    }

    duer_dcs_get_speaker_state(&volume, &is_mute);

    baidu_json_AddNumberToObject(payload, "volume", volume);
    baidu_json_AddBoolToObject(payload, "muted", is_mute);

    return state;

error_out:
    if (state) {
        baidu_json_Delete(state);
    }

    return NULL;
}

void duer_dcs_speaker_control_init(void)
{
    static bool is_first_time = true;

    if (!is_first_time) {
        return;
    }

    is_first_time = false;

    duer_directive_list res[] = {
        {"SetVolume", duer_volume_set_cb},
        {"AdjustVolume", duer_volume_adjust_cb},
        {"SetMute", duer_mute_set_cb},
    };

    duer_add_dcs_directive(res, sizeof(res) / sizeof(res[0]));
    s_is_controler_supported = true;
}

