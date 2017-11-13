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
 * File: lightduer_dcs_local.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Provide some functions for dcs module local.
 */

#include "lightduer_dcs_local.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"

static volatile int s_dialog_req_id = 0;

baidu_json *duer_create_dcs_event(const char *namespace, const char *name, const char *msg_id)
{
    baidu_json *event = NULL;
    baidu_json *header = NULL;
    baidu_json *payload = NULL;

    event = baidu_json_CreateObject();
    if (event == NULL) {
        goto error_out;
    }

    header = baidu_json_CreateObject();
    if (header == NULL) {
        goto error_out;
    }

    baidu_json_AddStringToObject(header, "namespace", namespace);
    baidu_json_AddStringToObject(header, "name", name);
    if (msg_id) {
        baidu_json_AddStringToObject(header, "messageId", msg_id);
    }
    baidu_json_AddItemToObject(event, "header", header);

    payload = baidu_json_CreateObject();
    if (payload == NULL) {
       goto error_out;
    }

    baidu_json_AddItemToObject(event, "payload", payload);

    return event;

error_out:
    if (event) {
        baidu_json_Delete(event);
    }
    return NULL;
}

int duer_get_request_id_internal()
{
    return s_dialog_req_id++;
}

baidu_json* duer_get_client_context_internal(void)
{
    baidu_json *client_context = NULL;
    baidu_json *state = NULL;

    client_context = baidu_json_CreateArray();
    if (!client_context) {
        DUER_LOGE("Memory overlow");
        return NULL;
    }

    state = duer_get_alert_state_internal();
    if (state) {
        baidu_json_AddItemToArray(client_context, state);
    }

    state = duer_get_playback_state_internal();
    if (state) {
        baidu_json_AddItemToArray(client_context, state);
    }

    state = duer_get_speak_control_state_internal();
    if (state) {
        baidu_json_AddItemToArray(client_context, state);
    }

    state = duer_get_speech_state_internal();
    if (state) {
        baidu_json_AddItemToArray(client_context, state);
    }



    return client_context;
}

char *duer_strdup_internal(const char *str)
{
    int len = 0;
    char *dest = NULL;

    if (!str) {
        return NULL;
    }

    len = strlen(str);
    dest = DUER_MALLOC(len + 1);
    if (!dest) {
        return NULL;
    }

    snprintf(dest, len + 1, "%s", str);
    return dest;
}

