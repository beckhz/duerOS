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
 * File: lightduer_dcs_system.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: apply APIs to support DCS audio system interface.
 */

#include "lightduer_dcs_local.h"
#include "lightduer_log.h"
#include "lightduer_ca.h"
#include "lightduer_dcs_router.h"
#include "lightduer_timers.h"
#include "lightduer_connagent.h"
#include "lightduer_ticker.h"

static duer_timer_handler s_activity_timer;
static const int INACTIVIRY_REPORT_PERIOD = 3600 * 1000; // 1 hour
static volatile int s_last_activity_time; // latest user activity time in seconds

void duer_dcs_sync_state(void)
{
    baidu_json *event = NULL;
    baidu_json *data = NULL;
    baidu_json *client_context = NULL;

    data = baidu_json_CreateObject();
    if (data == NULL) {
        goto RET;
    }

    client_context = duer_get_client_context_internal();
    if (client_context) {
        baidu_json_AddItemToObject(data, "clientContext", client_context);
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.system",
                                  "SynchronizeState",
                                  NULL);
    if (event == NULL) {
        goto RET;
    }

    baidu_json_AddItemToObject(data, "event", event);

    duer_data_report(data);
RET:
    if (data) {
        baidu_json_Delete(data);
    }
}

void duer_report_exception_internal(const char* directive, const char* type, const char* msg)
{
    baidu_json *event = NULL;
    baidu_json *data = NULL;
    baidu_json *payload = NULL;
    baidu_json *error = NULL;
    baidu_json *client_context = NULL;

    if (!directive || !type || !msg) {
        goto RET;
    }

    data = baidu_json_CreateObject();
    if (data == NULL) {
        goto RET;
    }

    client_context = duer_get_client_context_internal();
    if (client_context) {
        baidu_json_AddItemToObject(data, "clientContext", client_context);
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.system",
                                  "ExceptionEncountered",
                                  NULL);
    if (event == NULL) {
        goto RET;
    }
    baidu_json_AddItemToObject(data, "event", event);

    payload = baidu_json_GetObjectItem(event, "payload");
    if (!payload) {
        goto RET;
    }
    baidu_json_AddStringToObject(payload, "unparsedDirective", directive);

    error = baidu_json_CreateObject();
    if (!error) {
        goto RET;
    }
    baidu_json_AddItemToObject(payload, "error", error);
    baidu_json_AddStringToObject(error, "type", type);
    baidu_json_AddStringToObject(error, "message", msg);

    duer_data_report(data);
RET:
    if (data) {
        baidu_json_Delete(data);
    }
}

static duer_status_t duer_exception_cb(const baidu_json *directive)
{
    baidu_json* payload = NULL;
    baidu_json* error_code = NULL;
    baidu_json* description = NULL;
    duer_status_t ret = DUER_OK;

    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        DUER_LOGE("Failed to parse payload\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    error_code = baidu_json_GetObjectItem(payload, "code");
    if (!error_code) {
        DUER_LOGE("Failed to get erorr code\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    description = baidu_json_GetObjectItem(payload, "description");
    if (!description) {
        DUER_LOGE("Failed to get erorr description\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    DUER_LOGE("error code: %s", error_code->valuestring);
    DUER_LOGE("error: %s", description->valuestring);

RET:
    return ret;
}

static void duer_inactivity_report(void* param)
{
    int inactivity_time = 0;
    baidu_json* event = NULL;
    baidu_json* data = NULL;
    baidu_json* payload = NULL;

    data = baidu_json_CreateObject();
    if (data == NULL) {
        goto RET;
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.system",
                                  "SynchronizeState",
                                  NULL);
    if (event == NULL) {
        goto RET;
    }

    baidu_json_AddItemToObject(data, "event", event);
    payload = baidu_json_GetObjectItem(event, "payload");
    if (!payload) {
        goto RET;
    }

    inactivity_time = duer_read_ticker_ms() / 1000 - s_last_activity_time;
    inactivity_time = inactivity_time > 0 ? inactivity_time : 0;
    baidu_json_AddNumberToObject(payload, "inactiveTimeInSeconds", inactivity_time);

    duer_data_report(data);

RET:
    if (data) {
        baidu_json_Delete(data);
    }
}

void duer_user_activity_internal(void)
{
    s_last_activity_time = duer_read_ticker_ms() / 1000;
}

static duer_status_t duer_inactivity_reset_cb(const baidu_json *directive)
{
    duer_user_activity_internal();

    return DUER_OK;
}

void duer_declare_sys_interface_internal(void)
{
    static bool is_first_time = true;

    if (!is_first_time) {
        return;
    }

    is_first_time = false;

    s_activity_timer = duer_timer_acquire(duer_inactivity_report, NULL, DUER_TIMER_PERIODIC);
    duer_timer_start(s_activity_timer, INACTIVIRY_REPORT_PERIOD);

    duer_directive_list res[] = {
        {"ThrowException", duer_exception_cb},
        {"ResetUserInactivity", duer_inactivity_reset_cb},
    };

    duer_add_dcs_directive(res, sizeof(res) / sizeof(res[0]));
}

