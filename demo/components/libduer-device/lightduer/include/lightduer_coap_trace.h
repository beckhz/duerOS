// Copyright 2017 Baidu Inc. All Rights Reserved.
// Author: Su Hao (suhao@baidu.com)
//
// Description: Adapter for mbed-trace.

#ifndef BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_MBEDTRACE_H
#define BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_MBEDTRACE_H

#include "lightduer_types.h"

/*
 * Enable the mbed trace
 */
DUER_INT void duer_coap_trace_enable(void);

/*
 * Disable the mbed trace
 */
DUER_INT void duer_coap_trace_disable(void);

#endif // BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_MBEDTRACE_H
