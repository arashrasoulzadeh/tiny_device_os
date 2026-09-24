#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SIM_KEY_UNKNOWN = 0,
    SIM_KEY_UP,
    SIM_KEY_DOWN,
    SIM_KEY_LEFT,
    SIM_KEY_RIGHT,
    SIM_KEY_ENTER,
    SIM_KEY_ESCAPE,
    SIM_KEY_SPACE,
    SIM_KEY_A,
    SIM_KEY_B,
    SIM_KEY_X,
    SIM_KEY_Y,
    SIM_KEY_L,
    SIM_KEY_R,
    SIM_KEY_F1,
    SIM_KEY_F2,
    SIM_KEY_F3,
    SIM_KEY_F4,
    SIM_KEY_F5,
    SIM_KEY_F6,
    SIM_KEY_F7,
    SIM_KEY_F8,
    SIM_KEY_F9,
    SIM_KEY_F10,
    SIM_KEY_F11,
    SIM_KEY_F12,
} sim_key_t;

typedef void (*sim_key_callback_t)(sim_key_t key, bool pressed, void* arg);

int sim_video_init(int width, int height, const char* title);
void sim_video_cleanup(void);

void sim_video_poll_events(void);
void sim_video_render(void);

void sim_video_set_key_callback(sim_key_callback_t cb, void* arg);

void sim_video_get_size(int* width, int* height);
void sim_video_set_title(const char* title);

bool sim_video_is_headless(void);

uint32_t* sim_video_get_pixels(void);
int sim_video_get_width(void);
int sim_video_get_height(void);

#ifdef __cplusplus
}
#endif