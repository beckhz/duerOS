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
 * File: lightduer_queue_cache.c
 * Auth: Su Hao(suhao@baidu.com)
 * Desc: Provide the Cache util in the Queue.
 */

#include "lightduer_queue_cache.h"
//#include "lightduer_connagent.h"
#include "lightduer_memory.h"
#include "lightduer_lib.h"

typedef struct _duer_qcnode_s {
    void *                  _data;
    struct _duer_qcnode_s * _next;
} duer_qcnode_t;

typedef struct _duer_qcache_s {
    duer_qcnode_t   *_head;
    duer_qcnode_t   *_tail;
    size_t          _length;
} duer_qcache_t;

static duer_qcnode_t *duer_qcache_node_create(void *data)
{
    duer_qcnode_t *node = (duer_qcnode_t *)DUER_MALLOC(sizeof(duer_qcnode_t));
    if (node) {
        node->_data = data;
        node->_next = NULL;
    }
    return node;
}

duer_qcache_handler duer_qcache_create(void)
{
    duer_qcache_t *cache = (duer_qcache_t *)DUER_MALLOC(sizeof(duer_qcache_t));
    if (cache) {
        DUER_MEMSET(cache, 0, sizeof(duer_qcache_t));
    }
    return (duer_qcache_handler)cache;
}

int duer_qcache_push(duer_qcache_handler cache, void *data)
{
    int rs = DUER_ERR_INVALID_PARAMETER;
    duer_qcache_t *p = (duer_qcache_t *)cache;
    duer_qcnode_t *q = NULL;

    do {
        if (p == NULL) {
            break;
        }

        q = duer_qcache_node_create(data);
        if (q == NULL) {
            rs = DUER_ERR_MEMORY_OVERLOW;
            break;
        }

        if (p->_tail != NULL) {
            p->_tail->_next = q;
            p->_tail = q;
        } else {
            p->_head = p->_tail = q;
        }

        p->_length++;

        rs = DUER_OK;
    } while (0);

    return rs;
}

size_t duer_qcache_length(duer_qcache_handler cache)
{
    duer_qcache_t *p = (duer_qcache_t *)cache;
    return (p != NULL) ? p->_length : 0;
}

void *duer_qcache_top(duer_qcache_handler cache)
{
    duer_qcache_t *p = (duer_qcache_t *)cache;
    return p != NULL && p->_head != NULL ? p->_head->_data : NULL;
}


void *duer_qcache_pop(duer_qcache_handler cache)
{
    duer_qcache_t *p = (duer_qcache_t *)cache;
    duer_qcnode_t *q = NULL;
    void *rs = NULL;

    if (p && p->_head) {
        q = p->_head;
        p->_head = q->_next;
        rs = q->_data;
        if (--(p->_length) == 0) {
            p->_tail = NULL;
        }
        DUER_FREE(q);
    }

    return rs;
}

void duer_qcache_destroy(duer_qcache_handler cache)
{
    duer_qcache_t *p = (duer_qcache_t *)cache;
    duer_qcnode_t *q = p != NULL ? p->_head : NULL;
    duer_qcnode_t *r;

    while (q) {
        r = q;
        q = q->_next;
        DUER_FREE(r);
    }

    if (p) {
        DUER_MEMSET(p, 0, sizeof(duer_qcache_t));
        DUER_FREE(p);
    }
}
