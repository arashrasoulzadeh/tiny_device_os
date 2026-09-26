#pragma once

/**
 * Portable key codes shared by the simulator and hardware ports.
 * Kept independent of SDL so apps/board firmware can include this alone.
 */

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
    SIM_KEY_0,
    SIM_KEY_1,
    SIM_KEY_2,
    SIM_KEY_3,
    SIM_KEY_4,
    SIM_KEY_5,
    SIM_KEY_6,
    SIM_KEY_7,
    SIM_KEY_8,
    SIM_KEY_9,
    SIM_KEY_10,
    SIM_KEY_PLUS,
    SIM_KEY_MINUS,
    SIM_KEY_EQUALS,
    SIM_KEY_A,
    SIM_KEY_B,
    SIM_KEY_C,
    SIM_KEY_D,
    SIM_KEY_E,
    SIM_KEY_F,
    SIM_KEY_G,
    SIM_KEY_H,
    SIM_KEY_I,
    SIM_KEY_J,
    SIM_KEY_K,
    SIM_KEY_L,
    SIM_KEY_M,
    SIM_KEY_N,
    SIM_KEY_O,
    SIM_KEY_P,
    SIM_KEY_Q,
    SIM_KEY_R,
    SIM_KEY_S,
    SIM_KEY_T,
    SIM_KEY_U,
    SIM_KEY_V,
    SIM_KEY_W,
    SIM_KEY_X,
    SIM_KEY_Y,
    SIM_KEY_Z,
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
    SIM_KEY_SYS_NEXT_APP,
    SIM_KEY_SYS_ESCAPE,
    SIM_KEY_SYS_MENU,
} sim_key_t;

typedef enum {
    SIM_KEY_CLASS_APP = 0,
    SIM_KEY_CLASS_SYSTEM,
} sim_key_class_t;

sim_key_class_t sim_key_get_class(sim_key_t key);
bool sim_key_is_system(sim_key_t key);

#ifdef __cplusplus
}
#endif
