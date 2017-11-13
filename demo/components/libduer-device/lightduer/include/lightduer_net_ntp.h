// Copyright 2017 Baidu Inc. All Rights Reserved.
// Author: Chang Li (changli@baidu.com)
//
// Description: NTP client API decleration.

#include <stdint.h>

#ifndef BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_IOT_NTP_H
#define BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_IOT_NTP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _DuerTime {
    uint32_t sec;
    uint32_t usec;
} DuerTime;

/**
 * @brief get NTP time
 * 
 * @param host: NTP server name. If host is NULL, use "s2c.time.edu.cn" by
 * default.
 *
 * @param timeout: timeout to acquire the NTP time, unit is micro seconds. If
 * it's 0, use 2000 ms by default. 
 *
 * @param result_time: the acquired NTP time.
 *
 * @return 0 is okay, netgative value is failed. 
 */
int duer_ntp_client(char* host, int timeout, DuerTime* result_time);

#ifdef __cplusplus
}
#endif

#endif // BAIDU_IOT_TINYDU_IOT_OS_SRC_IOT_BAIDU_CA_SOURCE_BAIDU_CA_IOT_NTP_H

