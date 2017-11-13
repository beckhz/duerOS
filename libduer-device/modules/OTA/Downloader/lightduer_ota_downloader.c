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
/*
 * File: lightduer_ota_downloader.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Downloader
 */

#include "lightduer_ota_downloader.h"
#include <string.h>
#include <stdint.h>
#include "lightduer_log.h"
#include "lightduer_types.h"
#include "lightduer_mutex.h"
#include "lightduer_memory.h"

/*
 * Now we use array, I want to use list to reduce the size of data section
 * and support limitless downloader.(TBD)
 */
static OTADownloader *s_ota_downloader[MAX_PROTOCOL_COUNT];
volatile static duer_mutex_t s_ota_downloader_lock;

int duer_init_ota_downloader(void)
{
    int i = 0;
    int ret = DUER_OK;

#if 0
    memset(s_ota_downloader, 0, MAX_PROTOCOL_COUNT);
#else
    for (i = 0; i < MAX_PROTOCOL_COUNT; i++) {
        s_ota_downloader[i] = NULL;
    }
#endif
    if (s_ota_downloader_lock == NULL) {
        s_ota_downloader_lock = duer_mutex_create();
        if (s_ota_downloader_lock == NULL) {
            DUER_LOGE("OTA Downloader: Init OTA Downloader Module failed");

            ret = DUER_ERR_FAILED;
        }
    }

    return ret;
}

int duer_uninit_ota_downloader(void)
{
    int ret = DUER_OK;

    ret = duer_mutex_destroy(s_ota_downloader_lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Uninit OTA Downloader Module failed");

        ret = DUER_ERR_FAILED;
    }

    return ret;
}

int duer_ota_downloader_register_downloader(OTADownloader *downloader, enum DownloaderProtocol dp)
{
    int ret = DUER_OK;

    if (downloader == NULL || dp < HTTP || dp >= MAX_PROTOCOL_COUNT) {
        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    ret = duer_mutex_lock(s_ota_downloader_lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Lock OTA downloader failed");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    s_ota_downloader[dp] = downloader;
    downloader->dp = dp;

    ret = duer_mutex_unlock(s_ota_downloader_lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Unlock OTA downloader failed");

        ret = DUER_ERR_FAILED;

        goto out;
    }
out:
    return ret;
}

int duer_ota_downloader_unregister_downloader(enum DownloaderProtocol dp)
{
    int ret = DUER_OK;

    if (dp < HTTP || dp >= MAX_PROTOCOL_COUNT) {
        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    ret = duer_mutex_lock(s_ota_downloader_lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Lock OTA downloader failed");

        goto out;
    }

    /*
     * May be you will think that we need to free the "Downloader", if not memory leak
     * I do not think this is a good time to free it
     * so the user have to call duer_ota_downloader_destroy_downloader() by themself to free
     * "Downloader"
     */
    s_ota_downloader[dp] = NULL;

    ret = duer_mutex_unlock(s_ota_downloader_lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Unlock OTA downloader failed");

        goto out;
    }
out:
    return ret;
}

OTADownloader *duer_ota_downloader_get_downloader(enum DownloaderProtocol dp)
{
    int ret = DUER_OK;
    OTADownloader *downloader = NULL;

    if (dp < HTTP || dp >= MAX_PROTOCOL_COUNT) {
        DUER_LOGE("OTA Downloader: Argument Error");

        goto err;
    }

    if (s_ota_downloader[dp] == NULL) {
        DUER_LOGE("OTA Downloader: Do not support protocol");

        goto err;
    }

    ret = duer_mutex_lock(s_ota_downloader_lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Obtain OTA downloader lock failed");

        goto err;
    }

    downloader = s_ota_downloader[dp];

    ret = duer_mutex_unlock(s_ota_downloader_lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Unlock OTA lock failed");

        downloader = NULL;
    }
err:
    return downloader;
}

OTADownloader *duer_ota_downloader_create_downloader(void)
{
    OTADownloader *downloader = NULL;

    downloader = (OTADownloader *)DUER_MALLOC(sizeof(*downloader));
    if (downloader == NULL) {
        DUER_LOGE("OTA Downloader: Create downloader failed");
    }

    memset(downloader, 0, sizeof(*downloader));

    downloader->lock = duer_mutex_create();
    if (downloader->lock == NULL) {
        DUER_LOGE("OTA Downloader: Create lock failed");

        goto err;
    }

    return downloader;
err:
    DUER_FREE(downloader);

    return NULL;
}

int duer_ota_downloader_destroy_downloader(OTADownloader *downloader)
{
    int flag = 1;
    int ret = DUER_OK;

    if (downloader == NULL) {

        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    if (downloader->ops != NULL && downloader->ops->destroy != NULL) {
        ret = duer_mutex_lock(downloader->lock);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Downloader: Can not lock downloader");

            goto out;
        }

        ret = downloader->ops->destroy(downloader);
        if (ret != DUER_OK) {
            flag = -1;
            DUER_LOGE("OTA Downloader: Destroy downloader failed");
        }

        ret = duer_mutex_unlock(downloader->lock);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Downloader: Can not unlock downloader");

            goto out;
        }
    }

    if (flag < 0) {
        goto out;
    }

    ret = duer_ota_downloader_unregister_downloader(downloader->dp);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Unregister downloader failed");

        goto out;
    }

    ret = duer_mutex_destroy(downloader->lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Destroy downloader failed");
    }

    DUER_FREE(downloader);
out:
    return ret;
}

int duer_ota_downloader_register_downloader_ops(
        OTADownloader *downloader,
        struct OTADownloader_ops *downloader_ops)
{
    int ret = DUER_OK;

    if (downloader == NULL
        || downloader_ops == NULL
        || downloader_ops->init == NULL
        || downloader_ops->connect_server == NULL
        || downloader_ops->disconnect_server == NULL
        || downloader_ops->register_data_notify == NULL
        || downloader_ops->destroy == NULL) {

        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    ret = duer_mutex_lock(downloader->lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not obtain lock");

        goto out;
    }

    downloader->ops = downloader_ops;

    ret = duer_mutex_unlock(downloader->lock);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not unlock");

        goto out;
    }
out:
    return ret;
}

int duer_ota_downloader_init_downloader(OTADownloader *downloader)
{
    int ret = DUER_OK;
    int lock_ret = DUER_OK;

    if (downloader == NULL
        || downloader->ops == NULL
        || downloader->ops->init == NULL) {

        DUER_LOGE("OTA Downloader: Argument error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto err;
    }

    lock_ret = duer_mutex_lock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not obtain lock");

        ret = lock_ret;

        goto err;
    }

    ret = downloader->ops->init(downloader);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Init downloader failed");
    }

    lock_ret = duer_mutex_unlock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not unlock");

        ret = lock_ret;
    }
err:
    return ret;
}

int duer_ota_downloader_connect_server(OTADownloader *downloader, const char *url)
{
    int ret = DUER_OK;
    int lock_ret = DUER_OK;
    int url_len = 0;

    if (downloader == NULL
        || url == NULL
        || downloader->ops == NULL
        || downloader->ops->connect_server == NULL) {

        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto err;
    }

    url_len = strlen(url);
    if (url_len >= URL_LEN) {
        DUER_LOGE("OTA Downloader: URL too long");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto err;
    }
    memcpy(downloader->url, url, url_len);
    downloader->url[url_len + 1] = '\0';

    lock_ret = duer_mutex_lock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not acquire lock");

        ret = lock_ret;

        goto err;
    }

    ret = downloader->ops->connect_server(downloader, url);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Connect server failed");
    }

    lock_ret = duer_mutex_unlock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not unlock");

        ret = lock_ret;
    }
err:
    return ret;
}

int duer_ota_downloader_disconnect_server(OTADownloader *downloader)
{
    int ret = DUER_OK;
    int lock_ret = DUER_OK;

    if (downloader == NULL
        || downloader->ops == NULL
        || downloader->ops->disconnect_server == NULL) {

        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto err;
    }

    lock_ret = duer_mutex_lock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not acquire lock");

        ret = lock_ret;

        goto err;
    }

    ret = downloader->ops->disconnect_server(downloader);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Disconnect server failed");
    }

    lock_ret = duer_mutex_unlock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not unlock");

        ret = lock_ret;
    }
err:
    return ret;
}

int duer_ota_downloader_register_data_notify(
        OTADownloader *downloader,
        data_handler handler,
        void *private_data)
{
    int ret;
    int lock_ret = DUER_OK;

    if (downloader == NULL
        || downloader->ops == NULL
        || downloader->ops->register_data_notify == NULL) {

        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto err;
    }

    lock_ret = duer_mutex_lock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not obtain lock");

        ret = lock_ret;

        goto err;
    }

    ret = downloader->ops->register_data_notify(
            downloader,
            handler,
            private_data);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Register data notify failed");
    }

    lock_ret = duer_mutex_unlock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not unlock");

        ret = lock_ret;
    }
err:
    return ret;
}

int duer_ota_downloader_set_private_data(OTADownloader *downloader, void *private_data)
{
    int ret = DUER_OK;
    int lock_ret = DUER_OK;

    if (downloader == NULL || private_data == NULL) {

        DUER_LOGE("OTA Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto err;
    }

    lock_ret = duer_mutex_lock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not acquire lock");

        ret = lock_ret;

        goto err;
    }

    downloader->private_data = private_data;

    lock_ret = duer_mutex_unlock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not unlock");

        ret = lock_ret;
    }
err:
    return ret;
}

void *duer_ota_downloader_get_private_data(OTADownloader *downloader)
{
    int lock_ret = DUER_OK;
    void *data = NULL;

    if (downloader == NULL) {

        DUER_LOGE("OTA Downloader: Argument Error");

        goto err;
    }

    lock_ret = duer_mutex_lock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not acquire lock");

        goto err;
    }

    data = downloader->private_data;

    lock_ret = duer_mutex_unlock(downloader->lock);
    if (lock_ret != DUER_OK) {
        DUER_LOGE("OTA Downloader: Can not unlock");

        data = NULL;
    }
err:
    return data;
}
