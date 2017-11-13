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
 * File: lightduer_dcs_alert.h
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Light duer alert APIs.
 */

#ifndef BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DCS_ALERT_H
#define BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DCS_ALERT_H

#include "baidu_json.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SET_ALERT_SUCCESS,
    SET_ALERT_FAIL,
    DELETE_ALERT_SUCCESS,
    DELETE_ALERT_FAIL,
    ALERT_START,
    ALERT_STOP,
} duer_alert_event_type;

/**
 * DESC:
 * Init alert.
 *
 * PARAM: none
 *
 * @RETURN: none.
 */
void duer_dcs_alert_init(void);

/**
 * DESC:
 * Report alert event
 *
 * PARAM:
 * @param[in] token: the token of the alert.
 * @param[in] type: event type, refer to duer_alert_event_type.
 *
 * @return: 0 when success, negative value when failed.
 */
int duer_dcs_report_alert_event(const char *token, duer_alert_event_type type);

/**
 * DESC:
 * Developer needs to implement this interface to set alert.
 *
 * PARAM:
 * @param[in] token: the token of the alert, it's the ID of the alert.
 * @param[in] time: the schedule time of the alert, ISO 8601 format.
 * @param[in] type: the type of the alert, TIMER or ALARM.
 *
 * @RETURN: none.
 */
void duer_dcs_alert_set_handler(const char *token, const char *time, const char *type);

/**
 * DESC:
 * Developer needs to implement this interface to delete alert.
 *
 * PARAM:
 * @param[in] token: the token of the alert need to delete.
 *
 * @RETURN: none.
 */
void duer_dcs_alert_delete_handler(const char *token);

/**
 * DESC:
 * Report alert info.
 * This API could be used in duer_dcs_get_all_alert, so DCS can get all alerts info.
 *
 * PARAM:
 * @param[out] alert_array: used to store the alert info.
 * @param[in] token: the token of the alert.
 * @param[in] type: the type of the alert, TIMER or ALARM.
 * @param[in] time: the schedule time of the alert.
 *
 * @RETURN: none.
 */
void duer_dcs_report_alert(baidu_json *alert_array,
                           const char *token,
                           const char *type,
                           const char *time);

/**
 * DESC:
 * It's used to get all alert info by DCS.
 * Developer can implement this interface by call duer_dcs_report_alert to report all alert info.
 *
 * PARAM:
 * @param[out] alert_array: a json array used to store all alert info.
 *
 * @RETURN: none.
 */
void duer_dcs_get_all_alert(baidu_json *alert_array);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_INCLUDE_LIGHTDUER_DCS_ALERT_H*/

