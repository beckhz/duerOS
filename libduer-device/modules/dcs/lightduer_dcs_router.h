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
 * File: lightduer_dcs_router.h
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Light duer DCS APIS.
 */
#ifndef BAIDU_DUER_LIGHTDUER_DCS_ROUTER_H
#define BAIDU_DUER_LIGHTDUER_DCS_ROUTER_H

#include "stdlib.h"
#include "baidu_json.h"
#include "lightduer_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef duer_status_t (*dcs_directive_handler)(const baidu_json *directive);

typedef struct {
    const char *directive_name;
    dcs_directive_handler cb;
}duer_directive_list;

/**
 * Add dcs directive.
 *
 * @param directive: the directive info, include directive name, handler, etc.
 * @param count, how many directives will be added.
 * @return none.
 */
void duer_add_dcs_directive(const duer_directive_list *directive, size_t count);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_DCS_ROUTER_H*/
