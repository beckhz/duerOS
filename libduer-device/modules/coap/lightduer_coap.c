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
// Description: baidu_ca_coap

#include "lightduer_coap.h"
#include "ns_types.h"
#include "sn_nsdl.h"
#include "sn_coap_header.h"
#include "sn_nsdl_lib.h"
#include "lightduer_memory.h"
#include "lightduer_log.h"
#include "lightduer_hashcode.h"
#include "lightduer_net_util.h"
#include "lightduer_net_transport_wrapper.h"
#include "lightduer_nsdl_adapter.h"

#define DUER_COAP_TCP_HDR  (0xbeefdead)

#define DUER_LIST_INC_SIZE (10)
//Note1: this value should only bigger than the default value
//Note2: this value setting should follow the value of MBEDTLS_SSL_MAX_CONTENT_LEN
//in src/iot-baidu-ca/include/baidu_ca_mbedtls_config.h
//Note3: pay attention to thread stack size which will call the function bca_coap_data_available
#define DUER_BUFFER_SIZE   (1024 * 2)

typedef struct _baidu_ca_coap_tcp_header_s {
    duer_u32_t       mask;
    duer_u32_t       size;
} duer_coap_hdr_t;

typedef struct _baidu_ca_dynres_s {
    duer_u32_t                  key;
    duer_notify_f               f_res;
    struct _baidu_ca_dynres_s* next;
} duer_dynres_t;

typedef struct _baidu_ca_coap_s {
    duer_status_t        retval;
    duer_trans_handler   trans;
    struct nsdl_s*      nsdl;
    duer_dynres_t*       list;
    duer_coap_result_f   f_result;
    duer_context         context;
    duer_addr_t          remote;
    const void*          key_info;
} duer_coap_t, *duer_coap_ptr;

typedef struct _baidu_ca_nsdl_map_s {
    duer_coap_ptr                 coap;
    struct _baidu_ca_nsdl_map_s* next;
} duer_nsdl_map_t;

DUER_LOC_IMPL duer_nsdl_map_t* s_duer_nsdl_map = NULL;

DUER_LOC const duer_dynres_t* duer_coap_dynamic_resource_find(duer_coap_ptr coap, duer_u32_t key);

DUER_LOC_IMPL void duer_coap_nsdl_add(duer_coap_ptr coap) {
    duer_nsdl_map_t* item = (duer_nsdl_map_t*)DUER_MALLOC(sizeof(duer_nsdl_map_t));

    if (item) {
        item->coap = coap;
        item->next = s_duer_nsdl_map;
        s_duer_nsdl_map = item;
    }
}

DUER_LOC_IMPL void duer_coap_nsdl_del(duer_coap_ptr coap) {
    duer_nsdl_map_t* item = s_duer_nsdl_map;
    duer_nsdl_map_t* p = NULL;

    while (item) {
        if (item->coap == coap) {
            if (p) {
                p->next = item->next;
            } else {
                s_duer_nsdl_map = item->next;
            }
            break;
        }

        p = item;
        item = item->next;
    }

    if (item) {
        DUER_FREE(item);
    }
}

DUER_LOC_IMPL duer_coap_ptr duer_coap_nsdl_get(struct nsdl_s* nsdl) {
    duer_nsdl_map_t* item = s_duer_nsdl_map;

    while (item) {
        if (item->coap && item->coap->nsdl == nsdl) {
            break;
        }
        item = item->next;
    }

    return item ? item->coap : NULL;
}

DUER_LOC_IMPL uint8_t duer_coap_dyn_res_callback(struct nsdl_s* handle,
        sn_coap_hdr_s* hdr,
        sn_nsdl_addr_s* addr,
        sn_nsdl_capab_e cap) {
    duer_coap_ptr coap = duer_coap_nsdl_get(handle);
    duer_status_t rs = DUER_ERR_FAILED;

    if (coap && hdr) {
        duer_u32_t key = duer_hashcode((char*)hdr->uri_path_ptr, hdr->uri_path_len,
                                     (duer_u32_t)coap);
        const duer_dynres_t* res = duer_coap_dynamic_resource_find(coap, key);

        if (res) {
            duer_msg_t msg;
            duer_addr_t ca_addr;
            duer_coap_header_set(&msg, hdr);
            duer_coap_address_set(&ca_addr, addr);
            ca_addr.type = coap->remote.type;
            rs = res->f_res(coap->context, &msg, &ca_addr);
        }
    }

    return rs;
}

DUER_LOC_IMPL uint8_t duer_coap_nsdl_tx(struct nsdl_s* nsdl, sn_nsdl_capab_e cap,
                                      uint8_t* data, uint16_t size, sn_nsdl_addr_s* addr) {
    duer_coap_ptr coap = duer_coap_nsdl_get(nsdl);
    uint8_t rs = 0;
    duer_addr_t addr_in;
    uint8_t* temp = NULL;

    if (!coap) {
        goto error;
    }

    if (coap->remote.type == DUER_PROTO_TCP) {
        temp = DUER_MALLOC(sizeof(duer_coap_hdr_t) + size);

        if (!temp) {
            DUER_LOGW("duer_coap_nsdl_tx: memory overflow!!!");
            goto error;
        }

        duer_coap_hdr_t* ptr = (duer_coap_hdr_t*)temp;
        ptr->mask = duer_htonl(DUER_COAP_TCP_HDR);
        ptr->size = duer_htonl(size);
        DUER_MEMCPY(++ptr, data, size);
        data = temp;
        size += sizeof(duer_coap_hdr_t);
    }

    duer_coap_address_set(&addr_in, addr);
    coap->retval = duer_trans_send(coap->trans, data, size, addr ? &addr_in : NULL);

    if (coap->retval < 0) {
        goto error;
    } else {
        rs = 1; // return > 0 means success
    }

exit:

    if (temp) {
        DUER_FREE(temp);
    }

    return rs;
error:
    rs = 0; // return == 0 means fail
    goto exit;
}

DUER_LOC_IMPL uint8_t duer_coap_nsdl_rx(struct nsdl_s* nsdl, sn_coap_hdr_s* hdr,
                                      sn_nsdl_addr_s* addr) {
    duer_coap_ptr coap = duer_coap_nsdl_get(nsdl);
    uint8_t rs = 0;

    if (coap && coap->f_result) {
        duer_msg_t msg;
        duer_addr_t ca_addr;
        duer_coap_header_set(&msg, hdr);
        duer_coap_address_set(&ca_addr, addr);
        rs = coap->f_result(coap->context, coap, &msg, &ca_addr);
    }

    return rs;
}
void* duer_coap_nsdl_alloc(uint16_t size) {
    return DUER_MALLOC(size);
}

void duer_coap_nsdl_free(void* ptr) {
    DUER_FREE(ptr);
}

DUER_LOC_IMPL const char* duer_coap_convert_path(const char* path) {
    if (*path == '/') {
        path++;
    }

    return path;
}

DUER_LOC_IMPL const duer_dynres_t* duer_coap_dynamic_resource_find(
    duer_coap_ptr coap, duer_u32_t key) {
    const duer_dynres_t* p = NULL;

    if (coap && (p = coap->list) != NULL) {
        while (p && (p->key != key)) {
            p = p->next;
        }
    }

    return p;
}

DUER_LOC_IMPL duer_status_t duer_coap_dynamic_resource_add(duer_coap_ptr coap,
        const duer_res_t* res) {
    duer_dynres_t* dynres = NULL;
    duer_u32_t key;
    duer_status_t rs = DUER_ERR_FAILED;
    const char* path;

    if (!coap || !res) {
        goto error;
    }

    path = duer_coap_convert_path(res->path);
    key = duer_hashcode(path, DUER_STRLEN(path), (duer_u32_t)coap);

    if (duer_coap_dynamic_resource_find(coap, key) != NULL) {
        goto error;
    }

    dynres = DUER_MALLOC(sizeof(duer_dynres_t));

    if (!dynres) {
        goto error;
    }

    dynres->key = key;
    dynres->f_res = res->res.f_res;
    dynres->next = NULL;

    if (!coap->list) {
        coap->list = dynres;
    } else {
        dynres->next = coap->list;
        coap->list = dynres;
    }

    rs = DUER_OK;
exit:
    return rs;
error:

    if (dynres) {
        DUER_FREE(dynres);
    }

    goto exit;
}

DUER_LOC_IMPL duer_status_t duer_coap_dynamic_resource_remove(duer_coap_ptr coap, const char* path) {
    duer_u32_t key;
    duer_status_t rs = DUER_ERR_FAILED;
    duer_dynres_t* p = NULL;
    duer_dynres_t* q = NULL;

    if (!coap || !path || !coap->list) {
        goto exit;
    }

    path = duer_coap_convert_path(path);
    key = duer_hashcode(path, DUER_STRLEN(path), (duer_u32_t)coap);
    p = coap->list;

    while (p && p->key != key) {
        q = p;
        p = p->next;
    }

    if (!p) {
        goto exit;
    }

    if (p == coap->list) {
        coap->list = p->next;
    } else {
        q->next = p->next;
    }

    DUER_FREE(p);
    rs = DUER_OK;
exit:
    return rs;
}

DUER_LOC_IMPL void duer_coap_dynamic_resource_free(duer_coap_ptr coap) {
    duer_dynres_t* p = NULL;
    duer_dynres_t* q = NULL;

    if (!coap || !coap->list) {
        return;
    }

    p = coap->list;

    while (p) {
        q = p;
        p = p->next;
        DUER_FREE(q);
    }

    coap->list = NULL;
}

DUER_INT_IMPL duer_status_t duer_coap_resource_add(duer_coap_handler hdlr, const duer_res_t* res) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    duer_status_t rs = DUER_ERR_FAILED;

    if (coap && res) {
        sn_nsdl_resource_info_s sn_res;
        DUER_MEMSET(&sn_res, 0, sizeof(sn_res));
        sn_res.mode = (sn_nsdl_resource_mode_e)res->mode;
        sn_res.access = (sn_grs_resource_acl_e)res->allowed;
        sn_res.path = (uint8_t*)res->path;
        sn_res.pathlen = DUER_STRLEN(res->path);

        if (res->mode == DUER_RES_MODE_STATIC) {
            sn_res.resource = res->res.s_res.data;
            sn_res.resourcelen = res->res.s_res.size;
        } else {
            duer_coap_dynamic_resource_add(coap, res);
            sn_res.sn_grs_dyn_res_callback = duer_coap_dyn_res_callback;
        }

        if (sn_nsdl_create_resource(coap->nsdl, &sn_res) == SN_NSDL_SUCCESS) {
            rs = DUER_OK;
        }
    }

    return rs;
}

DUER_INT_IMPL duer_status_t duer_coap_resource_remove(duer_coap_handler hdlr, const char* path) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    duer_status_t rs = DUER_ERR_FAILED;

    if (coap && path) {
        if (sn_nsdl_delete_resource(coap->nsdl, DUER_STRLEN(path),
                                    (uint8_t*)path) == SN_NSDL_SUCCESS) {
            duer_coap_dynamic_resource_remove(coap, path);
            rs = DUER_OK;
        }
    }

    return rs;
}

DUER_INT_IMPL duer_coap_handler duer_coap_acquire(duer_coap_result_f f_result,
        duer_context ctx, duer_transevt_func ctx_socket, const void *key_info) {
    duer_coap_ptr coap = NULL;
    coap = DUER_MALLOC(sizeof(duer_coap_t));

    if (!coap) {
        DUER_LOGE("duer_coap_acquire: coap alloc failed");
        goto error;
    }

    DUER_MEMSET(coap, 0, sizeof(duer_coap_t));
    coap->trans = duer_trans_acquire(ctx_socket, key_info);

    if (!coap->trans) {
        DUER_LOGE("duer_coap_acquire: transport acquire failed");
        goto error;
    }

    coap->nsdl = sn_nsdl_init(duer_coap_nsdl_tx, duer_coap_nsdl_rx,
                              duer_coap_nsdl_alloc, duer_coap_nsdl_free);

    if (!coap->nsdl) {
        goto error;
    }

    duer_coap_nsdl_add(coap);
    coap->f_result = f_result;
    coap->context = ctx;
    coap->key_info = key_info;
    goto exit;
error:
    duer_coap_release(coap);
    coap = NULL;
exit:
    return coap;
}

DUER_INT_IMPL duer_status_t duer_coap_connect(duer_coap_handler hdlr,
        const duer_addr_t* addr, const void* data, duer_size_t size) {
    duer_coap_ptr coap   = (duer_coap_ptr)hdlr;
    duer_status_t rs     = DUER_ERR_FAILED;
    duer_addr_t* pAddr  = NULL;

    if (!coap || !addr) {
        DUER_LOGE("duer_coap_connect: Invalid arguments.");
        goto exit;
    }

    duer_trans_set_pk(coap->trans, data, size);
    pAddr = &(coap->remote);

    if (pAddr->host) {
        DUER_FREE(pAddr->host);
        pAddr->host = NULL;
    }

    if (addr->host || addr->host_size > 0) {
        pAddr->host = DUER_MALLOC(addr->host_size);

        if (!pAddr->host) {
            goto exit;
        }

        DUER_MEMCPY(pAddr->host, addr->host, addr->host_size);
        pAddr->host_size = addr->host_size;
        pAddr->port = addr->port;
        pAddr->type = addr->type;
    }

    rs = duer_trans_connect(coap->trans, addr);

    if (rs < DUER_OK && rs != DUER_ERR_TRANS_WOULD_BLOCK) {
        DUER_LOGE("duer_coap_connect: transport connect failed by %d", rs);
    }

exit:
    return rs;
}

DUER_INT_IMPL duer_status_t duer_coap_disconnect(duer_coap_handler hdlr) {
    duer_coap_ptr coap   = (duer_coap_ptr)hdlr;
    return duer_trans_close(coap ? coap->trans : NULL);
}

DUER_INT_IMPL duer_status_t duer_coap_send(duer_coap_handler hdlr,
                                        const duer_msg_t* hdr) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    duer_status_t rs = DUER_ERR_FAILED;

    if (coap && coap->nsdl) {
        sn_coap_hdr_s coap_hdr;
        sn_nsdl_addr_s addr;
        sn_coap_options_list_s opt_list;
        duer_nsdl_address_set(&addr, &(coap->remote));
        duer_nsdl_header_set(&coap_hdr, &opt_list, hdr);
        coap->retval = DUER_ERR_FAILED;
        rs = sn_nsdl_send_coap_message(coap->nsdl, &addr, &coap_hdr);

        if (coap->retval <= 0) {
            if (coap->retval != DUER_ERR_TRANS_WOULD_BLOCK) {
                DUER_LOGW("duer_coap_nsdl_tx: sent = %d", coap->retval);
            }

            rs = coap->retval;
        }
    }

    return rs;
}

DUER_INT_IMPL duer_status_t duer_coap_register(duer_coap_handler hdlr,
        const duer_coap_ep_t* ep) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    duer_status_t rs = DUER_ERR_FAILED;

    if (coap && coap->nsdl && ep) {
        sn_nsdl_ep_parameters_s endpoint;
        DUER_MEMSET(&endpoint, 0, sizeof(sn_nsdl_ep_parameters_s));
        endpoint.endpoint_name_ptr = (uint8_t*)ep->name_ptr;
        endpoint.endpoint_name_len = ep->name_len;
        endpoint.type_ptr = (uint8_t*)ep->type_ptr;
        endpoint.type_len = ep->type_len;
        endpoint.lifetime_ptr = (uint8_t*)ep->lifetime_ptr;
        endpoint.lifetime_len = ep->lifetime_len;
        endpoint.binding_and_mode = BINDING_MODE_U;

        if (set_NSP_address(coap->nsdl, (uint8_t*)"", 0,
                            SN_NSDL_ADDRESS_TYPE_IPV4) == 0) {
            coap->retval = DUER_ERR_FAILED;
            rs = sn_nsdl_register_endpoint(coap->nsdl, &endpoint);
            DUER_LOGV("sn_nsdl_register_endpoint: rs = %d, retval = %d", rs, coap->retval);

            if (coap->retval < 0) {
                rs = coap->retval;
            }
        }
    }

    return rs;
}

DUER_INT_IMPL duer_status_t duer_coap_unregister(duer_coap_handler hdlr) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    duer_status_t rs = DUER_ERR_FAILED;

    if (coap && coap->nsdl) {
        coap->retval = DUER_ERR_FAILED;
        rs = sn_nsdl_unregister_endpoint(coap->nsdl);
        DUER_LOGV("sn_nsdl_unregister_endpoint: rs = %d, retval = %d", rs, coap->retval);

        if (coap->retval < 0) {
            rs = coap->retval;
        }
    }

    return rs;
}

DUER_INT_IMPL duer_status_t duer_coap_update_registration(duer_coap_handler hdlr,
        duer_u32_t lifetime) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    duer_status_t rs = DUER_ERR_FAILED;

    if (coap && coap->nsdl) {
        uint8_t strlt[LIFETIME_MAX_LEN];
        duer_size_t len = DUER_SNPRINTF((char*)strlt, LIFETIME_MAX_LEN, "%d", lifetime);
        coap->retval = DUER_ERR_FAILED;
        rs = sn_nsdl_update_registration(coap->nsdl, strlt, len);
        DUER_LOGV("duer_coap_update_registration: rs = %d, retval = %d", rs,
                  coap->retval);

        if (coap->retval < 0) {
            rs = coap->retval;
        }
    }

    return rs;
}

/*
 * Receive the CoAP message
 *
 * @Param hdlr, in, the CoAP context
 * @Param data, in, the receive buffer
 * @Param size, in, the receive buffer size
 * @Return duer_status_t, the result
 */
DUER_LOC_IMPL duer_status_t duer_coap_recv(duer_coap_handler hdlr, void* data,
                                        duer_size_t size) {
    duer_status_t rs = DUER_ERR_FAILED;
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;

    if (coap) {
        if (coap->remote.type == DUER_PROTO_TCP) {
            duer_coap_hdr_t value = {0, 0};
            rs = duer_trans_recv(coap->trans, &value, sizeof(value), NULL);
            value.mask = duer_htonl(value.mask);
            value.size = duer_htonl(value.size);

            //TODO(leliang)if recv size is smaller than sizeof(value), then enter an unknown situation
            if (rs != sizeof(value) || value.mask != DUER_COAP_TCP_HDR
                    || value.size > size) {
                if (rs != DUER_ERR_TRANS_WOULD_BLOCK) {
                    DUER_LOGI("duer_coap_recv: get header error rs = %d, mask = %x, size = %d",
                              rs, value.mask, value.size);
                }
                goto exit;
            }

            size = value.size;
        }

        rs = duer_trans_recv(coap->trans, data, size, NULL);

        if (rs > 0) {
            sn_nsdl_addr_s addr;
            duer_nsdl_address_set(&addr, &(coap->remote));
            rs = sn_nsdl_process_coap(coap->nsdl, data, rs, &addr);
            if (rs < 0) {
                DUER_LOGW("invalid coap message!!");
            }
        }
    }

exit:

    if (coap) {
        DUER_LOGV("duer_coap_recv: rs = %d", rs);
    }

    return rs;
}

DUER_INT_IMPL duer_status_t duer_coap_data_available(duer_coap_handler hdlr) {
    duer_status_t rs = DUER_ERR_FAILED;
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    char data[DUER_BUFFER_SIZE];

    if (!coap) {
        goto exit;
    }

    rs = duer_coap_recv(coap, data, sizeof(data));
exit:
    return rs;
}

DUER_INT_IMPL duer_status_t duer_coap_exec(duer_coap_handler hdlr,
                                        duer_u32_t timestamp) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;

    if (coap) {
        sn_nsdl_exec(coap->nsdl, timestamp);
    }

    return DUER_ERR_FAILED;
}

DUER_INT_IMPL duer_status_t duer_coap_release(duer_coap_handler hdlr) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;

    if (coap) {
        duer_coap_nsdl_del(coap);

        if (coap->trans) {
            duer_trans_release(coap->trans);
            coap->trans = NULL;
        }

        if (coap->nsdl) {
            sn_nsdl_destroy(coap->nsdl);
            coap->nsdl = NULL;
        }

        duer_addr_t* pAddr = &(coap->remote);

        if (pAddr->host) {
            DUER_FREE(pAddr->host);
        }

        duer_coap_dynamic_resource_free(coap);
        DUER_FREE(coap);
        coap = NULL;
    }

    return DUER_OK;
}

DUER_INT_IMPL duer_status_t duer_coap_set_read_timeout(duer_coap_handler hdlr,
        duer_u32_t timeout) {
    duer_coap_ptr coap = (duer_coap_ptr)hdlr;
    return duer_trans_set_read_timeout(coap ? coap->trans : NULL, timeout);
}
