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
 * File: lightduer_dcs_router.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Light duer dcs directive router.
 */

#include "lightduer_dcs_router.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lightduer_dcs_local.h"
#include "lightduer_log.h"
#include "lightduer_memory.h"
#include "lightduer_connagent.h"
#include "lightduer_lib.h"
#include "lightduer_ca.h"
#include "lightduer_mutex.h"

static size_t s_directive_count = 0;
static duer_directive_list *s_dcs_directive_list = NULL;
static duer_mutex_t s_dcs_router_lock;

void duer_add_dcs_directive(const duer_directive_list *directive, size_t count)
{
    size_t size = 0;
    int i = 0;
    char *name = NULL;

    if (!directive || (count == 0)) {
        DUER_LOGE("Failed to add dcs directive: param error\n");
        return;
    }

    duer_mutex_lock(s_dcs_router_lock);
    if (!s_dcs_directive_list) {
        s_dcs_directive_list = DUER_MALLOC(sizeof(duer_directive_list) * count);
    } else {
        size = sizeof(duer_directive_list) * (count + s_directive_count);
        s_dcs_directive_list = DUER_REALLOC(s_dcs_directive_list, size);
    }

    if (!s_dcs_directive_list) {
        duer_mutex_unlock(s_dcs_router_lock);
        DUER_LOGE("Failed to add dcs directive: no memory\n");
        return;
    }

    for (i = 0; i < count; i++) {
        s_dcs_directive_list[s_directive_count + i].cb = directive[i].cb;
        name = duer_strdup_internal(directive[i].directive_name);
        if (name) {
            s_dcs_directive_list[s_directive_count + i].directive_name = name;
        } else {
            DUER_LOGE("Memory too low\n");
            break;
        }
    }

    s_directive_count += i;
    duer_mutex_unlock(s_dcs_router_lock);
}

duer_status_t duer_dcs_router(duer_context ctx, duer_msg_t* msg, duer_addr_t* addr)
{
    int i = 0;
    dcs_directive_handler handler = NULL;
    char *payload = NULL;
    duer_status_t rs = DUER_OK;
    baidu_json *value = NULL;
    baidu_json *name = NULL;
    baidu_json *header = NULL;
    baidu_json *directive = NULL;

    DUER_LOGV("Enter");

    if (!msg) {
        rs = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    if (!msg->payload || msg->payload_len <= 0) {
        rs = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    payload = (char *)DUER_MALLOC(msg->payload_len + 1);
    if (!payload) {
        DUER_LOGE("Memory not enough\n");
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    DUER_MEMCPY(payload, msg->payload, msg->payload_len);
    payload[msg->payload_len] = '\0';

    DUER_LOGI("payload: %s", payload);

    value = baidu_json_Parse(payload);
    if (value == NULL) {
        DUER_LOGE("Failed to parse payload");
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    directive = baidu_json_GetObjectItem(value, "directive");
    if (!directive) {
        DUER_LOGE("Failed to parse directive");
        rs = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    header = baidu_json_GetObjectItem(directive, "header");
    if (!header) {
        DUER_LOGE("Failed to parse header");
        rs = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    name = baidu_json_GetObjectItem(header, "name");
    if (!name) {
        DUER_LOGE("Failed to parse directive name");
        rs = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    duer_mutex_lock(s_dcs_router_lock);
    for (i = 0; i < s_directive_count; i++) {
        if (strcmp(s_dcs_directive_list[i].directive_name, name->valuestring) == 0) {
            handler = s_dcs_directive_list[i].cb;
        }
    }
    duer_mutex_unlock(s_dcs_router_lock);

    if (!handler) {
        DUER_LOGE("unrecognized directive: %s\n", name->valuestring);
        rs = DUER_MSG_RSP_NOT_FOUND;
        goto RET;
    }

    rs = handler(directive);

RET:
    if (rs != DUER_OK) {
        if (rs == DUER_MSG_RSP_NOT_FOUND) {
            duer_report_exception_internal(payload, "UNSUPPORTED_OPERATION", "Unknow directive");
        } else if (rs == DUER_MSG_RSP_BAD_REQUEST) {
            duer_report_exception_internal(payload,
                                           "UNEXPECTED_INFORMATION_RECEIVED",
                                           "Bad directive");
        } else {
            duer_report_exception_internal(payload, "INTERNAL_ERROR", "Internal error");
        }
    }

    if (payload) {
        DUER_FREE(payload);
    }

    if (value) {
        baidu_json_Delete(value);
    }

    return rs;
}

void duer_dcs_framework_init()
{
    duer_res_t res[] = {
        {DUER_RES_MODE_DYNAMIC, DUER_RES_OP_PUT, "duer_directive", duer_dcs_router},
    };

    duer_add_resources(res, sizeof(res) / sizeof(res[0]));

    if (!s_dcs_router_lock) {
        s_dcs_router_lock = duer_mutex_create();
    }

    duer_declare_sys_interface_internal();
}

