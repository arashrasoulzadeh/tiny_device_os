#pragma once

#include "display.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PONG_W APP_DISPLAY_WIDTH
#define PONG_H APP_DISPLAY_HEIGHT
#define PONG_PADDLE_X 1
#define PONG_PADDLE_W 2
#define PONG_PADDLE_H 8
#define PONG_BALL 2

typedef struct {
    int16_t paddle_y;
    int16_t ball_x;
    int16_t ball_y;
    int8_t ball_dx;
    int8_t ball_dy;
    uint16_t score;
    bool game_over;
} pong_t;

void pong_reset(pong_t* game);
void pong_paddle_up(pong_t* game);
void pong_paddle_down(pong_t* game);
/** Advance the ball one pixel. No-op after a miss. */
void pong_step(pong_t* game);

#ifdef __cplusplus
}
#endif
