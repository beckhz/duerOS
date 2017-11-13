// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: duerapp_alarm.h
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Duer init alarm.
 */

#ifndef BAIDU_DUER_DUERAPP_DUERAPP_INIT_ALARM_H
#define BAIDU_DUER_DUERAPP_DUERAPP_INIT_ALARM_H

#include "lightduer_timers.h"

typedef struct duerapp_alarm_node_{
    char *token;
    char *url;
    duer_timer_handler handle;
}duerapp_alarm_node;

void duer_init_alarm();

#endif/*BAIDU_DUER_DUERAPP_DUERAPP_INIT_ALARM_H*/
