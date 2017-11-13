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
 * File: lightduer_dcs_audio_player.c
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: apply APIs to support DCS audio player interface
 */

#include "lightduer_dcs.h"
#include <stdbool.h>
#include "lightduer_dcs_router.h"
#include "lightduer_dcs_local.h"
#include "lightduer_memory.h"
#include "lightduer_log.h"
#include "lightduer_ca.h"
#include "lightduer_queue_cache.h"
#include "lightduer_mutex.h"
#include "lightduer_connagent.h"

typedef struct _play_item_t {
    char *url;
    char *token;
} play_item_t;

enum _play_state {
    PLAYING,
    STOPPED,
    PAUSED,
    BUFFER_UNDERRUN,
    FINISHED,
    MAX_PLAY_STATE,
};

static const char* const s_player_activity[MAX_PLAY_STATE] = {"PLAYING", "STOPPED", "PAUSED",
                                                              "BUFFER_UNDERRUN", "FINISHED"};

static duer_qcache_handler s_play_queue = NULL;
static char *s_latest_token = NULL;
static const char* const s_event_name_tab[] = {"PlaybackStarted",
                                               "PlaybackFinished",
                                               "PlaybackNearlyFinished",
                                               "PlaybackStopped",
                                               "PlaybackPaused",
                                               "PlaybackResumed",
                                               "PlaybackQueueCleared"};
static bool s_is_player_supported = false;
static volatile int s_play_state = FINISHED;
static volatile int s_play_offset = 0;
static duer_mutex_t s_queue_lock;

static void duer_destroy_play_item(play_item_t *item)
{
    if (!item) {
        return;
    }

    if (item->url) {
        DUER_FREE(item->url);
    }

    if (item->token) {
        DUER_FREE(item->token);
    }

    DUER_FREE(item);
}

static play_item_t *duer_create_play_item(const char *url, const char *token)
{
    play_item_t *item = NULL;
    duer_status_t rs = DUER_OK;

    if (!url || !token) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    item = DUER_MALLOC(sizeof(play_item_t));
    if (!item) {
        rs = DUER_ERR_MEMORY_OVERLOW;
        goto RET;
    }

    item->url = duer_strdup_internal(url);
    if (!item->url) {
        rs = DUER_ERR_MEMORY_OVERLOW;
        goto RET;
    }

    item->token = duer_strdup_internal(token);
    if (!item->token) {
        rs = DUER_ERR_MEMORY_OVERLOW;
        goto RET;
    }

RET:
    if (rs != DUER_OK) {
        duer_destroy_play_item(item);
        item = NULL;
    }

    return item;
}

static void duer_empty_play_queue()
{
    play_item_t *item = NULL;

    while ((item = duer_qcache_pop(s_play_queue)) != NULL) {
        duer_destroy_play_item(item);
    }
}

int duer_dcs_report_play_event(duer_dcs_audio_event_t type)
{
    baidu_json *data = NULL;
    baidu_json *event = NULL;
    baidu_json *payload = NULL;
    int rs = DUER_OK;

    if ((int)type < 0 || type >= sizeof(s_event_name_tab) / sizeof(s_event_name_tab[0])) {
        DUER_LOGE("Invalid event type!");
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    if ((type != DCS_PLAY_QUEUE_CLEARED) && !s_latest_token) {
        DUER_LOGE("No token was stored!");
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    data = baidu_json_CreateObject();
    if (data == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    event = duer_create_dcs_event("ai.dueros.device_interface.audio_player",
                                  s_event_name_tab[type],
                                  NULL);
    if (event == NULL) {
        rs = DUER_ERR_FAILED;
        goto RET;
    }

    baidu_json_AddItemToObject(data, "event", event);

    if (type != DCS_PLAY_QUEUE_CLEARED) {
        payload = baidu_json_GetObjectItem(event, "payload");
        if (!payload) {
            rs = DUER_ERR_FAILED;
            goto RET;
        }
        baidu_json_AddStringToObject(payload, "token", s_latest_token);
        baidu_json_AddNumberToObject(payload, "offsetInMilliseconds", 0);
    }

    rs = duer_data_report(data);

RET:
    if (data) {
        baidu_json_Delete(data);
    }

    return rs;
}

static void duer_start_audio_play()
{
    play_item_t *first_item = NULL;

    first_item = duer_qcache_top(s_play_queue);
    if (!first_item || !first_item->token || !first_item->url) {
        s_play_state = FINISHED;
        return;
    }

    // If high priority channel is playing, pending audio player
    if (duer_speech_need_play_internal()) {
        s_play_state = PAUSED;
        s_play_offset = 0;
        return;
    } else {
        s_play_state = PLAYING;
    }

    if (s_latest_token) {
        DUER_FREE(s_latest_token);
    }
    s_latest_token = duer_strdup_internal(first_item->token);

    duer_dcs_report_play_event(DCS_PLAY_STARTTED);
    duer_dcs_audio_play_handler(first_item->url);
}

static void duer_replace_play_queue(const char *url, const char *token)
{
    play_item_t *item = NULL;

    if (s_play_state == PLAYING) {
        duer_dcs_audio_stop_handler();
    }

    duer_mutex_lock(s_queue_lock);

    duer_empty_play_queue();
    item = duer_create_play_item(url, token);
    duer_qcache_push(s_play_queue, item);
    duer_start_audio_play();

    duer_mutex_unlock(s_queue_lock);
}

static void duer_enqueue_play_item(const char *url, const char *token)
{
    play_item_t *item = NULL;

    item = duer_create_play_item(url, token);

    duer_mutex_lock(s_queue_lock);

    duer_qcache_push(s_play_queue, item);

    if (s_play_state == FINISHED) {
        duer_start_audio_play();
    }

    duer_mutex_unlock(s_queue_lock);
}

static void duer_replace_enqueued_item(const char *url, const char *token)
{
    play_item_t *item = NULL;

    duer_mutex_lock(s_queue_lock);

    item = duer_qcache_pop(s_play_queue);
    duer_empty_play_queue();
    duer_qcache_push(s_play_queue, item);

    item = duer_create_play_item(url, token);
    duer_qcache_push(s_play_queue, item);

    if (s_play_state == FINISHED) {
        duer_start_audio_play();
    }

    duer_mutex_unlock(s_queue_lock);
}

static duer_status_t duer_audio_play_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *url = NULL;
    baidu_json *token = NULL;
    baidu_json *stream = NULL;
    baidu_json *audio_item = NULL;
    duer_status_t ret = DUER_OK;
    baidu_json *behavior = NULL;

    DUER_LOGV("Enter");

    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        DUER_LOGE("Failed to parse payload\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    audio_item = baidu_json_GetObjectItem(payload, "audioItem");
    behavior = baidu_json_GetObjectItem(payload, "playBehavior");
    if (!audio_item || !behavior) {
        DUER_LOGE("Failed to parse directive\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    stream = baidu_json_GetObjectItem(audio_item, "stream");

    if (!stream) {
        DUER_LOGE("Failed to parse stream\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    token = baidu_json_GetObjectItem(stream, "token");
    url = baidu_json_GetObjectItem(stream, "url");
    if (!token || !url) {
        DUER_LOGE("Failed to parse directive\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    DUER_LOGI("url: %s", url->valuestring);
    DUER_LOGI("token: %s", token->valuestring);
    DUER_LOGI("behavior: %s", behavior->valuestring);

    if (strcmp(behavior->valuestring, "REPLACE_ALL") == 0) {
        duer_replace_play_queue(url->valuestring, token->valuestring);
    } else if (strcmp(behavior->valuestring, "ENQUEUE") == 0) {
        duer_enqueue_play_item(url->valuestring, token->valuestring);
    } else if (strcmp(behavior->valuestring, "REPLACE_ENQUEUED") == 0) {
        duer_replace_enqueued_item(url->valuestring, token->valuestring);
    } else {
        DUER_LOGE("Invalid playBehavior\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

RET:
    return ret;
}

static duer_status_t duer_queue_clear_cb(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *behavior = NULL;
    duer_status_t ret = DUER_OK;
    play_item_t *item = NULL;

    DUER_LOGV("Enter");

    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        DUER_LOGE("Failed to parse payload\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    behavior = baidu_json_GetObjectItem(payload, "clearBehavior");
    if (!behavior) {
        DUER_LOGE("Failed to parse clearBehavior\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

    if (strcmp(behavior->valuestring, "CLEAR_ENQUEUED") == 0) {
        duer_mutex_lock(s_queue_lock);
        item = duer_qcache_pop(s_play_queue);
        duer_empty_play_queue();
        duer_qcache_push(s_play_queue, item);
        duer_mutex_unlock(s_queue_lock);
        duer_dcs_report_play_event(DCS_PLAY_QUEUE_CLEARED);
    } else if (strcmp(behavior->valuestring, "CLEAR_ALL") == 0) {
        duer_dcs_audio_stop_handler();
        s_play_state = STOPPED;
        duer_mutex_lock(s_queue_lock);
        duer_empty_play_queue();
        duer_mutex_unlock(s_queue_lock);
        duer_dcs_report_play_event(DCS_PLAY_QUEUE_CLEARED);
    } else {
        DUER_LOGE("Invalid clearBehavior\n");
        ret = DUER_MSG_RSP_BAD_REQUEST;
        goto RET;
    }

RET:
    return ret;
}

static duer_status_t duer_audio_stop_cb(const baidu_json *directive)
{
    duer_dcs_audio_stop_handler();
    s_play_state = STOPPED;

    return DUER_OK;
}

void duer_pause_audio_internal()
{
    if (s_play_state == PLAYING) {
        s_play_state = PAUSED;
        s_play_offset = duer_dcs_audio_pause_handler();
        duer_dcs_report_play_event(DCS_PLAY_PAUSED);
    }
}

void duer_resume_audio_internal()
{
    play_item_t *first_item = NULL;

    if (s_play_state != PAUSED) {
        return;
    }

    s_play_state = PLAYING;

    if (s_play_offset == 0) {
        // This audio had not been played
        duer_mutex_lock(s_queue_lock);
        duer_start_audio_play();
        duer_mutex_unlock(s_queue_lock);
    } else {
        duer_mutex_lock(s_queue_lock);
        first_item = duer_qcache_top(s_play_queue);
        if (!first_item || !first_item->token || !first_item->url) {
            s_play_state = FINISHED;
            duer_mutex_unlock(s_queue_lock);
            return;
        }

        duer_dcs_audio_seek_handler(first_item->url, s_play_offset);
        duer_mutex_unlock(s_queue_lock);
        duer_dcs_report_play_event(DCS_PLAY_RESUMED);
    }
}

void duer_dcs_audio_on_finished()
{
    play_item_t *item;

    if (s_play_state != PLAYING) {
        return;
    }

    duer_dcs_report_play_event(DCS_PLAY_NEARLY_FINISHED);
    duer_dcs_report_play_event(DCS_PLAY_FINISHED);

    duer_mutex_lock(s_queue_lock);

    item = duer_qcache_pop(s_play_queue);
    duer_destroy_play_item(item);

    duer_start_audio_play();
    duer_mutex_unlock(s_queue_lock);
}

baidu_json* duer_get_playback_state_internal()
{
    baidu_json *state = NULL;
    baidu_json *payload = NULL;

    if (!s_is_player_supported) {
        goto error_out;
    }

    state = duer_create_dcs_event("ai.dueros.device_interface.audio_player", "PlaybackState", NULL);
    if (!state) {
        DUER_LOGE("Failed to create playback state");
        goto error_out;
    }

    payload = baidu_json_GetObjectItem(state, "payload");
    if (!payload) {
        DUER_LOGE("Failed to get payload item");
        goto error_out;
    }

    baidu_json_AddStringToObject(payload, "token", s_latest_token ? s_latest_token : "");
    baidu_json_AddNumberToObject(payload, "offsetInMilliseconds", 0);
    baidu_json_AddStringToObject(payload, "playerActivity", s_player_activity[s_play_state]);

    return state;

error_out:
    if (state) {
        baidu_json_Delete(state);
    }

    return NULL;
}

void duer_dcs_audio_player_init(void)
{
    static bool is_first_time = true;

    if (!is_first_time) {
        return;
    }

    is_first_time = false;

    if (!s_play_queue) {
        s_play_queue = duer_qcache_create();
    }

    if (!s_queue_lock) {
        s_queue_lock = duer_mutex_create();
    }

    duer_directive_list res[] = {
        {"Play", duer_audio_play_cb},
        {"Stop", duer_audio_stop_cb},
        {"ClearQueue", duer_queue_clear_cb},
    };

    duer_add_dcs_directive(res, sizeof(res) / sizeof(res[0]));
    s_is_player_supported = true;
}

