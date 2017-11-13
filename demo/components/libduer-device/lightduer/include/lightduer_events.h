// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: lightduer_events.h
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Light duer events looper.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_EVENTS_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_EVENTS_H

#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*duer_events_func)(int what, void *object);

typedef struct _duer_event_status_message_s {
    int                 _status;
    duer_events_func    _func;
    int                 _what;
    void *              _object;
} duer_evtst_message_t;

typedef void *duer_events_handler;

duer_events_handler duer_events_create(const char *name, size_t stack_size, size_t queue_length);

int duer_events_call(duer_events_handler handler, duer_events_func func, int what, void *object);

void duer_events_destroy(duer_events_handler handler);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_EVENTS_H*/
