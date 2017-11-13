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
 * File: lightduer_dcs_alert.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Light duer alert procedure.
 */

#include "lightduer_dcs_alert.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "lightduer_dcs.h"
#include "lightduer_dcs_local.h"
#include "lightduer_dcs_router.h"
#include "lightduer_connagent.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_lib.h"

static const char* const s_event_name_tab[] = {"SetAlertSucceeded",
                                               "SetAlertFailed",
                                               "DeleteAlertSucceeded",
                                               "DeleteAlertFailed",
                                               "AlertStarted",
                                               "AlertStopped"};
static bool s_is_alert_supported = false;

int duer_dcs_report_alert_event(const char *token, duer_alert_event_type type)
{
    baidu_json *data = NULL;
    baidu_json *event = NULL;
    baidu_json *payload = NULL;
    int rs = DUER_OK;

    if (!token) {
        DUER_LOGE("Param error: token cannot be null!");
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    if ((int)type < 0 || type >= sizeof(s_event_name_tab) / sizeof(s_event_name_tab[0])) {
        DUER_LOGE("Invalid event type!");
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    data = baidu_json_CreateObject();
    if (data == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.alerts",
                                  s_event_name_tab[type],
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
    baidu_json_AddStringToObject(payload, "token", token);

    duer_data_report(data);

RET:
    if (data) {
        baidu_json_Delete(data);
    }

    return rs;
}

static duer_status_t duer_alert_set_cb(const baidu_json *directive)
{
    int ret = DUER_OK;
    baidu_json *payload = NULL;
    baidu_json *scheduled_time = NULL;
    baidu_json *token = NULL;
    baidu_json *type = NULL;

    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        DUER_LOGE("Failed to get payload");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    token = baidu_json_GetObjectItem(payload, "token");
    if (!token) {
        DUER_LOGE("Failed to get token");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    scheduled_time = baidu_json_GetObjectItem(payload, "scheduledTime");
    if (!scheduled_time) {
        DUER_LOGE("Failed to get scheduledTime");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    type = baidu_json_GetObjectItem(payload, "type");
    if (!type) {
        DUER_LOGE("Failed to get alert type");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    duer_dcs_alert_set_handler(token->valuestring, scheduled_time->valuestring, type->valuestring);

RET:
    return ret;
}

static duer_status_t duer_alert_delete_cb(const baidu_json *directive)
{
    int ret = DUER_OK;
    baidu_json *payload = NULL;
    baidu_json *token = NULL;

    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        DUER_LOGE("Failed to get payload");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    token = baidu_json_GetObjectItem(payload, "token");
    if (!token) {
        DUER_LOGE("Failed to get token");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    duer_dcs_alert_delete_handler(token->valuestring);

RET:
    return ret;
}

void duer_dcs_report_alert(baidu_json *alert_array,
                           const char *token,
                           const char *type,
                           const char *time)
{
    if (!alert_array || !token || !type || !time) {
        DUER_LOGE("Invalid param");
        return;
    }

    baidu_json *alert = baidu_json_CreateObject();
    if (!alert) {
        DUER_LOGE("Memory overlow");
        return;
    }

    baidu_json_AddStringToObject(alert, "token", token);
    baidu_json_AddStringToObject(alert, "type", type);
    baidu_json_AddStringToObject(alert, "scheduledTime", time);

    baidu_json_AddItemToArray(alert_array, alert);
}

baidu_json* duer_get_alert_state_internal(void)
{
    baidu_json *state = NULL;
    baidu_json *payload = NULL;
    baidu_json *alert_array = NULL;

    if (!s_is_alert_supported) {
        return NULL;
    }

    state = duer_create_dcs_event("ai.dueros.device_interface.alerts", "AlertsState", NULL);
    if (!state) {
        DUER_LOGE("Filed to create alert state");
        goto error_out;
    }

    payload = baidu_json_GetObjectItem(state, "payload");
    if (!payload) {
        DUER_LOGE("Filed to get payload item");
        goto error_out;
    }

    alert_array = baidu_json_CreateObject();
    if (!alert_array) {
        DUER_LOGE("Filed to create alert_array");
        goto error_out;
    }

    baidu_json_AddItemToObject(payload, "allAlerts", alert_array);
    duer_dcs_get_all_alert(alert_array);

    return state;

error_out:
    if (state) {
        baidu_json_Delete(state);
    }

    return NULL;
}

void duer_dcs_alert_init()
{
    static bool is_first_time = true;

    if (!is_first_time) {
        return;
    }

    is_first_time = false;

    s_is_alert_supported = true;

    duer_directive_list res[] = {
        {"SetAlert", duer_alert_set_cb},
        {"DeleteAlert", duer_alert_delete_cb},
    };

    duer_add_dcs_directive(res, sizeof(res) / sizeof(res[0]));
}

