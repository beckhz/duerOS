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
 * File: duerapp_alarm.c
 * Auth: Gang Chen(chengang12@baidu.com)
 * Desc: Duer alarm functions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "duerapp_alarm.h"
#include "lightduer_alarm.h"
#include "duerapp_config.h"
#include "lightduer_net_ntp.h"
#include "lightduer_types.h"

// In this sample, we only support 100 alarms.
#define MAX_ALARM_NUM 100
static duerapp_alarm_node alarms[MAX_ALARM_NUM];

static void duer_alarm_callback(void *param)
{
    duerapp_alarm_node *node = (duerapp_alarm_node *)param;

    DUER_LOGI("alarm started: token: %s, url: %s\n", node->token, node->url);

    duer_report_alarm_event(0, node->token, ALERT_START);
    // play url
    duer_timer_release(node->handle);
    node->handle = NULL;
    free(node->token);
    node->token = NULL;
    free(node->url);
    node->url = NULL;
}

static int duer_alarm_set(const int id, const char *token, const char *url, const int time)
{
    size_t len = 0;
    int i = 0;
    int ret = 0;
    duerapp_alarm_node *empty_alarm = NULL;
    DuerTime cur_time;
    size_t delay = 0;
    int rs = DUER_OK;

    for (i = 0; i < sizeof(alarms) / sizeof(alarms[0]); i++) {
        if (alarms[i].token == NULL) {
            empty_alarm = &alarms[i];
            break;
        }
    }

    if (empty_alarm == NULL) {
        DUER_LOGE("Too many alarms\n");
        rs = DUER_ERR_FAILED;
        goto error_out;
    }

    len = strlen(url) + 1;
    empty_alarm->url = malloc(len);
    if (!empty_alarm->url) {
        DUER_LOGI("no memory\n");
        rs = DUER_ERR_MEMORY_OVERLOW;
        goto error_out;
    }
    snprintf(empty_alarm->url, len, "%s", url);

    len = strlen(token) + 1;
    empty_alarm->token = malloc(len);
    if (!empty_alarm->token) {
        DUER_LOGI("no memory\n");
        rs = DUER_ERR_MEMORY_OVERLOW;
        goto error_out;
    }
    snprintf(empty_alarm->token, len, "%s", token);

    DUER_LOGI("set alarm: scheduled_time: %d, token: %s, url: %s\n", time, token, url);

    ret = duer_ntp_client(NULL, 0, &cur_time);
    if (ret < 0) {
        DUER_LOGE("Failed to get NTP time\n");
        rs = DUER_ERR_FAILED;
        goto error_out;
    }

    if (time <= cur_time.sec) {
        DUER_LOGE("The alarm is expired\n");
        rs = DUER_ERR_FAILED;
        goto error_out;
    }

    delay = (time - cur_time.sec) * 1000 + cur_time.usec / 1000;

    empty_alarm->handle = duer_timer_acquire(duer_alarm_callback, empty_alarm, DUER_TIMER_ONCE);
    if (!empty_alarm->handle) {
        DUER_LOGE("Failed to set alarm: failed to create timer\n");
        goto error_out;
    }

    rs = duer_timer_start(empty_alarm->handle, delay);
    if (rs != DUER_OK) {
        rs = DUER_ERR_FAILED;
        DUER_LOGE("Failed to set alarm: failed to start timer\n");
        goto error_out;
    }

    duer_report_alarm_event(id, token, SET_ALERT_SUCCESS);

    return rs;

error_out:
    if (empty_alarm->url) {
        free(empty_alarm->url);
        empty_alarm->url = NULL;
    }

    if (empty_alarm->token) {
        free(empty_alarm->token);
        empty_alarm->token = NULL;
    }

    return rs;
}

static int duer_alarm_delete(const int id, const char *token)
{
    int i = 0;
    duerapp_alarm_node *target_alarm = NULL;

    DUER_LOGI("delete alarm: token %s", token);

    for (i = 0; i < sizeof(alarms) / sizeof(alarms[0]); i++) {
        if (alarms[i].token) {
            if (strcmp(alarms[i].token, token) == 0) {
                target_alarm = &alarms[i];
                break;
            }
        }
    }

    if (!target_alarm) {
        DUER_LOGE("Cannot find the target alarm\n");
        return DUER_ERR_FAILED;
    }

    duer_timer_release(target_alarm->handle);
    target_alarm->handle = NULL;
    free(target_alarm->token);
    target_alarm->token = NULL;
    free(target_alarm->url);
    target_alarm->url = NULL;

    duer_report_alarm_event(id, token, DELETE_ALERT_SUCCESS);

    return DUER_OK;
}

void duer_init_alarm()
{
    duer_alarm_initialize(duer_alarm_set, duer_alarm_delete);
}
