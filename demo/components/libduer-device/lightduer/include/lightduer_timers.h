// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: lightduer_timers.h
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Provide the timer APIs.
 */

#ifndef BAIDU_DUER_LIGHTDUER_PORT_INCLUDE_LIGHTDUER_TIMERS_H
#define BAIDU_DUER_LIGHTDUER_PORT_INCLUDE_LIGHTDUER_TIMERS_H

#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

enum _duer_timer_type_enum {
    DUER_TIMER_ONCE,
    DUER_TIMER_PERIODIC,
    DUER_TIMER_TOTAL
};

typedef void *duer_timer_handler;

typedef void (*duer_timer_callback)(void *);

duer_timer_handler duer_timer_acquire(duer_timer_callback callback, void *param, int type);

int duer_timer_start(duer_timer_handler handle, size_t delay);

int duer_timer_stop(duer_timer_handler handle);

void duer_timer_release(duer_timer_handler handle);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_PORT_INCLUDE_LIGHTDUER_TIMERS_H*/
