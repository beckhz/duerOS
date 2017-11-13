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
/**
 * File: config.h
 * Auth: Su Hao (suhao@baidu.com)
 * Desc: Speex port configuration.
 */

#include "config.h"
#include "lightduer_log.h"

void speex_error(const char *str)
{
    DUER_LOGE("Fatal (internal) error: %s\n", str);
}

void speex_warning(const char *str)
{
#ifndef DISABLE_WARNINGS
    DUER_LOGW("warning: %s\n", str);
#endif
}

void speex_warning_int(const char *str, int val)
{
#ifndef DISABLE_WARNINGS
    DUER_LOGW("warning: %s %d\n", str, val);
#endif
}

void speex_notify(const char *str)
{
#ifndef DISABLE_NOTIFICATIONS
    DUER_LOGI("notification: %s\n", str);
#endif
}
