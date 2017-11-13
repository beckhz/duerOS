// Copyright 2017 Baidu Inc. All Rights Reserved.
// Author: Su Hao (suhao@baidu.com)
//
// Description: Baidu CA configuration.

#ifndef BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_CONF_H
#define BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_CONF_H

#include "lightduer_types.h"

typedef void* duer_conf_handler;

/*
 * Create the configuation from buffer
 *
 * @Param data, in, the configuation stream
 * @Param size, in, the data size
 * @Return duer_conf_handler, the handler for operating the configuation
 */
DUER_INT duer_conf_handler duer_conf_create(const void* data, duer_size_t size);

/*
 * Get the string from the configuration by the key
 *
 * @Param hdlr, in, the handler for configuation
 * @Param key, in, the key of the item
 * @Return const char *, the string value of the key
 */
DUER_INT const char* duer_conf_get_string(duer_conf_handler hdlr, const char* key);

/*
 * Get the unsigned short from the configuration by the key
 *
 * @Param hdlr, in, the handler for configuation
 * @Param key, in, the key of the item
 * @Return duer_u16_t, the unsigned short value of the key
 */
DUER_INT duer_u16_t duer_conf_get_ushort(duer_conf_handler hdlr, const char* key);

/*
 * Get the unsigned int from the configuration by the key
 *
 * @Param hdlr, in, the handler for configuation
 * @Param key, in, the key of the item
 * @Return duer_u32_t, the unsigned int value of the key
 */
DUER_INT duer_u32_t duer_conf_get_uint(duer_conf_handler hdlr, const char* key);

/*
 * Get the string from the configuration by the key
 *
 * @Param hdlr, in, the handler for configuation
 */
DUER_INT void duer_conf_destroy(duer_conf_handler hdlr);

#endif // BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_CONF_H
