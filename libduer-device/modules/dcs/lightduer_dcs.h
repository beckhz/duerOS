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
 * File: lightduer_dcs_router.h
 * Auth: Gang Chen (chengang12@baidu.com)
 * Desc: Light duer DCS APIS.
 */
#ifndef BAIDU_DUER_LIGHTDUER_DCS_H
#define BAIDU_DUER_LIGHTDUER_DCS_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DCS_PLAY_CMD,
    DCS_PAUSE_CMD,
    DCS_PREVIOUS_CMD,
    DCS_NEXT_CMD,
    DCS_PLAY_CONTROL_EVENT_NUMBER,
} duer_dcs_play_control_cmd_t;

/**
 * Initialize the dcs framework.
 *
 * @return none.
 */
void duer_dcs_framework_init(void);

/**
 * DESC:
 * Initialize dcs voice input interface.
 *
 * PARAM: none
 *
 * @RETURN: none
 */
void duer_dcs_voice_input_init(void);

/**
 * DESC:
 * Notify DCS when recorder start to record.
 *
 * PARAM: none
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_dcs_on_listen_started(void);

/**
 * DESC:
 * Developer needs to implement this interface to stop recording.
 *
 * PARAM: none
 *
 * @RETURN: none.
 */
void duer_dcs_stop_listen_handler(void);

/**
 * DESC:
 * Initialize dcs voice output interface
 *
 * PARAM: none
 *
 * @RETURN: none
 */
void duer_dcs_voice_output_init(void);

/**
 * DESC:
 *
 * It should be called when speech finished, used to notify DCS level.
 *
 * PARAM: none
 *
 * @RETURN: none
 */
void duer_dcs_speech_on_finished(void);

/**
 * DESC:
 * Developer needs to implement this interface to play speech.
 *
 * PARAM:
 * @param[in] url: the url of the speech need to play
 *
 * @RETURN: none.
 */
void duer_dcs_speak_handler(const char *url);

/**
 * DESC:
 * Initialize dcs speaker controler interface to enable volume control function.
 *
 * PARAM: none
 *
 * @RETURN: none
 */
void duer_dcs_speaker_control_init(void);

/**
 * DESC:
 * Notify DCS when volume changed
 *
 * PARAM: none
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_dcs_on_volume_changed(void);

/**
 * DESC:
 * Notify DCS when mute state changed.
 *
 * PARAM: none
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_dcs_on_mute(void);

/**
 * DESC:
 * Developer needs to implement this interface, it is used to get volume state.
 *
 * @param[out] volume: current volume value.
 * @param[out] is_mute: current mute state.
 *
 * @RETURN: none.
 */
void duer_dcs_get_speaker_state(int *volume, bool *is_mute);

/**
 * DESC:
 * Developer needs to implement this interface to set volume.
 *
 * PARAM:
 * @param[in] volume: the value of the volume need to set
 *
 * @RETURN: none.
 */
void duer_dcs_volume_set_handler(int volume);

/**
 * DESC:
 * Developer needs to implement this interface to adjust volume.
 *
 * PARAM:
 * @param[in] volume: the value need to adjusted.
 *
 * @RETURN: none.
 */
void duer_dcs_volume_adjust_handler(int volume);

/**
 * DESC:
 * Developer needs to implement this interface to change mute state.
 *
 * PARAM:
 * @param[in] is_mute: set/discard mute.
 *
 * @RETURN: none.
 */
void duer_dcs_mute_handler(bool is_mute);

/**
 * DESC:
 * Initialize dcs audio player interface.
 *
 * PARAM: none
 *
 * @RETURN: none
 */
void duer_dcs_audio_player_init(void);

/**
 * DESC:
 * Notify DCS when an audio is finished.
 *
 * PARAM: none
 *
 * @RETURN: none.
 */
void duer_dcs_audio_on_finished(void);

/**
 * DESC:
 * Developer needs to implement this interface to play audio.
 *
 * PARAM:
 * @param[in] url: the url of the audio need to play
 *
 * @RETURN: none.
 */
void duer_dcs_audio_play_handler(const char *url);

/**
 * DESC:
 * Developer needs to implement this interface to stop audio player.
 *
 * PARAM: none
 *
 * @RETURN: none.
 */
void duer_dcs_audio_stop_handler(void);

/**
 * DESC:
 * Developer needs to implement this interface to resume audio play.
 *
 * PARAM:
 * @param[in] url: the url of the audio need to play
 * @param[in] offset: the position of the audio need to seek
 *
 * @RETURN: none.
 */
void duer_dcs_audio_seek_handler(const char* url, int offset);

/**
 * DESC:
 * Developer needs to implement this interface to pause audio play.
 *
 * PARAM: none
 *
 * @RETURN: the play position of the current audio.
 */
int duer_dcs_audio_pause_handler(void);

/**
 * DESC:
 * Realize play control(play, pause, next/previous audio) by send command to DCS.
 *
 * PARAM[in] cmd: command type.
 *
 * @RETURN: 0 when success, negative when fail.
 */
int duer_dcs_send_play_control_cmd(duer_dcs_play_control_cmd_t cmd);

/**
 * DESC:
 * Report current state after device boot.
 *
 * PARAM: none
 *
 * @RETURN: none.
 */
void duer_dcs_sync_state(void);

#ifdef __cplusplus
}
#endif

#endif/*BAIDU_DUER_LIGHTDUER_DCS_H*/

