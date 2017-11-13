// Copyright (2017) Baidu Inc. All rights reserveed.
/*
 * File: lightduer_ota_http_downloader.h
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA HTTP Downloader Head File
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_HTTP_DOWNLOADER_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_HTTP_DOWNLOADER_H

#include "lightduer_ota_downloader.h"

/*
 * Initialise HTTP Downloader
 *
 * @param void:
 *
 * @return int: Success: DUER_OK
 *              Failed:  Other
 */
extern int duer_init_http_downloader(void);

#endif // BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_OTA_HTTP_DOWNLOADER_H
