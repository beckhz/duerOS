// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: duerapp_ota.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Demo
 *       This is a demo for ESPRESSIF platform
 */

#include <stdint.h>
#include <stdio.h>
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "IOtaUpdater.h"
#include "duerapp_config.h"
#include "lightduer_types.h"
#include "lightduer_ota_unpack.h"
#include "lightduer_ota_updater.h"
#include "lightduer_ota_notifier.h"

struct OTAInstallHandle{
    OTAUpdater *updater;
    esp_ota_handle_t update_handle;
    const esp_partition_t *update_partition;
};

static struct PackageInfo s_package_info = {
    .product = "ESP32 Demo",
    .batch   = "12",
    .os_info = {
        .os_name = "FreeRTOS",
        .developer = "Allen",
        .staged_version = "1.0.0.0",
    }
};

static int ota_notify_data_begin(void *ctx)
{
    esp_err_t err;
    int ret = DUER_OK;
    struct OTAInstallHandle *pdata = NULL;
    const esp_partition_t *update_partition = NULL;
    esp_ota_handle_t update_handle = 0 ;

    if (ctx == NULL) {
        DUER_LOGE("OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    pdata = (struct OTAInstallHandle *)ctx;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition != NULL) {
        DUER_LOGI("OTA Unpack OPS: Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    } else {
        DUER_LOGE("OTA Unpack OPS: Get update partition failed");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        DUER_LOGE("Init OTA failed, error = %d", err);

        ret = DUER_ERR_FAILED;

        goto out;
    }

    pdata->update_partition = update_partition;
    pdata->update_handle = update_handle;
out:
    return ret;
}

static int ota_notify_meta_data(void *cxt, struct package_meta_data *meta)
{
    if (meta == NULL) {
        DUER_LOGE("OTA Unpack OPS: Argument Error");

        return DUER_ERR_INVALID_PARAMETER;
    }
#if 1
    DUER_LOGI("OTA Unpack OPS: Basic info:");
    DUER_LOGI("OTA Unpack OPS: Package name: %s", meta->basic_info.package_name);
    DUER_LOGI("OTA Unpack OPS: Package type: %c", meta->basic_info.package_type);
    DUER_LOGI("OTA Unpack OPS: Package update: %c", meta->basic_info.package_update);

    DUER_LOGI("OTA Unpack OPS: Install info:");
    DUER_LOGI("OTA Unpack OPS: Install path: %s", meta->install_info.package_install_path);
    DUER_LOGI("OTA Unpack OPS: Module count: %d", meta->install_info.module_count);

    DUER_LOGI("OTA Unpack OPS: Update info:");
    DUER_LOGI("OTA Unpack OPS: Package version: %s", meta->update_info.package_version);

    DUER_LOGI("OTA Unpack OPS: Extension info:");
    DUER_LOGI("OTA Unpack OPS: Pair count: %d", meta->extension_info.pair_count);
#endif

    return DUER_OK;
}

static int ota_notify_module_data(
        void *cxt,
        unsigned int offset,
        unsigned char *data,
        unsigned int size)
{
    esp_err_t err;
    int ret = DUER_OK;
    esp_ota_handle_t update_handle = 0 ;
    struct OTAInstallHandle *pdata = NULL;

    if (cxt == NULL) {
        DUER_LOGE("OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    pdata = (struct OTAInstallHandle *)cxt;
    update_handle = pdata->update_handle;

    err = esp_ota_write(update_handle, (const void *)data, size);
    if (err != ESP_OK) {
        DUER_LOGE("OTA Unpack OPS: Write OTA data failed! err: 0x%x", err);

        ret = DUER_ERR_FAILED;
    }
out:
    return ret;
}

static int ota_notify_data_end(void *ctx)
{
    esp_err_t err;
    int ret = DUER_OK;
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    struct OTAInstallHandle *pdata = NULL;

    if (ctx == NULL) {
        DUER_LOGE("OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    pdata = (struct OTAInstallHandle *)ctx;
    update_handle = pdata->update_handle;
    update_partition = pdata->update_partition;

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        DUER_LOGE("OTA Unpack OPS: End OTA failed! err: %d", err);

        ret = DUER_ERR_FAILED;

        goto out;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        DUER_LOGE("OTA Unpack OPS: Set boot partition failed! err = 0x%x", err);

        ret = DUER_ERR_FAILED;

        goto out;
    }
out:
    return ret;
}

static int ota_update_img_begin(void *ctx)
{
    int ret = DUER_OK;

    DUER_LOGI("OTA Unpack OPS: update image begin");

    return ret;

}

static int ota_update_img(void *ctx)
{
    int ret = DUER_OK;

    DUER_LOGI("OTA Unpack OPS: updating image");

    return ret ;
}

static int ota_update_img_end(void *ctx)
{
    int ret = DUER_OK;

    DUER_LOGI("OTA Unpack OPS: update image end");

    return ret;
}

static struct IOtaUpdater updater = {
    .notify_data_begin  = ota_notify_data_begin,
    .notify_meta_data   = ota_notify_meta_data,
    .notify_module_data = ota_notify_module_data,
    .notify_data_end    = ota_notify_data_end,
    .update_img_begin   = ota_update_img_begin,
    .update_img         = ota_update_img,
    .update_img_end     = ota_update_img_end,
};

static int duer_ota_init_updater(OTAUpdater *ota_updater)
{
    int ret = DUER_OK;
    struct OTAInstallHandle *ota_install_handle = NULL;

    ota_install_handle = (struct OTAInstallHandle *)malloc(sizeof(*ota_install_handle));
    if (ota_install_handle == NULL) {

        DUER_LOGE("OTA Unpack OPS: Malloc failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto out;
    }

    ota_install_handle->updater = ota_updater;

    ret = duer_ota_unpack_register_updater(ota_updater->unpacker, &updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Unpack OPS: Register updater failed ret:%d", ret);

        goto out;
    }

    ret = duer_ota_unpack_set_private_data(ota_updater->unpacker, ota_install_handle);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Unpack OPS: Set private data failed ret:%d", ret);
    }
out:
    return ret;
}

static int duer_ota_uninit_updater(OTAUpdater *ota_updater)
{
    struct OTAInstallHandle *ota_install_handle = NULL;

    ota_install_handle = duer_ota_unpack_get_private_data(ota_updater->unpacker);
    if (ota_install_handle == NULL) {
        DUER_LOGE("OTA Unpack OPS: Get private data failed");

        return DUER_ERR_INVALID_PARAMETER;
    }

    free(ota_install_handle);

    return DUER_OK;
}

static int reboot(void *arg)
{
    int ret = DUER_OK;

    DUER_LOGE("OTA Unpack OPS: Prepare to restart system");

    esp_restart();

    return ret;
}

static int get_package_info(struct PackageInfo *info)
{
    int ret = DUER_OK;
    char firmware_version[FIRMWARE_VERSION_LEN + 1];

    if (info == NULL) {
        DUER_LOGE("Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    memset(firmware_version, 0, sizeof(firmware_version));

    ret = duer_get_firmware_version(firmware_version);
    if (ret != DUER_OK) {
        DUER_LOGE("Get firmware version failed");

        goto out;
    }

    strncpy((char *)&s_package_info.os_info.os_version,
            firmware_version,
            FIRMWARE_VERSION_LEN + 1);
    memcpy(info, &s_package_info, sizeof(*info));

out:
    return ret;
}

static struct PackageInfoOPS s_package_info_ops = {
    .get_package_info = get_package_info,
};

static struct OTAInitOps s_ota_init_ops = {
    .init_updater = duer_ota_init_updater,
    .uninit_updater = duer_ota_uninit_updater,
    .reboot = reboot,
};

int duer_initialize_ota(void)
{
    int ret = DUER_OK;

    ret = duer_init_ota(&s_ota_init_ops);
    if (ret != DUER_OK) {
        DUER_LOGE("Init OTA failed");
    }

    ret = duer_ota_register_package_info_ops(&s_package_info_ops);
    if (ret != DUER_OK) {
        DUER_LOGE("Register OTA package info ops failed");
    }

    return ret;
}
