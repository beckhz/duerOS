// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: lightduer_engine.h
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Light duer IoT CA engine.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_ENGINE_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_ENGINE_H

#include "baidu_json.h"
#include "lightduer_types.h"

#ifdef __cplusplus
extern "C"
#endif

enum duer_engine_events_enum {
    DUER_EVT_CREATE,
    DUER_EVT_START,
    DUER_EVT_ADD_RESOURCES,
    DUER_EVT_SEND_DATA,
    DUER_EVT_DATA_AVAILABLE,
    DUER_EVT_STOP,
    DUER_EVT_DESTROY,
    DUER_EVENTS_TOTAL
};

typedef void (*duer_engine_notify_func)(int event, int status, int what, void *object);

void duer_engine_register_notify(duer_engine_notify_func func);

void duer_engine_create(int what, void *object);

void duer_engine_start(int what, void *object);

void duer_engine_add_resources(int what, void *object);

void duer_engine_data_available(int what, void *object);

int duer_engine_enqueue_report_data(const baidu_json *data);

void duer_engine_send(int what, void *object);

void duer_engine_stop(int what, void *object);

void duer_engine_destroy(int what, void *object);

int duer_engine_qcache_length(void);

duer_bool duer_engine_is_started(void);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_ENGINE_H*/
