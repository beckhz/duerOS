// Copyright (2017) Baidu Inc. All rights reserveed.
/*
 * File: lightduer_dev_info.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: Device information
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DEV_INFO_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DEV_INFO_H

#ifdef __cplusplus
extern "C" {
#endif

#define FIRMWARE_VERSION_LEN   16
#define SDK_VERSION_LEN        15
#define CHIP_VERSION_LEN       20
#define WIFI_SSID_LEN          20
#define MAC_ADDRESS_LEN        18

enum NetworkType {
    WIFI      = 1,
    ETHERNET  = 2,
    BLUETOOTH = 3,
    USB       = 4,
};

struct NetworkInfo {
    enum NetworkType network_type;
    char mac_address[MAC_ADDRESS_LEN + 1];
    char wifi_ssid[WIFI_SSID_LEN + 1];
};

struct DevInfo {
    char firmware_version[FIRMWARE_VERSION_LEN + 1];
    char chip_version[CHIP_VERSION_LEN + 1];
    char sdk_version[SDK_VERSION_LEN + 1];
    struct NetworkInfo networkinfo;
};

struct DevInfoOps {
/*
 * Provide firmware information about the current system
 */
    int (*get_firmware_version)(char *firmware_version);
/*
 * Provide chip version about the current system
 */
    int (*get_chip_version)(char *chip_version);
/*
 * Provide sdk version about the current system
 */
    int (*get_sdk_version)(char *sdk_version);
/*
 * Provide newwork information about the current system
 */
    int (*get_network_info)(struct NetworkInfo *info);
};

/*
 * Get the firmware version of the current system
 *
 * @param firmware_version: size > FIRMWARE_VERSION_LEN
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_get_firmware_version(char *firmware_version);

/*
 * Get the chip version of the current system
 *
 * @param chip_version: size > CHIP_VERSION_LEN
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_get_chip_version(char *chip_version);

/*
 * Get the SDK version of the current system
 *
 * @param sdk_version: size > SDK_VERSION_LEN
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_get_sdk_version(char *sdk_version);

/*
 * Get the newwork info of the current system
 *
 * @param info:
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_get_network_info(struct NetworkInfo *info);

/*
 * Report device info to Duer Cloud
 *
 * @param info:
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_report_device_info(void);

/*
 * Register device info ops
 *
 * @param ops:
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_register_device_info_ops(struct DevInfoOps *ops);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DEV_INFO_H
