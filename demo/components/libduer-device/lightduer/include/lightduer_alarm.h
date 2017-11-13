// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: lightduer_alarm.h
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Light duer alarm APIs.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_ALARM_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_ALARM_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*duer_set_alarm_func)(const int id, const char *token, const char *url, const int time);
typedef int (*duer_delete_alarm_func)(const int id, const char *token);
typedef enum {
    SET_ALERT_SUCCESS,
    SET_ALERT_FAIL,
    DELETE_ALERT_SUCCESS,
    DELETE_ALERT_FAIL,
    ALERT_START,
    ALERT_STOP,
} alarm_event_type;

int duer_report_alarm_event(int id, const char *token, alarm_event_type type);
void duer_alarm_initialize(duer_set_alarm_func set_alarm_cb,
                           duer_delete_alarm_func delete_alarm_cb);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_ALARM_H*/

