#include "pong.h"

void pong_reset(pong_t* game) {
    if (!game) {
        return;
    }
    game->paddle_y = (int16_t)((PONG_H - PONG_PADDLE_H) / 2);
    game->ball_x = (int16_t)(PONG_W / 2);
    game->ball_y = (int16_t)((PONG_H - PONG_BALL) / 2);
    game->ball_dx = -1;
    game->ball_dy = 1;
    game->score = 0;
    game->game_over = false;
}

void pong_paddle_up(pong_t* game) {
    if (!game || game->game_over || game->paddle_y <= 0) {
        return;
    }
    game->paddle_y--;
}

void pong_paddle_down(pong_t* game) {
    if (!game || game->game_over) {
        return;
    }
    if (game->paddle_y >= (int16_t)(PONG_H - PONG_PADDLE_H)) {
        return;
    }
    game->paddle_y++;
}

static bool hits_paddle(const pong_t* game) {
    const int ball_bottom = game->ball_y + PONG_BALL - 1;
    const int pad_bottom = game->paddle_y + PONG_PADDLE_H - 1;
    return ball_bottom >= game->paddle_y && game->ball_y <= pad_bottom;
}

void pong_step(pong_t* game) {
    if (!game || game->game_over) {
        return;
    }

    game->ball_x = (int16_t)(game->ball_x + game->ball_dx);
    game->ball_y = (int16_t)(game->ball_y + game->ball_dy);

    if (game->ball_y < 0) {
        game->ball_y = 0;
        game->ball_dy = 1;
    } else if (game->ball_y > (int16_t)(PONG_H - PONG_BALL)) {
        game->ball_y = (int16_t)(PONG_H - PONG_BALL);
        game->ball_dy = -1;
    }

    if (game->ball_dx < 0 && game->ball_x <= (int16_t)(PONG_PADDLE_X + PONG_PADDLE_W)) {
        if (hits_paddle(game)) {
            game->ball_dx = 1;
            game->ball_x = (int16_t)(PONG_PADDLE_X + PONG_PADDLE_W);
            if (game->score < 65535) {
                game->score++;
            }
            return;
        }
        if (game->ball_x < PONG_PADDLE_X) {
            game->game_over = true;
            return;
        }
    }

    if (game->ball_x > (int16_t)(PONG_W - 2 - PONG_BALL)) {
        game->ball_x = (int16_t)(PONG_W - 2 - PONG_BALL);
        game->ball_dx = -1;
    }
}
