#include "hal_audio.h"
#include "hal_power.h"
#include <driver/i2s.h>
#include <esp_err.h>
#include <string.h>
#include <stdlib.h>

#define I2S_NUM I2S_NUM_0
#define I2S_DMA_BUF_COUNT 4
#define I2S_DMA_BUF_LEN 256

typedef struct hal_audio {
    char path[32];
    hal_audio_config_t config;
    i2s_config_t i2s_config;
    bool initialized;
    bool running;
    hal_audio_callback_t callback;
    void* callback_arg;
    float volume;
    bool mute;
} hal_audio_t;

static i2s_comm_format_t hal_to_i2s_format(hal_audio_format_t format) {
    return I2S_COMM_FORMAT_STAND_I2S;
}

static i2s_bits_per_sample_t hal_to_i2s_bits(hal_audio_format_t format) {
    switch (format) {
        case HAL_AUDIO_FORMAT_PCM_U8: return I2S_BITS_PER_SAMPLE_8BIT;
        case HAL_AUDIO_FORMAT_PCM_S16_LE:
        case HAL_AUDIO_FORMAT_PCM_S16_BE: return I2S_BITS_PER_SAMPLE_16BIT;
        case HAL_AUDIO_FORMAT_PCM_S24_LE:
        case HAL_AUDIO_FORMAT_PCM_S24_BE: return I2S_BITS_PER_SAMPLE_24BIT;
        case HAL_AUDIO_FORMAT_PCM_S32_LE:
        case HAL_AUDIO_FORMAT_PCM_S32_BE:
        case HAL_AUDIO_FORMAT_PCM_F32_LE: return I2S_BITS_PER_SAMPLE_32BIT;
        default: return I2S_BITS_PER_SAMPLE_16BIT;
    }
}

hal_audio_t* hal_audio_open(const char* path, const hal_audio_config_t* config) {
    hal_audio_t* audio = calloc(1, sizeof(hal_audio_t));
    if (!audio) return NULL;
    
    strncpy(audio->path, path, sizeof(audio->path) - 1);
    
    if (config) {
        audio->config = *config;
    } else {
        audio->config.sample_rate = 44100;
        audio->config.channels = 2;
        audio->config.format = HAL_AUDIO_FORMAT_PCM_S16_LE;
        audio->config.buffer_frames = 512;
        audio->config.period_frames = 256;
        audio->config.output = true;
        audio->config.input = false;
    }
    
    audio->volume = 1.0f;
    audio->mute = false;
    audio->running = false;
    
    return audio;
}

void hal_audio_close(hal_audio_t* audio) {
    if (!audio) return;
    if (audio->initialized) {
        i2s_driver_uninstall(I2S_NUM);
    }
    free(audio);
}

int hal_audio_init(hal_audio_t* audio) {
    if (!audio || audio->initialized) return -1;
    
    audio->i2s_config = (i2s_config_t){
        .mode = I2S_MODE_MASTER | (audio->config.output ? I2S_MODE_TX : 0) | (audio->config.input ? I2S_MODE_RX : 0),
        .sample_rate = audio->config.sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = audio->config.channels == 1 ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 4,
        .dma_buf_len = 256,
        .use_apll = true,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0,
    };
    
    esp_err_t err = i2s_driver_install(I2S_NUM, &audio->i2s_config, 0, NULL);
    if (err != ESP_OK) return -1;
    
    i2s_pin_config_t pin_config = {
        .bck_io_num = GPIO_NUM_14,
        .ws_io_num = GPIO_NUM_15,
        .data_out_num = GPIO_NUM_13,
        .data_in_num = GPIO_NUM_12,
    };
    
    i2s_set_pin(I2S_NUM, &pin_config);
    
    audio->initialized = true;
    return 0;
}

int hal_audio_start(hal_audio_t* audio) {
    if (!audio || audio->running) return -1;
    if (!audio->initialized && hal_audio_init(audio) != 0) return -1;
    
    i2s_start(I2S_NUM);
    audio->running = true;
    return 0;
}

int hal_audio_stop(hal_audio_t* audio) {
    if (!audio || !audio->running) return -1;
    
    i2s_stop(I2S_NUM);
    audio->running = false;
    return 0;
}

int hal_audio_write(hal_audio_t* audio, const void* buffer, size_t frames) {
    if (!audio || !buffer || !audio->running) return -1;
    
    size_t bytes_written = 0;
    esp_err_t err = i2s_write(I2S_NUM, buffer, frames * audio->config.channels * 2, 
                              &bytes_written, pdMS_TO_TICKS(100));
    
    return err == ESP_OK ? (int)(bytes_written / (audio->config.channels * 2)) : -1;
}

int hal_audio_read(hal_audio_t* audio, void* buffer, size_t frames) {
    if (!audio || !buffer) return -1;
    
    size_t bytes_read = 0;
    esp_err_t err = i2s_read(I2S_NUM, buffer, frames * audio->config.channels * 2,
                             &bytes_read, pdMS_TO_TICKS(100));
    
    return err == ESP_OK ? (int)(bytes_read / (audio->config.channels * 2)) : -1;
}

int hal_audio_set_callback(hal_audio_t* audio, hal_audio_callback_t cb, void* arg) {
    if (!audio) return -1;
    audio->callback = cb;
    audio->callback_arg = arg;
    return 0;
}

int hal_audio_get_buffered_frames(const hal_audio_t* audio) {
    if (!audio) return 0;
    return 0;
}

int hal_audio_get_available_frames(const hal_audio_t* audio) {
    if (!audio) return 0;
    return audio->config.buffer_frames;
}

int hal_audio_set_volume(hal_audio_t* audio, float volume) {
    if (!audio) return -1;
    audio->volume = volume < 0 ? 0 : (volume > 1 ? 1 : volume);
    return 0;
}

float hal_audio_get_volume(const hal_audio_t* audio) {
    return audio ? audio->volume : 1.0f;
}

int hal_audio_set_mute(hal_audio_t* audio, bool mute) {
    if (!audio) return -1;
    audio->mute = mute;
    return 0;
}

bool hal_audio_get_mute(const hal_audio_t* audio) {
    return audio ? audio->mute : false;
}

const char* hal_audio_get_path(const hal_audio_t* audio) {
    return audio ? audio->path : NULL;
}

int hal_audio_suspend(hal_audio_t* audio) {
    if (!audio) return -1;
    audio->running = false;
    return 0;
}

int hal_audio_resume(hal_audio_t* audio) {
    if (!audio) return -1;
    audio->running = true;
    return 0;
}