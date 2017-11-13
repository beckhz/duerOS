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
 * File: duerapp.c
 * Auth: Zhang Leliang(zhanglelaing@baidu.com)
 * Desc: Duer Application Main.
 */

#include <string.h>
#include <stdbool.h>

#include "lightduer.h"
#include "lightduer_voice.h"
#include "lightduer_net_ntp.h"
#include "lightduer_memory.h"
#include "lightduer_log.h"

#include "duerapp_args.h"
#include "duerapp_config.h"
#include "lightduer_key.h"

#include "duerapp_alarm.h"
#include <unistd.h>

extern int duer_init_play(void);

static bool s_started = false;

static void duer_record_event_callback(int event, const void *data, size_t param)
{
    DUER_LOGD("record callback : %d", event);
    switch (event) {
    case REC_START:
    {
        DUER_LOGD("start send voice: %s", (const char *)data);
        baidu_json *path = baidu_json_CreateObject();
        baidu_json_AddStringToObject(path, "path", (const char *)data);
        duer_data_report(path);
        baidu_json_Delete(path);
        duer_voice_start(param);
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

    DUER_LOGD("event: %d", event->_event);
    switch (event->_event) {
    case DUER_EVENT_STARTED:
        s_started = true;
        break;
    case DUER_EVENT_STOPPED:
        s_started = false;
        break;
    }
}

static void duer_voice_result(struct baidu_json *result)
{
    char* str_result = baidu_json_PrintUnformatted(result);
    DUER_LOGD("duer_voice_result:%s", str_result);
    DUER_FREE(str_result);

}

// It called in duerapp_wifi_config.c, when the Wi-Fi is connected.
void duer_test_start(const char* profile)
{
    const char *data = duer_load_profile(profile);
    if (data == NULL) {
        DUER_LOGE("load profile failed!");
        return;
    }

    DUER_LOGD("profile: \n%s", data);

    // We start the duer with duer_start(), when network connected.
    duer_start(data, strlen(data));

    free((void *)data);
}

int main(int argc, char* argv[])
{
    //initialize_sdcard();

    //initialize_gpio();
    //get NTP time
    //get_ntp_time();

    DUER_LOGI("before initialize");// will not print due to before initialize

    // Initialize the Duer OS
    duer_args_t duer_args = DEFAULT_ARGS;
    duer_args_parse(argc, argv, &duer_args, false);

    printf("profile_path : [%s] \n", duer_args.profile_path);
    printf("voice_record_path: [%s] \n", duer_args.voice_record_path);
    printf("interval_between_query: [%d] \n", duer_args.interval_between_query);

    duer_initialize();
    DUER_LOGI("after initialize");

    // Initialize the alarm
    //TODO duer_init_alarm();

    // Set the Duer Event Callback
    duer_set_event_callback(duer_event_hook);

    // Set the voice interaction result
    duer_voice_set_result_callback(duer_voice_result);

    // Initialize play
    //TODO duer_init_play();

    duer_test_start(duer_args.profile_path);
    duer_record_set_event_callback(duer_record_event_callback);

    sleep(10); // wait for ca started
    while(1) {
        //initialize_wifi();
        DUER_LOGI("in main thread send record file");

        if (s_started) {
            duer_record_all(duer_args.voice_record_path, duer_args.interval_between_query);
        } else {
            DUER_LOGI("not start yet!!");
        }
        sleep(duer_args.interval_between_query);
    }
    return 0;
}
