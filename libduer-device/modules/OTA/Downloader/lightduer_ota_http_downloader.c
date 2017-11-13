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
 * File: lightduer_ota_http_downloader.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA HTTP Downloader
 */

#include "lightduer_ota_downloader.h"
#include <string.h>
#include <stdint.h>
#include "lightduer_log.h"
#include "lightduer_types.h"
#include "lightduer_mutex.h"
#include "lightduer_memory.h"
#include "lightduer_http_client.h"
#include "lightduer_http_client_ops.h"

struct HTTPPrivateData {
    data_handler handler;
    void *private_data;
    http_client_c *client;
};

static int get_http_data(
        void* p_user_ctx,
        e_data_pos pos,
        const char* buf,
        size_t len,
        const char* type)
{
    int ret = DUER_OK;
    data_handler handler;
    struct HTTPPrivateData *pdata = NULL;

    if (p_user_ctx != NULL) {
        pdata = (struct HTTPPrivateData *)p_user_ctx;
    } else {
        DUER_LOGE("HTTP Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    if (pdata->handler == NULL) {
        DUER_LOGE("HTTP Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }
    handler = pdata->handler;

    ret = (*handler)(pdata->private_data, buf, len);
out:
    return ret;
}

static int init_http_downloader(OTADownloader *downloader)
{
    int ret = DUER_OK;
    http_client_c *client = NULL;
    struct HTTPPrivateData *pdata = NULL;

    if (downloader == NULL) {
        DUER_LOGE("HTTP Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    client = duer_create_http_client();
    if (client == NULL) {
        DUER_LOGE("HTTP Downloader: Init HTTP failed");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    pdata = (struct HTTPPrivateData *)DUER_MALLOC(sizeof(*pdata));
    if (pdata == NULL) {
        DUER_LOGE("HTTP Downloader: Malloc failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto out;
    }

    pdata->client = client;
    downloader->private_data = (void *)pdata;
out:
    return ret;
}

static int connect_http_server(OTADownloader *downloader, const char *url)
{
    int ret = DUER_OK;
    e_http_result status;
    struct HTTPPrivateData *pdata = NULL;

    if (downloader->private_data != NULL) {
        pdata = (struct HTTPPrivateData *)downloader->private_data;
    } else {
        DUER_LOGE("HTTP Downloader: There is no HTTP client");

        ret = DUER_ERR_INVALID_PARAMETER;
    }

    status =  duer_http_client_get(pdata->client, url, 0);
    if (status != HTTP_OK) {
        DUER_LOGE("HTTP Downloader: Get URL failed");

        ret = DUER_ERR_FAILED;
    }

    return ret;
}

static int disconnect_http_server(OTADownloader *downloader)
{
    int ret = DUER_OK;

    if (downloader == NULL) {
        DUER_LOGE("HTTP Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;
    }

    return ret;
}

static int register_data_notify(OTADownloader *downloader, data_handler handler, void *private_data)
{
    int ret = DUER_OK;
    struct HTTPPrivateData *pdata = NULL;

    if (downloader == NULL || private_data == NULL || handler == NULL) {
        DUER_LOGE("HTTP Downloader: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    if (downloader->private_data != NULL) {
        pdata = (struct HTTPPrivateData *)downloader->private_data;
    } else {
        DUER_LOGE("HTTP Downloader: Uninitialized HTTP Downloader");

        ret = DUER_ERR_INVALID_PARAMETER;
    }

    pdata->handler = handler;
    pdata->private_data = private_data;

    duer_http_client_reg_data_hdlr_cb(pdata->client, get_http_data, pdata);
out:
    return ret;
}

static int destroy_http_downloader(OTADownloader *downloader)
{
    int ret = DUER_OK;
    http_client_c *client = NULL;
    struct HTTPPrivateData *pdata = NULL;

    if (downloader->private_data != NULL) {
        pdata = (struct HTTPPrivateData *)downloader->private_data;
    } else {
        DUER_LOGE("HTTP Downloader: There is no HTTP private data");

        ret = DUER_ERR_INVALID_PARAMETER;
    }

    duer_http_client_destroy(pdata->client);

    DUER_FREE(pdata);

    return ret;
}

static struct OTADownloader_ops ops = {
    .init                 = init_http_downloader,
    .connect_server       = connect_http_server,
    .disconnect_server    = disconnect_http_server,
    .register_data_notify = register_data_notify,
    .destroy              = destroy_http_downloader,
};

int duer_init_http_downloader(void)
{
    int ret = DUER_OK;
    OTADownloader *downloader = NULL;

    downloader = duer_ota_downloader_create_downloader();
     if (downloader == NULL) {
        DUER_LOGE("OTA: Create downloader failed");

        goto err;
    }

    ret = duer_ota_downloader_register_downloader_ops(downloader, &ops);
    if (ret != DUER_OK) {
        DUER_LOGE("HTTP Downloader: Register downloader ops failed ret:%d", ret);

        goto err;
    }

    ret = duer_ota_downloader_register_downloader(downloader, HTTP);
    if (ret != DUER_OK) {
        DUER_LOGE("HTTP Downloader: Register HTTP downloader ret:%d", ret);

        goto err;
    }

    return ret;
err:
    duer_ota_downloader_destroy_downloader(downloader);

    return ret;
}
