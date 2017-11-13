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
// Description: The adapter for Memory Management.

#include "lightduer_memory.h"
#include "lightduer_lib.h"
#include "lightduer_log.h"
#include "baidu_json.h"

typedef struct _baidu_ca_memory_s {
    duer_context     context;
    duer_malloc_f    f_malloc;
    duer_realloc_f   f_realloc;
    duer_free_f      f_free;
#ifdef DUER_MEMORY_USAGE
    duer_size_t      alloc_counts;
    duer_size_t      alloc_size;
    duer_size_t      max_size;
#endif/*DUER_MEMORY_USAGE*/
} duer_memory_t;

DUER_LOC_IMPL duer_memory_t s_duer_memory = {NULL};

#ifdef DUER_MEMORY_USAGE

#define DUER_MEM_HDR_MASK        (0xFEDCBA98)

#define DUER_MEMORY_HEADER   \
    duer_u32_t   mask; \
    duer_size_t  size;

#define DUER_MEM_CONVERT(ptr)  (duer_memdbg_t *)(ptr ? ((char *)ptr - sizeof(duer_memhdr_t)) : NULL)

typedef struct _baidu_ca_memory_header_s {
    DUER_MEMORY_HEADER
} duer_memhdr_t;

typedef struct _baidu_ca_memory_debug_s {
    DUER_MEMORY_HEADER
    char        data[];
} duer_memdbg_t;

DUER_LOC_IMPL void* duer_memdbg_acquire(duer_memdbg_t* ptr, duer_size_t size) {
    if (ptr) {
        ptr->mask = DUER_MEM_HDR_MASK;
        ptr->size = size - sizeof(duer_memhdr_t);
        s_duer_memory.alloc_counts++;
        s_duer_memory.alloc_size += ptr->size;

        if (s_duer_memory.max_size < s_duer_memory.alloc_size) {
            s_duer_memory.max_size = s_duer_memory.alloc_size;
        }
    }

    return ptr ? ptr->data : NULL;
}

DUER_LOC_IMPL void duer_memdbg_release(duer_memdbg_t* p) {
    if (p) {
        if (p->mask != DUER_MEM_HDR_MASK) {
            DUER_LOGE("The memory <%x> has been trampled!!!", p->data);
        }

        s_duer_memory.alloc_counts--;
        s_duer_memory.alloc_size -= p->size;
    }
}

DUER_INT_IMPL void duer_memdbg_usage() {
    DUER_LOGI("duer_memdbg_usage: alloc_counts = %d, alloc_size = %d, max_size = %d",
             s_duer_memory.alloc_counts, s_duer_memory.alloc_size, s_duer_memory.max_size);
}

#endif/*DUER_MEMORY_USAGE*/

DUER_EXT_IMPL void baidu_ca_memory_init(duer_context context,
                                       duer_malloc_f f_malloc,
                                       duer_realloc_f f_realloc,
                                       duer_free_f f_free) {
    s_duer_memory.context = context;
    s_duer_memory.f_malloc = f_malloc;
    s_duer_memory.f_realloc = f_realloc;
    s_duer_memory.f_free = f_free;
    baidu_json_Hooks hooks = {
        duer_malloc,
        duer_free
    };
    baidu_json_InitHooks(&hooks);
}

DUER_INT_IMPL void* duer_malloc(duer_size_t size) {
    void* rs = NULL;

    if (s_duer_memory.f_malloc) {
#ifdef DUER_MEMORY_USAGE
        size += sizeof(duer_memhdr_t);
#endif
        rs = s_duer_memory.f_malloc(s_duer_memory.context, size);
#ifdef DUER_MEMORY_USAGE
        rs = duer_memdbg_acquire(rs, size);
#endif
    }

    return rs;
}

DUER_INT_IMPL void* duer_realloc(void* ptr, duer_size_t size) {
    void* rs = NULL;

    if (s_duer_memory.f_realloc || (s_duer_memory.f_malloc && s_duer_memory.f_free)) {
#ifdef DUER_MEMORY_USAGE
        ptr = DUER_MEM_CONVERT(ptr);
        size += sizeof(duer_memhdr_t);
        duer_memdbg_release(ptr);
#endif

        if (s_duer_memory.f_realloc) {
            rs = s_duer_memory.f_realloc(s_duer_memory.context, ptr, size);
        } else if (s_duer_memory.f_malloc && s_duer_memory.f_free) {
            rs = duer_malloc(size);

            if (rs) {
                DUER_MEMCPY(rs, ptr, size);
            }

            duer_free(ptr);
        } else {
            // do nothing
        }

#ifdef DUER_MEMORY_USAGE
        rs = duer_memdbg_acquire(rs, size);
#endif
    }

    return rs;
}

DUER_INT_IMPL void duer_free(void* ptr) {
    if (s_duer_memory.f_free) {
#ifdef DUER_MEMORY_USAGE
        ptr = DUER_MEM_CONVERT(ptr);
        duer_memdbg_release(ptr);
#endif
        s_duer_memory.f_free(s_duer_memory.context, ptr);
    }
}

DUER_INT_IMPL void* duer_malloc_ext(duer_size_t size, const char* file,
                                  duer_u32_t line) {
    void* rs = duer_malloc(size);
    DUER_LOGI("duer_malloc_ext: file:%s, line:%d, addr = %x, size = %d", file, line, rs, size);

    if (rs) {
        DUER_MEMSET(rs, 0, size);
    }

    return rs;
}

DUER_INT_IMPL void* duer_realloc_ext(void* ptr, duer_size_t size, const char* file,
                                   duer_u32_t line) {
    void* rs = duer_realloc(ptr, size);
    DUER_LOGI("duer_realloc_ext: file:%s, line:%d, new_addr = %x, old_addr = %x, size = %d",
                                    file, line, rs, ptr, size);
    return rs;
}

DUER_INT_IMPL void duer_free_ext(void* ptr, const char* file, duer_u32_t line) {
    DUER_LOGI("duer_free_ext: file:%s, line:%d, addr = %x", file, line, ptr);
    duer_free(ptr);
}
