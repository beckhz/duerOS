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
//
// File: baidu_ca_mutex_adp.c
// Auth: Zhang Leliang (zhangleliang@baidu.com)
// Desc: Adapt the mutex function to linux.


#include "lightduer_mutex.h"

#include <pthread.h>

#include "lightduer_log.h"
#include "lightduer_memory.h"

duer_mutex_t create_mutex(void)
{
    duer_mutex_t mutex;
    pthread_mutex_t* p_mutex = (pthread_mutex_t*)DUER_MALLOC(sizeof(pthread_mutex_t));
    if (!p_mutex) {
        DUER_LOGW("malloc mutext fail!");
        return NULL;
    }
    int ret = pthread_mutex_init(p_mutex, NULL); // use the default attribute to initialize the mutex
    if (ret) {
        free(p_mutex);
        p_mutex = NULL;
        DUER_LOGW("pthread_mutex_init fail!, ret:%d", ret);
        return NULL;
    }
    mutex = (duer_mutex_t)p_mutex;

    return mutex;
}

duer_status_t lock_mutex(duer_mutex_t mutex)
{
    pthread_mutex_t* p_mutex;

    if (!mutex) {
        return DUER_ERR_FAILED;
    }

    p_mutex = (pthread_mutex_t*)mutex;

    int ret = pthread_mutex_lock(p_mutex);
    if(ret) {
        DUER_LOGI("pthread_mutex_lock fail!, ret:%d", ret);
        return DUER_ERR_FAILED;
    }
    return DUER_OK;
}

duer_status_t unlock_mutex(duer_mutex_t mutex)
{
    pthread_mutex_t* p_mutex;

    if (!mutex) {
        return DUER_ERR_FAILED;
    }

    p_mutex = (pthread_mutex_t*)mutex;

    int ret = pthread_mutex_unlock(p_mutex);
    if(ret) {
        DUER_LOGW("pthread_mutex_unlock fail!, ret:%d", ret);
        return DUER_ERR_FAILED;
    }
    return DUER_OK;
}

duer_status_t delete_mutex_lock(duer_mutex_t mutex)
{
    pthread_mutex_t* p_mutex;
    if (!mutex) {
        return DUER_ERR_FAILED;
    }

    p_mutex = (pthread_mutex_t*)mutex;
    pthread_mutex_destroy(p_mutex);
    free(p_mutex);

    return DUER_OK;
}
