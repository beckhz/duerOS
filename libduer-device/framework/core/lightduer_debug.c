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
// Author: Su Hao (suhao@baidu.com)
//
// Description: The debug module.

#include "lightduer_debug.h"
#include <stdarg.h>
#include "lightduer_lib.h"
#include "lightduer_memory.h"

#define DEBUG_BUF_SIZE  (512)

typedef struct _baidu_ca_debug_s {
    duer_context     context;
    duer_debug_f     f_debug;
} duer_debug_t;

DUER_LOC_IMPL duer_debug_t s_duer_debug = {NULL};

DUER_EXT_IMPL void baidu_ca_debug_init(duer_context ctx, duer_debug_f f_debug) {
    s_duer_debug.context = ctx;
    s_duer_debug.f_debug = f_debug;
}

DUER_INT_IMPL void duer_debug_print(duer_u32_t level, const char* file,
                                  duer_u32_t line, const char* msg) {
    if (s_duer_debug.f_debug) {
        s_duer_debug.f_debug(s_duer_debug.context, level, file, line, msg);
    }
}

DUER_INT_IMPL void duer_debug(duer_u32_t level, const char* file, duer_u32_t line,
                            const char* fmt, ...) {
    va_list argp;
    char str[DEBUG_BUF_SIZE];
    int ret;

    if (!s_duer_debug.f_debug) {
        return;
    }

    va_start(argp, fmt);
#if defined(_WIN32)
#if defined(_TRUNCATE)
    ret = _vsnprintf_s(str, DEBUG_BUF_SIZE, _TRUNCATE, fmt, argp);
#else
    ret = _vsnprintf(str, DEBUG_BUF_SIZE, fmt, argp);

    if (ret < 0 || (duer_size_t) ret == DEBUG_BUF_SIZE) {
        str[DEBUG_BUF_SIZE - 1] = '\0';
        ret = -1;
    }

#endif
#else
    ret = vsnprintf(str, DEBUG_BUF_SIZE, fmt, argp);
#endif
    va_end(argp);

    if (ret >= 0) {
        if (ret > DEBUG_BUF_SIZE - 1) {
            ret = DEBUG_BUF_SIZE - 5;
            str[ret++] = '.';
            str[ret++] = '.';
            str[ret++] = '.';
        }

        str[ret++]     = '\n';
        str[ret] = '\0';
    }

    duer_debug_print(level, file, line, str);
}
