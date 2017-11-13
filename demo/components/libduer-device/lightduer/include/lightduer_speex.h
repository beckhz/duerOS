// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: lightduer_speex.h
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Light duer Speex encoder.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SPEEX_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SPEEX_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C"
#endif

typedef void *duer_speex_handler;

typedef void (*duer_encoded_func)(const void *, size_t);

duer_speex_handler duer_speex_create(int rate);

void duer_speex_encode(duer_speex_handler handler, const void *data, size_t size, duer_encoded_func func);

void duer_speex_destroy(duer_speex_handler handler);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_SPEEX_H*/
