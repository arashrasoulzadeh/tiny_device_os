#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "hal_audio.h"

#ifdef __cplusplus
extern "C" {
#endif

struct hal_audio;

int sim_audio_init(uint32_t sample_rate, uint16_t channels, uint16_t buffer_frames);
void sim_audio_cleanup(void);

void sim_audio_set_hal_audio(struct hal_audio* audio);
void sim_audio_start(void);
void sim_audio_stop(void);

// Volume and mute control (delegates to HAL audio)
int sim_audio_set_volume(float volume);
float sim_audio_get_volume(void);
int sim_audio_set_mute(bool mute);
bool sim_audio_get_mute(void);

// Callback (delegates to HAL audio)
int sim_audio_set_callback(hal_audio_callback_t cb, void* arg);

#ifdef __cplusplus
}
#endif