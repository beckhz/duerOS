// Copyright (2017) Baidu Inc. All rights reserved.
/**
 * File: duerapp.c
 * Auth: Su Hao(suhao@baidu.com)
 * Desc: Duer Application Main.
 */

//esp32上运行，可以实现将本地请求语音上传，并接收云端下发的url。

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lightduer.h"
#include "lightduer_voice.h"
#include "lightduer_net_ntp.h"

#include "duerapp_config.h"
#include "duerapp_key.h"
#include "lightduer_key.h"
#include "lightduer_play_event.h"
#include "driver/gpio.h"
#include "duerapp_alarm.h"
#include "duerapp_ota.h"
#include "duerapp_device_info.h"
#include "lightduer_dev_info.h"
#include "lightduer_ota_notifier.h"
#include "lightduer_dcs.h"
#include "duerapp_dcs.h"

#define PROFILE_PATH    SDCARD_PATH"/profile"

extern TaskHandle_t play_media_task_handle;
extern QueueHandle_t voice_result_queue;
extern int duer_init_play(void);


TaskHandle_t duer_test_task_handle = NULL;

static void duer_test_task(void *pvParameters);

static void duer_record_event_callback(int event, const void *data, size_t param)
{
    switch (event) {
        case REC_START:
        {
            DUER_LOGD("start send voice: %s", (const char *)data);
            baidu_json *path = baidu_json_CreateObject();
            baidu_json_AddStringToObject(path, "path", (const char *)data);
            duer_data_report(path);
            baidu_json_Delete(path);      
            duer_voice_start(param);

            duer_dcs_on_listen_started();   //dcs listen_start
            break;
        }
        case REC_DATA:
            duer_voice_send(data, param);
            break;
        case REC_STOP:
            DUER_LOGD("stop send voice: %s", (const char *)data);
            duer_voice_stop();
            break;
        default:
            DUER_LOGE("event not supported: %d!", event);
            break;
    }
}

static void duer_event_hook(duer_event_t *event)
{
    if (!event) {
        DUER_LOGE("NULL event!!!");
    }

    DUER_LOGI("event: %d", event->_event);

    switch (event->_event) {
    case DUER_EVENT_STARTED:

        // Initialize the DCS API
        duer_dcs_init();

        xTaskCreate(&duer_test_task, "duer_test_task", 1024 * 8, NULL, 5, &duer_test_task_handle);

        break;
    case DUER_EVENT_STOPPED:
        if (duer_test_task_handle) {
            vTaskDelete(duer_test_task_handle);
            duer_test_task_handle = NULL;
        }
        break;
    }
}

static void duer_voice_result(struct baidu_json *result)
{
    BaseType_t ret;
    baidu_json *voice_result = NULL;

    voice_result = baidu_json_Duplicate(result, 1);
    if (voice_result == NULL) {
        DUER_LOGE("Duplication voice result failed");

        return;
    }

    duer_set_play_source(PLAY_NET_FILE);

    ret = xQueueSendToFront(voice_result_queue, voice_result, 0);
    if (ret != pdTRUE) {
        DUER_LOGE("Send voice result failed");

        baidu_json_Delete(voice_result);
    }
}

void duer_test_task(void *pvParameters)
{
    int ret;

    (void)pvParameters;

    duer_record_set_event_callback(duer_record_event_callback);

    ret = duer_report_device_info();
    if (ret != DUER_OK) {
        DUER_LOGE("Report device info failed ret:%d", ret);
    }

    ret = duer_ota_notify_package_info();
    if (ret != DUER_OK) {
        DUER_LOGE("Report package info failed ret:%d", ret);
    }

    while (1) {
        duer_record_all();

        vTaskSuspend(NULL);
    }
}

// It called in duerapp_wifi_config.c, when the Wi-Fi is connected.
void duer_test_start()
{

    const char *data = duer_load_profile(PROFILE_PATH);
    if (data == NULL) {
        DUER_LOGE("load profile failed!");
        return;
    }

    DUER_LOGD("profile: \n%s", data);

    // We start the duer with duer_start(), when network connected.
    duer_start(data, strlen(data));

    free((void *)data);
}

static TaskHandle_t xNtpHandle = NULL;

void ntp_task() {
    DuerTime time;
    int ret = -1;

    vTaskDelay(5000 / portTICK_PERIOD_MS); // wait network ready

    ret = duer_ntp_client(NULL, 0, &time);

    while (ret < 0) {
        ret = duer_ntp_client(NULL, 0, &time);
    }

    if (xNtpHandle) {
        vTaskDelete(xNtpHandle);
        xNtpHandle = NULL;
    }
}

static void get_ntp_time() {
    xTaskCreate(&ntp_task, "ntp_task", 4096, NULL, 5, &xNtpHandle);
}

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );

    initialize_sdcard();

    initialize_gpio();
    
    // get NTP time
    //get_ntp_time();

    // Initialize the Duer OS
    duer_initialize();

    // Initialize the alarm
    //duer_init_alarm();
    
    // Set the Duer Event Callback
    duer_set_event_callback(duer_event_hook);

    // Set the voice interaction result
    duer_voice_set_result_callback(duer_voice_result);

    // Initialize play
    duer_init_play();

    initialize_wifi();

    duer_initialize_ota();

    duer_init_device_info();
      
}