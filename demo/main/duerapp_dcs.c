// Copyright (2017) Baidu Inc. All rights reserveed.
/**
 * File: duerapp_dcs.c
 * Auth:       
 * Desc: About DCS 3.0
 */

#include "duerapp_dcs.h"
#include "duerapp_config.h"
#include "lightduer_voice.h"

static unsigned char s_volume = 20;  //(0 - 100)
static bool s_is_mute = false;

void duer_dcs_init(void)
{
	static bool is_first_time = true;

	DUER_LOGI("duer_dcs_init\n");
	duer_dcs_framework_init();
	duer_dcs_voice_input_init();
	duer_dcs_voice_output_init();
	//duer_dcs_speaker_control_init();
	//duer_dcs_audio_player_init();

	if(is_first_time)
	{
		is_first_time = false;
		//freertos register_listener
		duer_dcs_sync_state();
	}
}

void duer_dcs_stop_listen_handler(void)
{
    DUER_LOGI("stop_listen and please cloces mic\n");
    duer_voice_stop();
}

void duer_dcs_speak_handler(const char *url)
{
    DUER_LOGI("play speak: %s\n", url );
    duer_dcs_speech_on_finished();
}

void duer_dcs_audio_play_handler(const char *url)
{
    DUER_LOGI("play audio: %s\n", url );
    duer_dcs_audio_on_finished();
}

void duer_dcs_get_speaker_state(int *volume, bool *is_mute)
{
	DUER_LOGD("duer_dcs_get_speaker_state\n");
	*volume = s_volume;
	*is_mute = s_is_mute;
}
