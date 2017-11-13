// Copyright 2017 Baidu Inc. All Rights Reserved.
// Author: Su Hao (suhao@baidu.com)
//
// Description: The common type definitions.

#ifndef BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_INCLUDE_BAIDU_CA_TYPES_H
#define BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_INCLUDE_BAIDU_CA_TYPES_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Declaration for local varaible & function.
#define DUER_LOC         static
// Definition for local varaible & function.
#define DUER_LOC_IMPL    static

// Declaration for internal varaible & function.
#define DUER_INT         extern
// Definition for internal varaible & function.
#define DUER_INT_IMPL

// Declaration for external varaible & function.
#define DUER_EXT         extern
// Definition for external varaible & function.
#define DUER_EXT_IMPL

/*
 * The ca handler
 */
typedef void*          duer_handler;

typedef void*          duer_context;
typedef int            duer_status_t;
#ifdef __GNUC__
typedef size_t   duer_size_t;
#else
typedef unsigned int   duer_size_t;
#endif
typedef unsigned int   duer_u32_t;
typedef unsigned short duer_u16_t;
typedef unsigned char  duer_u8_t;
typedef signed int     duer_s32_t;
typedef char           duer_bool;

enum _baidu_ca_bool_e {
    DUER_FALSE,
    DUER_TRUE
};

/*
 * The error codes.
 */
typedef enum _duer_errcode_enum {
    DUER_OK,
    DUER_ERR_FAILED                  = -0x0001,
    DUER_ERR_CONNECT_TIMEOUT         = -0x0002,

    /* Generic Errors */
    DUER_ERR_INVALID_PARAMETER       = -0x0010,
    DUER_ERR_MEMORY_OVERLOW          = -0x0011,
    DUER_ERR_PROFILE_INVALID         = -0x0012,

    /* Network Errors */
    DUER_ERR_TRANS_INTERNAL_ERROR    = -0x0030,
    DUER_ERR_TRANS_WOULD_BLOCK       = -0x0031,
    DUER_ERR_TRANS_TIMEOUT           = -0x0032, // send timeout
    DUER_ERR_REG_FAIL                = -0x0033,

    DUER_ERR_REPORT_FAILED           = -0x070000,
    DUER_ERR_REPORT_FAILED_BEGIN     = DUER_ERR_REPORT_FAILED + 0x10000
} duer_errcode_t;

#endif // BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_INCLUDE_BAIDU_CA_TYPES_H
