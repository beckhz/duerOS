// Copyright (2017) Baidu Inc. All rights reserveed.
/*
 * File: lightduer_ota_updater.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Updater Head File
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UPDATER_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UPDATER_H

#include "lightduer_ota_unpack.h"
#include "lightduer_ota_downloader.h"

#define TRANSACTION_LEN   65
#define VERSION_LEN       16
#define URL_LEN           301
#define SIGNATURE_LEN     129

#ifdef __cplusplus
extern "C" {
#endif

typedef enum OTASwitch {
    ENABLE_OTA  =  1,
    DISABLE_OTA = -1,
} OTASwitch;

typedef enum OTAReboot {
    ENABLE_REBOOT  =  1,
    DISABLE_REBOOT = -1,
} OTAReboot;

struct OTAUpdateCommand {
    char transaction[TRANSACTION_LEN + 1];
    char version[VERSION_LEN + 1];
    char old_version[VERSION_LEN + 1];
    char url[URL_LEN + 1];
    char signature[SIGNATURE_LEN + 1];
    unsigned int size;
};

typedef struct OTAUpdater {
    int id;
    OTAUnpacker *unpacker;
    OTADownloader *downloader;
    struct OTAUpdateCommand *update_cmd;
    size_t received_data_size;
} OTAUpdater;

struct OTAInitOps {
/*
 * You need to call duer_ota_unpack_register_updater()
 * to implement struct IOtaUpdater in init_updater callback function
 * Call duer_ota_unpack_set_private_data() to set your privater data
 */
    int (*init_updater)(OTAUpdater *updater);
/*
 * You can free the resource which you create in init_updater callback function
 *
 */
    int (*uninit_updater)(OTAUpdater *updater);
/*
 * Implement reboot system function
 */
    int (*reboot)(void *arg);
};

typedef void (*OTA_Updater)(int arg, void *update_cmd);

/*
 * Call it to initialize the OTA module
 *
 * @param ops: You need to implement the structure OTAInitOps
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_init_ota(struct OTAInitOps *ops);

/*
 * User Call it to enable/disable OTA
 *
 * @param ops: You need to implement the structure OTAInitOps
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_set_switch(enum OTASwitch flag);

/*
 * Get the OTA status
 *
 * @param void:
 *
 * @return int: Enable   1
 *              Disable -1
 */
extern int duer_ota_get_switch(void);

/*
 * Set the OTA reboot status
 *
 * @param void:
 *
 * @return int: Enable   1
 *              Disable -1
 */
extern int duer_ota_set_reboot(enum OTAReboot reboot);

/*
 * get the OTA reboot status
 *
 * @param void:
 *
 * @return int: Enable   1
 *              Disable -1
 */
extern int duer_ota_get_reboot(void);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UPDATER_H
