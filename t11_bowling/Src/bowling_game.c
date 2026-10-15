#include "bowling_game.h"

#include "ST7920_Parallel.h"
#include "teclado.h"
#include "bowling_score.h"

#include <stdint.h>

#define BOWLING_FULL_PIN_MASK      0x03FFU
#define BOWLING_ANIM_STEP_MS       35U
#define BOWLING_RESULT_HOLD_MS     1100U
#define BOWLING_RENDER_PERIOD_MS   40U

typedef enum
{
    BOWLING_SCREEN_WELCOME = 0,
    BOWLING_SCREEN_PLAY,
    BOWLING_SCREEN_ROLLING,
    BOWLING_SCREEN_RESULT,
    BOWLING_SCREEN_SUMMARY,
    BOWLING_SCREEN_FINAL
} BowlingScreen;

typedef enum
{
    LAST_OPEN = 0,
    LAST_SPARE,
    LAST_STRIKE
} LastResult;

static const uint8_t font_digit[10][7] =
{
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
    {0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}
};

static const uint8_t font_letter[26][7] =
{
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
    {0x0F,0x10,0x10,0x10,0x10,0x10,0x0F},
    {0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
    {0x0F,0x10,0x10,0x17,0x11,0x11,0x0F},
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
    {0x07,0x02,0x02,0x02,0x12,0x12,0x0C},
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
    {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
    {0x11,0x19,0x19,0x15,0x13,0x13,0x11},
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
    {0x11,0x11,0x11,0x11,0x0A,0x0A,0x04},
    {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
    {0x11,0x0A,0x04,0x04,0x04,0x0A,0x11},
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}
};

static const uint8_t font_colon[7] = {0x00,0x04,0x04,0x00,0x04,0x04,0x00};
static const uint8_t font_dash[7] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00};
static const uint8_t font_hash[7] = {0x0A,0x1F,0x0A,0x0A,0x1F,0x0A,0x00};
static const uint8_t font_star[7] = {0x00,0x0A,0x04,0x1F,0x04,0x0A,0x00};
static const uint8_t font_slash[7] = {0x01,0x02,0x04,0x08,0x10,0x00,0x00};
static const uint8_t font_space[7] = {0,0,0,0,0,0,0};

static const int16_t pin_px[10] = {64,58,70,52,64,76,46,58,70,82};
static const int16_t pin_py[10] = {18,24,24,30,30,30,36,36,36,36};
static const int8_t pin_gx[10] = {4,3,5,2,4,6,1,3,5,7};
static const int8_t pin_gy[10] = {0,1,1,2,2,2,3,3,3,3};

static BowlingScreen screen;
static BowlingScoreState score_state;

static uint8_t frame_index;
static uint8_t throw_index;
static uint8_t pins_remaining;

static uint8_t frame_rolls[10][3];
static uint8_t frame_roll_count[10];

static uint8_t frame_total_valid[10];
static uint16_t frame_cumulative[10];

static uint16_t standing_mask;

static uint8_t aim_index;
static uint8_t power_level;

static int16_t ball_x;
static int16_t ball_y;
static int16_t ball_target_x;

static uint8_t pending_knocked;
static uint16_t pending_fall_mask;

static uint8_t last_knocked;
static LastResult last_result;

static uint8_t game_complete;

static uint32_t rng_state;
static uint32_t last_anim_step;
static uint32_t screen_entered_at;
static uint32_t last_render;

static uint8_t abs_diff_u8(uint8_t a, uint8_t b)
{
    if (a >= b)
    {
        return (uint8_t)(a - b);
    }

    return (uint8_t)(b - a);
}

static uint32_t rng_next(void)
{
    rng_state = (rng_state * 1664525UL) + 1013904223UL;
    return rng_state;
}

static void draw_safe_pixel(int16_t x, int16_t y)
{
    if ((x < 0) || (x > 127) || (y < 0) || (y > 63))
    {
        return;
    }

    SetPixel((uint8_t)x, (uint8_t)y);
}

static const uint8_t *get_glyph(char c)
{
    if ((c >= '0') && (c <= '9'))
    {
        return font_digit[c - '0'];
    }

    if ((c >= 'A') && (c <= 'Z'))
    {
        return font_letter[c - 'A'];
    }

    if (c == ':')
    {
        return font_colon;
    }

    if (c == '-')
    {
        return font_dash;
    }

    if (c == '#')
    {
        return font_hash;
    }

    if (c == '*')
    {
        return font_star;
    }

    if (c == '/')
    {
        return font_slash;
    }

    return font_space;
}

static void draw_char5x7(uint8_t x, uint8_t y, char c)
{
    const uint8_t *glyph;
    uint8_t row;

    glyph = get_glyph(c);

    for (row = 0U; row < 7U; row++)
    {
        uint8_t mask;
        uint8_t col;

        mask = 0x10U;

        for (col = 0U; col < 5U; col++)
        {
            if ((glyph[row] & mask) != 0U)
            {
                draw_safe_pixel((int16_t)x + (int16_t)col,
                                (int16_t)y + (int16_t)row);
            }

            mask >>= 1U;
        }
    }
}

static void draw_text5x7(uint8_t x, uint8_t y, const char *text)
{
    uint8_t cursor;

    cursor = x;

    while (*text != '\0')
    {
        draw_char5x7(cursor, y, *text);
        cursor = (uint8_t)(cursor + 6U);
        text++;
    }
}

static void draw_u8(uint8_t x, uint8_t y, uint16_t value, uint8_t digits)
{
    char buffer[6];
    uint8_t i;

    for (i = 0U; i < digits; i++)
    {
        uint8_t idx;

        idx = (uint8_t)(digits - 1U - i);
        buffer[idx] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    }

    buffer[digits] = '\0';
    draw_text5x7(x, y, buffer);
}

static void reset_round_pins(void)
{
    standing_mask = BOWLING_FULL_PIN_MASK;
    pins_remaining = 10U;
}

static void recalculate_cumulative(void)
{
    uint8_t frame;
    uint16_t running_total;

    running_total = 0U;

    for (frame = 0U; frame < 10U; frame++)
    {
        int16_t frame_score;

        frame_score = BowlingScore_FrameScore(&score_state, frame);

        if (frame_score < 0)
        {
            frame_total_valid[frame] = 0U;
            continue;
        }

        running_total = (uint16_t)(running_total + (uint16_t)frame_score);
        frame_total_valid[frame] = 1U;
        frame_cumulative[frame] = running_total;
    }
}

static uint8_t calculate_knocked_pins(void)
{
    int16_t quality;
    int16_t knocked;
    int16_t random_shift;

    random_shift = (int16_t)(rng_next() % 31UL);
    random_shift = (int16_t)(random_shift - 15);

    quality = (int16_t)(45 + ((int16_t)power_level * 11) -
                        ((int16_t)abs_diff_u8(aim_index, 2U) * 14) +
                        random_shift);

    if (quality < 5)
    {
        quality = 5;
    }

    knocked = (int16_t)((((int16_t)pins_remaining) * quality + 50) / 100);

    if ((knocked == 0) && (pins_remaining > 0U) && ((rng_next() % 100UL) < 30UL))
    {
        knocked = 1;
    }

    if (knocked > (int16_t)pins_remaining)
    {
        knocked = (int16_t)pins_remaining;
    }

    if (knocked < 0)
    {
        knocked = 0;
    }

    return (uint8_t)knocked;
}

static uint16_t select_fallen_mask(uint8_t knocked)
{
    uint8_t picked;
    uint16_t mask;

    int16_t impact_x;
    int16_t impact_y;

    impact_x = (int16_t)(4 + ((int16_t)aim_index - 2) * 2);
    impact_y = (int16_t)(3 - (power_level / 2U));

    mask = 0U;

    for (picked = 0U; picked < knocked; picked++)
    {
        uint8_t pin;
        uint8_t found;
        uint8_t best_pin;
        int16_t best_metric;

        found = 0U;
        best_pin = 0U;
        best_metric = 32767;

        for (pin = 0U; pin < 10U; pin++)
        {
            int16_t dx;
            int16_t dy;
            int16_t metric;

            if ((standing_mask & (uint16_t)(1U << pin)) == 0U)
            {
                continue;
            }

            if ((mask & (uint16_t)(1U << pin)) != 0U)
            {
                continue;
            }

            dx = pin_gx[pin] - impact_x;
            if (dx < 0)
            {
                dx = -dx;
            }

            dy = pin_gy[pin] - impact_y;
            if (dy < 0)
            {
                dy = -dy;
            }

            metric = (int16_t)(dx * 3 + dy * 4 + (int16_t)(rng_next() % 3UL));

            if ((found == 0U) || (metric < best_metric))
            {
                best_metric = metric;
                best_pin = pin;
                found = 1U;
            }
        }

        if (found == 0U)
        {
            break;
        }

        mask = (uint16_t)(mask | (uint16_t)(1U << best_pin));
    }

    return mask;
}

static void evaluate_last_result(uint8_t frame_before, uint8_t throw_before, uint8_t knocked)
{
    if ((throw_before == 0U) && (knocked == 10U))
    {
        last_result = LAST_STRIKE;
        return;
    }

    if ((throw_before == 1U) &&
        ((uint8_t)(frame_rolls[frame_before][0] + knocked) == 10U))
    {
        last_result = LAST_SPARE;
        return;
    }

    if ((frame_before == 9U) && (throw_before == 2U) && (knocked == 10U))
    {
        last_result = LAST_STRIKE;
        return;
    }

    last_result = LAST_OPEN;
}

static void advance_turn(void)
{
    if (frame_index < 9U)
    {
        if ((throw_index == 0U) && (last_knocked == 10U))
        {
            frame_index++;
            throw_index = 0U;
            reset_round_pins();
            return;
        }

        if (throw_index == 0U)
        {
            throw_index = 1U;
            pins_remaining = (uint8_t)(10U - last_knocked);
            return;
        }

        frame_index++;
        throw_index = 0U;
        reset_round_pins();

        if (frame_index >= 10U)
        {
            game_complete = 1U;
        }

        return;
    }

    if (throw_index == 0U)
    {
        throw_index = 1U;

        if (last_knocked == 10U)
        {
            reset_round_pins();
        }
        else
        {
            pins_remaining = (uint8_t)(10U - last_knocked);
        }

        return;
    }

    if (throw_index == 1U)
    {
        uint8_t first;
        uint8_t second;

        first = frame_rolls[9][0];
        second = frame_rolls[9][1];

        if (first == 10U)
        {
            throw_index = 2U;

            if (second == 10U)
            {
                reset_round_pins();
            }
            else
            {
                pins_remaining = (uint8_t)(10U - second);
            }

            return;
        }

        if ((uint8_t)(first + second) == 10U)
        {
            throw_index = 2U;
            reset_round_pins();
            return;
        }

        game_complete = 1U;
        return;
    }

    game_complete = 1U;
}

static void apply_roll(void)
{
    uint8_t frame_before;
    uint8_t throw_before;

    frame_before = frame_index;
    throw_before = throw_index;

    if (pending_knocked > pins_remaining)
    {
        pending_knocked = pins_remaining;
    }

    frame_rolls[frame_before][throw_before] = pending_knocked;

    if (frame_roll_count[frame_before] < (uint8_t)(throw_before + 1U))
    {
        frame_roll_count[frame_before] = (uint8_t)(throw_before + 1U);
    }

    BowlingScore_Roll(&score_state, pending_knocked);

    standing_mask = (uint16_t)(standing_mask & (uint16_t)(~pending_fall_mask));
    if (pending_knocked >= pins_remaining)
    {
        pins_remaining = 0U;
    }
    else
    {
        pins_remaining = (uint8_t)(pins_remaining - pending_knocked);
    }

    last_knocked = pending_knocked;
    evaluate_last_result(frame_before, throw_before, pending_knocked);

    recalculate_cumulative();

    advance_turn();

    if (BowlingScore_IsComplete(&score_state) != 0U)
    {
        game_complete = 1U;
    }
}

static void start_new_game(void)
{
    uint8_t i;
    uint8_t j;

    BowlingScore_Reset(&score_state);

    for (i = 0U; i < 10U; i++)
    {
        frame_roll_count[i] = 0U;
        frame_total_valid[i] = 0U;
        frame_cumulative[i] = 0U;

        for (j = 0U; j < 3U; j++)
        {
            frame_rolls[i][j] = 0U;
        }
    }

    frame_index = 0U;
    throw_index = 0U;
    aim_index = 2U;
    power_level = 3U;
    last_knocked = 0U;
    last_result = LAST_OPEN;
    game_complete = 0U;

    reset_round_pins();

    ball_x = 64;
    ball_y = 56;
    ball_target_x = 64;

    pending_knocked = 0U;
    pending_fall_mask = 0U;

    recalculate_cumulative();

    screen = BOWLING_SCREEN_PLAY;
    screen_entered_at = HAL_GetTick();
}

static void draw_pin(int16_t x, int16_t y)
{
    DrawLine((uint8_t)x, (uint8_t)y, (uint8_t)(x - 2), (uint8_t)(y + 5));
    DrawLine((uint8_t)x, (uint8_t)y, (uint8_t)(x + 2), (uint8_t)(y + 5));
    DrawLine((uint8_t)(x - 2), (uint8_t)(y + 5), (uint8_t)(x + 2), (uint8_t)(y + 5));
}

static void draw_lane(void)
{
    uint8_t i;

    DrawRectangle(12, 12, 104, 48);

    for (i = 0U; i < 5U; i++)
    {
        uint8_t x;
        x = (uint8_t)(32U + (i * 16U));
        DrawLine(x, 12, x, 60);
    }
}

static void draw_pins(void)
{
    uint8_t i;

    for (i = 0U; i < 10U; i++)
    {
        if ((standing_mask & (uint16_t)(1U << i)) != 0U)
        {
            draw_pin(pin_px[i], pin_py[i]);
        }
    }
}

static void draw_hud(void)
{
    draw_text5x7(1, 1, "BOWLING");
    draw_text5x7(1, 9, "F");
    draw_u8(7, 9, (uint16_t)(frame_index + 1U), 2U);
    draw_text5x7(22, 9, "T");
    draw_u8(28, 9, (uint16_t)(throw_index + 1U), 1U);

    draw_text5x7(38, 9, "P");
    draw_u8(44, 9, pins_remaining, 2U);

    draw_text5x7(58, 9, "TOT");
    draw_u8(80, 9, BowlingScore_Total(&score_state), 3U);
}

static void draw_controls(void)
{
    draw_text5x7(1, 56, "A/D AIM 2/8 POW");
    draw_text5x7(1, 48, "5/# ROLL B SUM *RST");
}

static void draw_last_result(void)
{
    draw_text5x7(1, 40, "K");
    draw_u8(7, 40, last_knocked, 2U);

    if (last_result == LAST_STRIKE)
    {
        draw_text5x7(24, 40, "STRIKE");
    }
    else if (last_result == LAST_SPARE)
    {
        draw_text5x7(24, 40, "SPARE");
    }
    else
    {
        draw_text5x7(24, 40, "OPEN");
    }
}

static void draw_aim_indicator(void)
{
    int16_t x;

    x = (int16_t)(64 + ((int16_t)aim_index - 2) * 8);

    DrawLine((uint8_t)x, 60, (uint8_t)(x - 2), 62);
    DrawLine((uint8_t)x, 60, (uint8_t)(x + 2), 62);
    DrawLine((uint8_t)(x - 2), 62, (uint8_t)(x + 2), 62);

    draw_text5x7(95, 56, "PW");
    draw_u8(109, 56, power_level, 1U);
}

static void draw_summary_screen(void)
{
    uint8_t frame;

    draw_text5x7(1, 1, "RESUMEN FRAMES");

    for (frame = 0U; frame < 5U; frame++)
    {
        uint8_t x;
        uint8_t y;

        x = (uint8_t)(1U + frame * 25U);
        y = 18U;

        draw_u8(x, y, (uint16_t)(frame + 1U), 2U);
        draw_text5x7((uint8_t)(x + 12U), y, ":");

        if (frame_total_valid[frame] != 0U)
        {
            draw_u8((uint8_t)(x + 17U), y, frame_cumulative[frame], 3U);
        }
        else
        {
            draw_text5x7((uint8_t)(x + 17U), y, "---");
        }
    }

    for (frame = 5U; frame < 10U; frame++)
    {
        uint8_t x;
        uint8_t y;

        x = (uint8_t)(1U + (frame - 5U) * 25U);
        y = 32U;

        draw_u8(x, y, (uint16_t)(frame + 1U), 2U);
        draw_text5x7((uint8_t)(x + 12U), y, ":");

        if (frame_total_valid[frame] != 0U)
        {
            draw_u8((uint8_t)(x + 17U), y, frame_cumulative[frame], 3U);
        }
        else
        {
            draw_text5x7((uint8_t)(x + 17U), y, "---");
        }
    }

    draw_text5x7(1, 48, "TOTAL");
    draw_u8(37, 48, BowlingScore_Total(&score_state), 3U);
    draw_text5x7(1, 56, "B/# VOLVER * REIN");
}

static void draw_welcome_screen(void)
{
    draw_text5x7(14, 10, "T11 BOWLING");
    draw_text5x7(1, 24, "A/D MOV 2/8 POT");
    draw_text5x7(1, 32, "5/# LANZAR");
    draw_text5x7(1, 40, "B RESUMEN");
    draw_text5x7(1, 48, "* REINICIAR");
    draw_text5x7(1, 56, "5 O # INICIAR");
}

static void draw_final_screen(void)
{
    draw_text5x7(18, 14, "PARTIDA FINAL");
    draw_text5x7(18, 28, "TOTAL");
    draw_u8(56, 28, BowlingScore_Total(&score_state), 3U);
    draw_text5x7(1, 48, "B VER FRAMES");
    draw_text5x7(1, 56, "* O 5 REINICIAR");
}

static void start_roll_animation(uint32_t now)
{
    int16_t x_offset;

    pending_knocked = calculate_knocked_pins();
    pending_fall_mask = select_fallen_mask(pending_knocked);

    x_offset = (int16_t)(((int16_t)aim_index - 2) * 4);

    ball_x = (int16_t)(64 + x_offset);
    ball_y = 56;
    ball_target_x = (int16_t)(64 + x_offset);

    last_anim_step = now;
    screen = BOWLING_SCREEN_ROLLING;
    screen_entered_at = now;
}

void Bowling_GPIO_Init(void)
{
}

void Bowling_Init(void)
{
    rng_state = 0x00C0FFEEUL;
    last_render = 0U;
    screen = BOWLING_SCREEN_WELCOME;
    screen_entered_at = HAL_GetTick();

    start_new_game();
    screen = BOWLING_SCREEN_WELCOME;
}

void Bowling_Update(uint32_t now)
{
    char key;

    key = teclado();

    if (key == '*')
    {
        start_new_game();
        screen = BOWLING_SCREEN_WELCOME;
        return;
    }

    if (screen == BOWLING_SCREEN_WELCOME)
    {
        if ((key == '5') || (key == '#'))
        {
            start_new_game();
        }

        return;
    }

    if (screen == BOWLING_SCREEN_SUMMARY)
    {
        if ((key == 'B') || (key == '#') || (key == '5'))
        {
            if (game_complete != 0U)
            {
                screen = BOWLING_SCREEN_FINAL;
            }
            else
            {
                screen = BOWLING_SCREEN_PLAY;
            }

            screen_entered_at = now;
        }

        return;
    }

    if (screen == BOWLING_SCREEN_FINAL)
    {
        if ((key == '5') || (key == '#'))
        {
            start_new_game();
            return;
        }

        if (key == 'B')
        {
            screen = BOWLING_SCREEN_SUMMARY;
            screen_entered_at = now;
        }

        return;
    }

    if (screen == BOWLING_SCREEN_RESULT)
    {
        if (key == 'B')
        {
            screen = BOWLING_SCREEN_SUMMARY;
            screen_entered_at = now;
            return;
        }

        if ((now - screen_entered_at) >= BOWLING_RESULT_HOLD_MS ||
            (key == '5') || (key == '#'))
        {
            if (game_complete != 0U)
            {
                screen = BOWLING_SCREEN_FINAL;
            }
            else
            {
                screen = BOWLING_SCREEN_PLAY;
            }

            screen_entered_at = now;
        }

        return;
    }

    if (screen == BOWLING_SCREEN_ROLLING)
    {
        if ((now - last_anim_step) >= BOWLING_ANIM_STEP_MS)
        {
            last_anim_step = now;

            if (ball_y > 20)
            {
                ball_y = (int16_t)(ball_y - 3);

                if (ball_x < ball_target_x)
                {
                    ball_x++;
                }
                else if (ball_x > ball_target_x)
                {
                    ball_x--;
                }
            }
            else
            {
                apply_roll();
                screen = BOWLING_SCREEN_RESULT;
                screen_entered_at = now;
            }
        }

        return;
    }

    if (screen == BOWLING_SCREEN_PLAY)
    {
        if (key == 'A' || key == '4')
        {
            if (aim_index > 0U)
            {
                aim_index--;
            }

            return;
        }

        if (key == 'D' || key == '6')
        {
            if (aim_index < 4U)
            {
                aim_index++;
            }

            return;
        }

        if (key == '2')
        {
            if (power_level < 5U)
            {
                power_level++;
            }

            return;
        }

        if (key == '8')
        {
            if (power_level > 1U)
            {
                power_level--;
            }

            return;
        }

        if (key == 'B')
        {
            screen = BOWLING_SCREEN_SUMMARY;
            screen_entered_at = now;
            return;
        }

        if ((key == '5') || (key == '#'))
        {
            start_roll_animation(now);
        }
    }
}

void Bowling_Render(uint32_t now)
{
    if ((now - last_render) < BOWLING_RENDER_PERIOD_MS)
    {
        return;
    }

    last_render = now;

    ST7920_ClearBuffer();

    if (screen == BOWLING_SCREEN_WELCOME)
    {
        draw_welcome_screen();
    }
    else if (screen == BOWLING_SCREEN_SUMMARY)
    {
        draw_summary_screen();
    }
    else if (screen == BOWLING_SCREEN_FINAL)
    {
        draw_final_screen();
    }
    else
    {
        draw_hud();
        draw_lane();
        draw_pins();
        draw_last_result();
        draw_controls();

        if ((screen == BOWLING_SCREEN_PLAY) || (screen == BOWLING_SCREEN_RESULT))
        {
            draw_aim_indicator();
            DrawFilledCircle((int16_t)(64 + ((int16_t)aim_index - 2) * 4), 56, 2);
        }
        else if (screen == BOWLING_SCREEN_ROLLING)
        {
            DrawFilledCircle(ball_x, ball_y, 2);
        }
    }

    ST7920_Update();
}
