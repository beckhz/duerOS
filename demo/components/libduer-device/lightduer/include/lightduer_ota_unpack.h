// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: lightduer_ota_unpack.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Unpack Head File
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UNPACK_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UNPACK_H

#include <stdint.h>
#include <stddef.h>
#include "lightduer_mutex.h"
#include "IOtaUpdater.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OTAUnpacker {
    void *private_data;              // Which you want to pass to callback functions
    void *verification;
    void *decompress;
    struct IOtaUpdater *updater;
    duer_mutex_t lock;
} OTAUnpacker;

/*
 * Create a unpacker
 * Now it can supports zliblite to adaptive to Duer Cloud
 * We can refactor it to support multi-decompressor
 *
 * @param void:
 *
 * @return  OTAUnpacker *:  Success: Other
 *                          Failed:  NULL
 */
extern OTAUnpacker *duer_ota_unpack_create_unpacker(void);

/*
 * Destroy a unpacker
 *
 * @param unpacker: OTAUnpacker object
 *
 * @return void:
 */
extern void duer_ota_unpack_destroy_unpacker(OTAUnpacker *unpacker);

/*
 * Register a callback function to a unpacker, to receive uncompressed data .....
 *
 * @param unpacker: OTAUnpacker object
 *
 * @param updater: update operations
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_register_updater(OTAUnpacker *unpacker, struct IOtaUpdater *updater);

/*
 * Get private data
 *
 * @param unpacker: OTAUnpacker object
 *
 * @return void *: Success: Other
 *                 Failed:  NULL
 */
extern void *duer_ota_unpack_get_private_data(OTAUnpacker *unpacker);

/*
 * Set private data
 *
 * @param unpacker: OTAUnpacker object
 *
 * @param private:  Data which you wang to pass
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_set_private_data(OTAUnpacker *unpacker, void *private_data);

/*
 * Call it before you decompress data
 *
 * @param unpacker: OTAUnpacker object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_decompress_data_begin(OTAUnpacker *unpacker);

/*
 * Pass the compressed data to it, it will decompress the data.
 * and send the decompressed data to you
 *
 * @param unpacker: OTAUnpacker object
 *
 * @param data: compressed data
 *
 * @param size: the size of data
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_decompress_data(OTAUnpacker *unpacker, unsigned char *data, size_t size);

/*
 * Call it when you finish decompressing the data
 *
 * @param unpacker: OTAUnpacker object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_decompress_data_end(OTAUnpacker *unpacker);

/*
 * Call it before you update firmware
 *
 * @param unpacker: OTAUnpacker object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_update_image_begin(OTAUnpacker *unpacker);

/*
 * Call it when you want to update firmware
 *
 * @param unpacker: OTAUnpacker object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_update_image(OTAUnpacker *unpacker);

/*
 * Call it after you finished updating firmware
 *
 * @param unpacker: OTAUnpacker object
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_ota_unpack_update_image_end(OTAUnpacker *unpacker);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_UNPACK_H
