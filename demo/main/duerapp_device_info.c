// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: duerapp_device_info.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: Device Information
 */

#include <stdint.h>
#include <stdio.h>
#include "duerapp_config.h"
#include "lightduer_types.h"
#include "lightduer_dev_info.h"

#define FIRMWARE_VERSION "1.0.0.0"
#define CHIP_VERSION     "esp32"
#define SDK_VERSION      "1.0"

static int get_firmware_version(char *firmware_version)
{

    strncpy(firmware_version, FIRMWARE_VERSION, FIRMWARE_VERSION_LEN);

    return DUER_OK;
}

static int get_chip_version(char *chip_version)
{
    strncpy(chip_version, CHIP_VERSION, CHIP_VERSION_LEN);

    return DUER_OK;
}

static int get_sdk_version(char *sdk_version)
{
    strncpy(sdk_version, SDK_VERSION, SDK_VERSION_LEN);

    return DUER_OK;
}

static int get_network_info(struct NetworkInfo *info)
{
    info->network_type = WIFI;

    // Just demo
    strncpy(info->mac_address, "00:0c:29:f6:c8:24", MAC_ADDRESS_LEN);
    strncpy(info->wifi_ssid, "test", WIFI_SSID_LEN);

    return DUER_OK;
}

static struct DevInfoOps dev_info_ops = {
    .get_firmware_version = get_firmware_version,
    .get_chip_version     = get_chip_version,
    .get_sdk_version      = get_sdk_version,
    .get_network_info     = get_network_info,
};

int duer_init_device_info(void)
{
    int ret = DUER_OK;

    ret = duer_register_device_info_ops(&dev_info_ops);
    if (ret != DUER_OK) {
        DUER_LOGE("Dev Info: Register dev ops failed");
    }

    return ret;
}
