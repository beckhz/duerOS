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
 * File: lightduer_ota_updater.c
 * Auth: Zhong Shuai (zhongshuai@baidu.com)
 * Desc: OTA Updater
 */

#include "lightduer_ota_updater.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "baidu_json.h"
#include "lightduer_log.h"
#include "lightduer_types.h"
#include "lightduer_mutex.h"
#include "lightduer_memory.h"
#include "lightduer_events.h"
#include "lightduer_connagent.h"
#include "lightduer_ota_unpack.h"
#include "lightduer_http_client.h"
#include "lightduer_ota_notifier.h"
#include "lightduer_ota_downloader.h"
#include "lightduer_ota_http_downloader.h"

#define STACK_SIZE    (6 * 1024)
#define QUEUE_LENGTH  (1)

static struct OTAInitOps *s_ota_init_ops = NULL;

/* May be we need to consider to support multi-updater
 * Use list or pointer to reduce the size of data section
 */
volatile static OTASwitch s_ota_switch = ENABLE_OTA;
volatile static OTASwitch s_ota_reboot = ENABLE_REBOOT;
volatile static duer_mutex_t s_lock_ota_updater = NULL;
volatile static duer_events_handler s_ota_updater_handler = NULL;

static int notify_package_info(OTAUpdater *updater)
{
    int ret = DUER_OK;
    char *report_info = NULL;
    struct PackageInfo package_info;
    baidu_json *data_json = NULL;
    baidu_json *os_info_json = NULL;
    baidu_json *package_info_json = NULL;

    memset(&package_info, 0, sizeof(package_info));

    ret = duer_ota_get_package_info(&package_info);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Notifier: Get package info failed");

        goto out;
    }

    package_info_json = baidu_json_CreateObject();
    if (package_info_json == NULL) {
        DUER_LOGE("OTA Notifier: Create json object failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto out;
    }

    os_info_json = baidu_json_CreateObject();
    if (os_info_json == NULL) {
        DUER_LOGE("OTA Notifier: Create json object failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto delete_package_json;
    }

    data_json = baidu_json_CreateObject();
    if (data_json == NULL) {
        DUER_LOGE("OTA Notifier: Create json object failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto delete_osinfo_json;
    }

    baidu_json_AddStringToObject(os_info_json, "name", (char *)&package_info.os_info.os_name);
    baidu_json_AddStringToObject(
            os_info_json,
            "developer",
            (char *)&package_info.os_info.developer);
    baidu_json_AddStringToObject(
            os_info_json,
            "staged_version",
            (char *)&package_info.os_info.staged_version);
    baidu_json_AddStringToObject(os_info_json, "version", (char *)&updater->update_cmd->version);
    baidu_json_AddItemToObject(package_info_json, "os", os_info_json);

    baidu_json_AddStringToObject(package_info_json, "version", OTA_PROTOCOL_VERSION);
    baidu_json_AddStringToObject(package_info_json, "product", (char *)&package_info.product);
    baidu_json_AddStringToObject(package_info_json, "batch", (char *)&package_info.batch);
    baidu_json_AddItemToObject(data_json, "package_info", package_info_json);

    ret = duer_data_report(data_json);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Notifier: Report package info failed ret: %d", ret);
    }
#if 0
    report_info = baidu_json_PrintUnformatted(data_json);
    if (report_info != NULL) {
        DUER_LOGE("OTA Notifier: Report data: %s", report_info);

        baidu_json_release(report_info);
    } else {
        DUER_LOGE("OTA Notifier: baidu_json_PrintUnformatted failed");
    }
#endif

    /*
     * Json object use linked list to link related json objects
     * and, it will delete all json objects which linked automatic
     * So, We do not need to delete them(os_info_json, package_info_json, data_json) one by one
     */
    baidu_json_Delete(data_json);

    return ret;
delete_osinfo_json:
    baidu_json_Delete(os_info_json);
delete_package_json:
    baidu_json_Delete(package_info_json);
out:
    return ret;
}

static int handler(void *private, const char *buf, size_t len)
{
    int ret = DUER_OK;
    char *data = NULL;
    static int count = 1;
    double progress = 0.0;
    double total_data_size = 0.0;
    double received_data_size = 0.0;
    OTAUpdater *updater = NULL;

    if (private == NULL) {
        DUER_LOGE("OTA Updater: Argument Error");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    if (buf != NULL && len > 0) {
        data = (char *)DUER_MALLOC(len);
        if (data == NULL) {
            DUER_LOGE("OTA Updater: Malloc failed");

            ret = DUER_ERR_FAILED;

            goto out;
        }

        memcpy(data, buf, len);

        updater = (OTAUpdater *)private;
        ret = duer_ota_unpack_decompress_data(updater->unpacker, data, len);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Updater: decompress data failed ret:%d", ret);
        }

        DUER_FREE(data);

        updater->received_data_size += len;
        total_data_size = updater->update_cmd->size;
        received_data_size = updater->received_data_size;
        progress = received_data_size / total_data_size;

        if (((int)(progress * 10) % 10) >= count) {
            count++;
            ret = duer_ota_notifier_event(updater, OTA_EVENT_DOWNLOADING, progress);
            if (ret != DUER_OK) {
                DUER_LOGE("OTA Updater: Notifier OTA download progress failed ret: %d", ret);
            }
        }
    } else {
        /*
         * Some Downloader will send a NULL pointer or 0 to indicate the
         * end of the transmission
         */
        DUER_LOGI("OTA Updater: HTTP Data length: %d, buf: %p", len, buf);
    }
out:
    return ret;
}

int duer_ota_destroy_updater(OTAUpdater *updater)
{
    int ret = DUER_OK;

    if (updater == NULL) {
        DUER_LOGE("OTA Updater: Argument Error");

        return DUER_ERR_INVALID_PARAMETER;
    }

    if (s_ota_init_ops->uninit_updater != NULL) {
        ret = s_ota_init_ops->uninit_updater(updater);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Updater: Uninit Updater failed ret: %d", ret);
        }
    }

    if (updater->unpacker != NULL) {
        DUER_LOGI("OTA Updater: Destroy unpacker");

        duer_ota_unpack_destroy_unpacker(updater->unpacker);
    }

    if (updater->downloader != NULL) {
        ret = duer_ota_downloader_destroy_downloader(updater->downloader);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Updater: Destroy downloader failed ret:%d", ret);

            goto out;
        }

        DUER_LOGI("OTA Updater: Destroy downloader");

        ret = duer_uninit_ota_downloader();
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Updater: Uninit OTA downloader failed ret: %d", ret);
        }

        DUER_LOGI("OTA Updater: Uninit OTA downloader");
    }

    if (updater->update_cmd != NULL) {
        DUER_FREE(updater->update_cmd);
    }

    DUER_FREE(updater);

    DUER_LOGI("OTA Updater: Destroy updater");
out:
    return ret;
}

// TBD
OTAUpdater *duer_ota_get_updater(int id)
{
    return NULL;
}

static int duer_ota_create_updater(OTA_Updater ota_updater, struct OTAUpdateCommand *update_cmd)
{
    int ret = DUER_OK;
    int tmp = DUER_OK;
    int id = 1; // ID for Updater (Unused)
    OTAUpdater *updater = NULL;
    struct OTAUpdateCommand *cmd = NULL;

    if (update_cmd == NULL) {
        DUER_LOGE("OTA Updater: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    // Do not support multi-updater
    if (s_ota_updater_handler == NULL) {
        s_ota_updater_handler = duer_events_create("lightduer_OTA_Updater", STACK_SIZE, QUEUE_LENGTH);
        if (s_ota_updater_handler == NULL) {
            DUER_LOGE("OTA Updater: Create OTA Updater failed");

            ret = DUER_ERR_FAILED;

            goto out;
        }
    }

    cmd = DUER_MALLOC(sizeof(*cmd));
    if (cmd == NULL) {
        DUER_LOGE("OTA Updater: Malloc cmd failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto out;
    }

    memmove(cmd, update_cmd, sizeof(*cmd));

    updater = DUER_MALLOC(sizeof(*updater));
    if (updater == NULL) {
        DUER_LOGE("OTA Updater: Malloc updater failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto free_cmd;
    }

    memset(updater, 0, sizeof(*updater));

    updater->update_cmd = cmd;

    ret = duer_events_call(s_ota_updater_handler, ota_updater, id, updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Call Updater failed");

        ret = DUER_ERR_FAILED;

        goto free_updater;
    }

    ret = duer_ota_notifier_event(updater, OTA_EVENT_BEGIN, 0);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Notifier OTA begin failed ret: %d", ret);

        ret = DUER_ERR_FAILED;

        goto free_updater;
    }

    return ret;

free_updater:
    tmp = duer_ota_notifier_event(updater, OTA_EVENT_REJECT, 0);
    if (tmp != DUER_OK) {
        DUER_LOGE("OTA Updater: Notifier OTA begin failed ret: %d", ret);
    }

    DUER_FREE(updater);
free_cmd:
    DUER_FREE(cmd);
out:
    duer_events_destroy(s_ota_updater_handler);

    return ret;
}

static enum DownloaderProtocol get_download_protocol(char *url, size_t len)
{
    /*
     * Now the Duer Cloud use HTTP protocol
     * So we only need to support it
     * Implement this function in order to expand
     */

    return HTTP;
}

static void ota_updater(int arg, void *ota_updater)
{
    int ret = DUER_OK;
    OTAEvent event;
    enum DownloaderProtocol dp;
    OTAUpdater *updater       = NULL;
    OTAUnpacker *unpacker     = NULL;
    OTADownloader *downloader = NULL;

    DUER_LOGI("OTA Updater: Start OTA Update");

    if (ota_updater == NULL) {
        DUER_LOGE("OTA Updater: Argument Error");

        goto out;
    }

    updater = (OTAUpdater *)ota_updater;

    unpacker = duer_ota_unpack_create_unpacker();
    if (unpacker == NULL) {
        DUER_LOGE("OTA Updater: Create unpacker failed");

        ret = DUER_ERR_FAILED;

        event = OTA_EVENT_REJECT;

        goto notify_ota_event;
    }
    updater->unpacker = unpacker;

    if (s_ota_init_ops->init_updater != NULL) {
        ret = s_ota_init_ops->init_updater(updater);
        if (ret != DUER_OK) {
            DUER_LOGE("OTA Updater: Init OTA Unpack ops failed ret: %d", ret);

            event = OTA_EVENT_REJECT;

            goto notify_ota_event;
        }
    } else {
        DUER_LOGE("OTA Updater: No Unpack OPS");

        ret = DUER_ERR_FAILED;

        event = OTA_EVENT_REJECT;

        goto notify_ota_event;
    }

    ret = duer_ota_unpack_decompress_data_begin(unpacker);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Decompress begin failed ret: %d", ret);

        event = OTA_EVENT_REJECT;

        goto notify_ota_event;
    }

    dp = get_download_protocol(updater->update_cmd->url, URL_LEN);

    downloader = duer_ota_downloader_get_downloader(dp);
    if (downloader == NULL) {
        DUER_LOGE("OTA Updater: Get OTA downloader failed ret: %d", ret);

        event = OTA_EVENT_REJECT;

        goto notify_ota_event;
    }
    updater->downloader = downloader;

    ret = duer_ota_downloader_init_downloader(downloader);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Init OTA HTTP downloader failed ret: %d", ret);

        event = OTA_EVENT_REJECT;

        goto notify_ota_event;
    }

    ret = duer_ota_downloader_register_data_notify(downloader, handler, updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Register data handler failed ret: %d", ret);

        event = OTA_EVENT_REJECT;

        goto notify_ota_event;
    }

    DUER_LOGI("OTA Updater: URL: %s", updater->update_cmd->url);

    ret = duer_ota_downloader_connect_server(downloader, updater->update_cmd->url);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Init OTA HTTP downloader failed ret: %d", ret);

        event = OTA_EVENT_CONNECT_FAIL;

        goto notify_ota_event;
    }

    ret = duer_ota_unpack_decompress_data_end(unpacker);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Decompress data end failed ret: %d", ret);

        event = OTA_EVENT_REJECT;

        goto notify_ota_event;
    }

    ret = duer_ota_notifier_event(updater, OTA_EVENT_DOWNLOAD_COMPLETE, 0);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Notifier OTA download complete failed ret: %d", ret);

        goto notify_ota_event;
    }

    ret = duer_ota_unpack_update_image_begin(unpacker);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Update image begin failed ret: %d", ret);

        event = OTA_EVENT_WRITE_ERROR;

        goto notify_ota_event;
    }

    ret = duer_ota_unpack_update_image(unpacker);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Update image failed ret: %d", ret);

        event = OTA_EVENT_WRITE_ERROR;

        goto notify_ota_event;
    }

    ret = duer_ota_unpack_update_image_end(unpacker);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Update image end failed ret: %d", ret);

        event = OTA_EVENT_WRITE_ERROR;

        goto notify_ota_event;
    }

    ret = duer_ota_notifier_event(updater, OTA_EVENT_INSTALLED, 0);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Notifier OTA install complete failed ret: %d", ret);

        goto notify_ota_event;
    }

    if (s_ota_init_ops->reboot != NULL) {
        if (s_ota_reboot == ENABLE_REBOOT) {
            ret = s_ota_init_ops->reboot(NULL);
            if (ret != DUER_OK) {
                DUER_LOGE("OTA Updater: Reboot failed ret: %d", ret);

                goto notify_ota_event;
            }
        } else {
            DUER_LOGE("OTA Updater: Disable Reboot");
        }
    } else {
        DUER_LOGE("OTA Updater: No Unpack OPS");

        ret = DUER_ERR_FAILED;
    }

    ret = notify_package_info(updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Notify package info failed ret: %d", ret);
    }

    goto destroy_updater;

notify_ota_event:
    ret = duer_ota_notifier_event(updater, event, 0);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Notifier OTA begin failed ret: %d", ret);
    }

destroy_updater:
    duer_ota_destroy_updater(updater);

out:
    duer_events_destroy(s_ota_updater_handler);
}

static int duer_analyze_update_cmd(const char *update_cmd, size_t cmd_len, struct OTAUpdateCommand *ota_update_cmd)
{
    int ret = DUER_OK;
    char *cmd_str = NULL;
    baidu_json *cmd = NULL;
    baidu_json *child = NULL;

    if (update_cmd == NULL || cmd_len <= 0) {
        DUER_LOGE("OTA Updater: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    cmd_str = (char *)DUER_MALLOC(cmd_len + 1);
    if (cmd_str == NULL) {
        DUER_LOGE("OTA Updater: Malloc failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto out;
    }

    memset(cmd_str, 0, cmd_len + 1);
    memmove(cmd_str, update_cmd, cmd_len);

    DUER_LOGI("OTA Updater: Update command: %s Length: %d", cmd_str, cmd_len);

    cmd = baidu_json_Parse(cmd_str);
    if (cmd == NULL) {
        DUER_LOGE("OTA Updater: Invalid command: %s", cmd);

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto free_cmd_string;
    }

    memset(ota_update_cmd, 0, sizeof(*ota_update_cmd));

    /*
     * This code is less ugly, but it is not security
     * The information of command will be cut off, if it is out of bounds
     * and there is no warning, I do not like it, but ...
     * We need to refactor the function later
     */

    child = cmd->child;
    child = child ? child->child : NULL;
    while (child) {
        if (strcmp(child->string, "transaction") == 0) {
            memmove(ota_update_cmd->transaction, child->valuestring, sizeof(ota_update_cmd->transaction) - 1);
            DUER_LOGI("Transaction = %s", ota_update_cmd->transaction);
        } else if (strcmp(child->string, "version") == 0) {
            memmove(ota_update_cmd->version, child->valuestring, sizeof(ota_update_cmd->version) - 1);
            DUER_LOGI("Version = %s", ota_update_cmd->version);
        } else if (strcmp(child->string, "old_version") == 0) {
            memmove(ota_update_cmd->old_version, child->valuestring, sizeof(ota_update_cmd->old_version) - 1);
            DUER_LOGI("Old Version = %s", ota_update_cmd->old_version);
        } else if (strcmp(child->string, "url") == 0) {
            memmove(ota_update_cmd->url, child->valuestring, sizeof(ota_update_cmd->url) - 1);
            DUER_LOGI("URL = %s", ota_update_cmd->url);
        } else if (strcmp(child->string, "size") == 0) {
            ota_update_cmd->size = child->valueint;
            DUER_LOGI("Size = %d", ota_update_cmd->size);
        } else if (strcmp(child->string, "signature") == 0) {
            memmove(ota_update_cmd->signature, child->valuestring, sizeof(ota_update_cmd->signature) - 1);
            DUER_LOGI("Signature = %s", ota_update_cmd->signature);
        } else {
            // DO nothing
        }

        child = child->next;
    }

delete_json:
    baidu_json_Delete(cmd);
free_cmd_string:
    DUER_FREE(cmd_str);
out:
    return ret;

    // This code is ugly, but it is security
#if 0
    result = baidu_json_GetObjectItem(cmd, "transaction");
    if (result == NULL) {
        DUER_LOGE("OTA Updater: Invalid command: %s", update_cmd);

        ret = DUER_ERR_FAILED;

        goto unlock_updater;
    } else {
        str_len = strlen(result->valuestring);
        if (str_len >= TRANSACTION_LEN) {
            DUER_LOGE("OTA Updater: Transaction string too long");

            ret = DUER_ERR_FAILED;

            goto unlock_updater;
        }

        strncpy(&ota_update_cmd->transaction, result->valuestring, str_len);
        ota_update_cmd->transaction[str_len + 1] = '\0';
    }

    result = baidu_json_GetObjectItem(cmd, "version");
    if (result == NULL) {
        DUER_LOGE("OTA Updater: Invalid command: %s", update_cmd);

        ret = DUER_ERR_FAILED;

        goto unlock_updater;
    } else {
        str_len = strlen(result->valuestring);
        if (str_len >= VERSION_LEN) {
            DUER_LOGE("OTA Updater: version string too long");

            ret = DUER_ERR_FAILED;

            goto unlock_updater;
        }

        strncpy(&ota_update_cmd->version, result->valuestring, str_len);
        ota_update_cmd->version[str_len + 1] = '\0';
    }

    result = baidu_json_GetObjectItem(cmd, "old_version");
    if (result == NULL) {
        DUER_LOGE("OTA Updater: Invalid command: %s", update_cmd);

        ret = DUER_ERR_FAILED;

        goto unlock_updater;
    } else {
        str_len = strlen(result->valuestring);
        if (str_len >= VERSION_LEN) {
            DUER_LOGE("OTA Updater: old version string too long");

            ret = DUER_ERR_FAILED;

            goto unlock_updater;
        }

        strncpy(&ota_update_cmd->old_version, result->valuestring, str_len);
        ota_update_cmd->old_version[str_len + 1] = '\0';
    }

    result = baidu_json_GetObjectItem(cmd, "url");
    if (result == NULL) {
        DUER_LOGE("OTA Updater: Invalid command: %s", update_cmd);

        ret = DUER_ERR_FAILED;

        goto unlock_updater;
    } else {
        str_len = strlen(result->valuestring);
        if (str_len >= URL_LEN) {
            DUER_LOGE("OTA Updater: URL string too long");

            ret = DUER_ERR_FAILED;

            goto unlock_updater;
        }

        strncpy(&ota_update_cmd->url, result->valuestring, str_len);
        ota_update_cmd->url[str_len + 1] = '\0';
    }

    result = baidu_json_GetObjectItem(cmd, "signature");
    if (result == NULL) {
        DUER_LOGE("OTA Updater: Invalid command: %s", update_cmd);

        ret = DUER_ERR_FAILED;

        goto unlock_updater;
    } else {
        str_len = strlen(result->valuestring);
        if (str_len >= SIGNATURE_LEN) {
            DUER_LOGE("OTA Updater: Transaction string too long");

            ret = DUER_ERR_FAILED;

            goto unlock_updater;
        }

        strncpy(&ota_update_cmd->signature, result->valuestring, str_len);
        ota_update_cmd->signature[str_len + 1] = '\0';
    }

    result = baidu_json_GetObjectItem(cmd, "size");
    if (result == NULL) {
        DUER_LOGE("OTA Updater: Invalid command: %s", update_cmd);

        ret = DUER_ERR_FAILED;

        goto unlock_updater;
    } else {
        ota_update_cmd->size = result->valueint;
    }

unlock_updater:
    ret = duer_mutex_unlock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        return DUER_ERR_FAILED;
    }

delete_json:
    baidu_json_Delete(cmd);

out:
    return ret;
#endif
}

static duer_status_t ota_update(duer_context ctx, duer_msg_t *msg, duer_addr_t *addr)
{
    int msg_code;
    duer_status_t ret = DUER_OK;
    struct OTAUpdateCommand ota_update_cmd;

    OTASwitch ota_switch = duer_ota_get_switch();
    if (ota_switch == DISABLE_OTA) {
        // May be We need to tell the Duer Cloud that user do not want to run OTA
        DUER_LOGI("OTA Updater: OTA upgrade is prohibited");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    if (msg == NULL || msg->payload == NULL || msg->payload_len <= 0) {
        DUER_LOGE("OTA Updater: Update command error");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    ret = duer_analyze_update_cmd(msg->payload, msg->payload_len, &ota_update_cmd);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Analyze update command failed");

        goto out;
    }

    ret = duer_ota_create_updater(ota_updater, &ota_update_cmd);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Create updater failed ret: %d", ret);
    }

out:
    msg_code = (ret == DUER_OK) ? DUER_MSG_RSP_CHANGED : DUER_MSG_RSP_BAD_REQUEST;

    duer_response(msg, msg_code, NULL, 0);

    return ret;
}

int duer_init_ota(struct OTAInitOps *ops)
{
    int ret = DUER_OK;

    if (ops == NULL
       || ops->init_updater == NULL
       || ops->uninit_updater == NULL
       || ops->reboot == NULL) {
        DUER_LOGE("OTA Updater: Argument Error");

        return DUER_ERR_INVALID_PARAMETER;
    }

    static bool first_time = true;

    if (first_time) {
        duer_res_t res[] = {
            {
                DUER_RES_MODE_DYNAMIC,
                DUER_RES_OP_PUT,
                "newfirmware",
                .res.f_res = ota_update
            },

        };

        duer_add_resources(res, sizeof(res) / sizeof(res[0]));
        first_time = false;
    } else {
        return ret;
    }

    if (s_lock_ota_updater == NULL) {
        s_lock_ota_updater = duer_mutex_create();
        if (s_lock_ota_updater == NULL) {
            DUER_LOGE("OTA Updater: Create mutex failed");

            return DUER_ERR_MEMORY_OVERLOW;
        }
    }

    s_ota_init_ops = ops;

    ret = duer_init_ota_downloader();
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Init OTA downloader failed ret: %d", ret);
    }

    ret = duer_init_http_downloader();
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Init OTA HTTP downloader failed ret: %d", ret);
    }

    return ret;
}

int duer_ota_set_switch(enum OTASwitch flag)
{
    int ret = DUER_OK;

    if (flag != ENABLE_OTA || flag != DISABLE_OTA) {
        DUER_LOGE("OTA Updater: Argument Error");

        return DUER_ERR_INVALID_PARAMETER;
    }

    if (s_lock_ota_updater == NULL) {
        DUER_LOGE("OTA Updater: Uninit mutex");

        return DUER_ERR_FAILED;
    }

    ret = duer_mutex_lock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        goto out;
    }

    s_ota_switch = flag;

    ret = duer_mutex_unlock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        goto out;
    }
out:
    return ret;
}

int duer_ota_get_switch(void)
{
    int ret;

    if (s_lock_ota_updater == NULL) {
        DUER_LOGE("OTA Updater: Uninit mutex");

        return DUER_ERR_FAILED;
    }

    ret = duer_mutex_lock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        return DUER_ERR_FAILED;
    }

    ret = s_ota_switch;

    ret = duer_mutex_unlock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        return DUER_ERR_FAILED;
    }

    return ret;
}

int duer_ota_set_reboot(enum OTAReboot reboot)
{
    int ret = DUER_OK;

    if (reboot != ENABLE_REBOOT && reboot != DISABLE_REBOOT) {
        DUER_LOGE("OTA Updater: Argument Error");

        return DUER_ERR_INVALID_PARAMETER;
    }

    if (s_lock_ota_updater == NULL) {
        DUER_LOGE("OTA Updater: Uninit mutex");

        return DUER_ERR_FAILED;
    }

    ret = duer_mutex_lock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        goto out;
    }

    s_ota_reboot = reboot;

    ret = duer_mutex_unlock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        goto out;
    }
out:
    return ret;
}

int duer_ota_get_reboot(void)
{
    int ret;

    if (s_lock_ota_updater == NULL) {
        DUER_LOGE("OTA Updater: Uninit mutex");

        return DUER_ERR_FAILED;
    }

    ret = duer_mutex_lock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        return DUER_ERR_FAILED;
    }

    ret = s_ota_reboot;

    ret = duer_mutex_unlock(s_lock_ota_updater);
    if (ret != DUER_OK) {
        DUER_LOGE("OTA Updater: Lock ota switch failed");

        return DUER_ERR_FAILED;
    }

    return ret;
}
