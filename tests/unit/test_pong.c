#include "unity.h"
#include "pong.h"

void setUp(void) {}
void tearDown(void) {}

void test_pong_reset_centers_play(void) {
    pong_t game;
    pong_reset(&game);
    TEST_ASSERT_EQUAL(PONG_W / 2, game.ball_x);
    TEST_ASSERT_EQUAL((PONG_H - PONG_BALL) / 2, game.ball_y);
    TEST_ASSERT_EQUAL((PONG_H - PONG_PADDLE_H) / 2, game.paddle_y);
    TEST_ASSERT_EQUAL(-1, game.ball_dx);
    TEST_ASSERT_EQUAL(0, game.score);
    TEST_ASSERT_FALSE(game.game_over);
}

void test_pong_paddle_clamps(void) {
    pong_t game;
    pong_reset(&game);
    game.paddle_y = 0;
    pong_paddle_up(&game);
    TEST_ASSERT_EQUAL(0, game.paddle_y);
    game.paddle_y = (int16_t)(PONG_H - PONG_PADDLE_H);
    pong_paddle_down(&game);
    TEST_ASSERT_EQUAL(PONG_H - PONG_PADDLE_H, game.paddle_y);
}

void test_pong_step_moves_ball(void) {
    pong_t game;
    pong_reset(&game);
    pong_step(&game);
    TEST_ASSERT_EQUAL(PONG_W / 2 - 1, game.ball_x);
    TEST_ASSERT_EQUAL((PONG_H - PONG_BALL) / 2 + 1, game.ball_y);
}

void test_pong_top_wall_bounces(void) {
    pong_t game;
    pong_reset(&game);
    game.ball_x = 40;
    game.ball_y = 0;
    game.ball_dy = -1;
    pong_step(&game);
    TEST_ASSERT_EQUAL(0, game.ball_y);
    TEST_ASSERT_EQUAL(1, game.ball_dy);
}

void test_pong_paddle_scores(void) {
    pong_t game;
    pong_reset(&game);
    game.ball_x = (int16_t)(PONG_PADDLE_X + PONG_PADDLE_W + 1);
    game.ball_y = game.paddle_y;
    game.ball_dx = -1;
    pong_step(&game);
    TEST_ASSERT_EQUAL(1, game.score);
    TEST_ASSERT_EQUAL(1, game.ball_dx);
    TEST_ASSERT_FALSE(game.game_over);
}

void test_pong_miss_ends_game(void) {
    pong_t game;
    int guard;
    pong_reset(&game);
    game.ball_x = 2;
    game.ball_y = 0;
    game.paddle_y = (int16_t)(PONG_H - PONG_PADDLE_H);
    game.ball_dx = -1;
    game.ball_dy = 0;
    for (guard = 0; guard < 8 && !game.game_over; guard++) {
        pong_step(&game);
    }
    TEST_ASSERT_TRUE(game.game_over);
    TEST_ASSERT_EQUAL(0, game.score);
    game.ball_x = 10;
    pong_step(&game);
    TEST_ASSERT_EQUAL(10, game.ball_x);
}

void test_pong_right_wall_bounces(void) {
    pong_t game;
    pong_reset(&game);
    game.ball_x = (int16_t)(PONG_W - 2 - PONG_BALL);
    game.ball_dx = 1;
    game.ball_dy = 0;
    pong_step(&game);
    TEST_ASSERT_EQUAL(-1, game.ball_dx);
    TEST_ASSERT_EQUAL(PONG_W - 2 - PONG_BALL, game.ball_x);
    TEST_ASSERT_EQUAL(0, game.score);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pong_reset_centers_play);
    RUN_TEST(test_pong_paddle_clamps);
    RUN_TEST(test_pong_step_moves_ball);
    RUN_TEST(test_pong_top_wall_bounces);
    RUN_TEST(test_pong_paddle_scores);
    RUN_TEST(test_pong_miss_ends_game);
    RUN_TEST(test_pong_right_wall_bounces);
    return UNITY_END();
}
