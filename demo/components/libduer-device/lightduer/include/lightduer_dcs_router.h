// Copyright (2017) Baidu Inc. All rights reserveed.
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
 * @param list: the directive info, include directive name, handler, etc.
 * @param count, how many directives will be added.
 * @return none.
 */
void duer_add_dcs_directive(const duer_directive_list *list, size_t count);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_DCS_ROUTER_H*/
