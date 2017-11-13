// Copyright (2017) Baidu Inc. All rights reserved.
/**
 * File: duerapp_recorder.c
 * Auth: Su Hao(suhao@baidu.com)
 * Desc: Simulate the recorder from the WAV with 8KHz/16bits.
 */

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "duerapp_config.h"
#include "lightduer_types.h"

#define SAMPLES_PATH    SDCARD_PATH"/samples"
#define TEST_FILE_NUM   5
#define FILE_NAME_LEN   25

#define BUFFER_SIZE         2000 //((2000 / RECORD_FARME_SIZE) * RECORD_FARME_SIZE)
#define RECORD_DELAY(_x)    ((_x) * 20 * 10000 / RECORD_FARME_SIZE / 10000)

#define TO_LOWER(_x)    (((_x) <= 'Z' && (_x) >= 'A') ? ('a' + (_x) - 'A') : (_x))
#define WAV_HEADER_SAMPLE_RATE_POS 24

static duer_record_event_func g_event_callback = NULL;

static int duer_is_wav(const char *name)
{
    const char *p = NULL;
    const char *wav = ".wav";

    if (name == NULL) {
        return 0;
    }

    while (*name != '\0') {
        if (*name == '.') {
            p = name;
        }
        name++;
    }

    if (p == NULL) {
        return 0;
    }

    while (*p != '\0' && *wav != '\0' && TO_LOWER(*p) == *wav) {
        p++;
        wav++;
    }

    return *p == *wav;
}

static void duer_record_event(int status, const void *data, size_t size)
{
    if (g_event_callback != NULL) {
        g_event_callback(status, data, size);
    }
}

static void duer_record_from_file(const char *path)
{
    FILE *file = NULL;
    char buff[BUFFER_SIZE];
    size_t buffsize = BUFFER_SIZE;
    size_t sample_rate = 0;

    do {
        file = fopen(path, "rb");
        if (file == NULL) {
            DUER_LOGE("Open file %s", path);
            break;
        }

        // Skip the header
        int rs = fread(buff, 1, 44, file);
        if (rs != 44) {
            DUER_LOGE("Read header failed %s", path);
            break;
        }

        sample_rate = *((int32_t *)&buff[WAV_HEADER_SAMPLE_RATE_POS]);

        duer_record_event(REC_START, path, sample_rate);   //to duer_record_set_event_callback

        buffsize = 1 + rand() % (BUFFER_SIZE - 1);
        while ((rs = fread(buff, 1, buffsize, file)) != 0) {
            duer_record_event(REC_DATA, buff, rs);
            vTaskDelay(RECORD_DELAY(rs) / portTICK_PERIOD_MS);
        }

        duer_record_event(REC_STOP, path, 0);
    } while (0);

    if (file) {
        fclose(file);
    }
}

void duer_record_set_event_callback(duer_record_event_func func)
{
    if (g_event_callback != func) {
        g_event_callback = func;
    }
}


void duer_record_from_dir(const char *path)
{
#ifdef TEST_PLAYLIST
    static int i = 1;
    char file_path[FILE_NAME_LEN];

    i %= TEST_FILE_NUM + 1;
    if (!i) {
        i++;
    }
    snprintf(file_path, FILE_NAME_LEN, "%s/%d%s", path, i++, ".WAV");
    DUER_LOGI("Open file %s", file_path);
    duer_record_from_file(file_path);
#else

    DIR *dir = NULL;
    char buff[100];
    size_t length = 0;
    do {
        length = snprintf(buff, sizeof(buff), "%s/", path);
        if (length == sizeof(buff)) {
            DUER_LOGE("The path (%s) is too long!", buff);
            break;
        }

        dir = opendir(path);
        if (dir == NULL) {
            DUER_LOGE("Open %s failed!", path);
            break;
        }
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            DUER_LOGI("Open file type = %d, name = %s", ent->d_type, ent->d_name);

            if (ent->d_type == 1 && duer_is_wav(ent->d_name)) {

                snprintf(buff + length, sizeof(buff) - length, "%s", ent->d_name);

                    duer_record_from_file(buff);

                    vTaskDelay(10000 / portTICK_PERIOD_MS);
            }

        }
    } while (0);

    if (dir) {
        closedir(dir);
    }
#endif
}
void duer_record_all()
{
    duer_record_from_dir(SAMPLES_PATH);
}
