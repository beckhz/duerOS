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
 * File: lightduer_ticker.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Provide APIs to get ticker in ms.
 */

#include "lightduer_ticker.h"
#include "us_ticker_api.h"

int duer_read_ticker_ms(void)
{
    return us_ticker_read() / 1000;
}

