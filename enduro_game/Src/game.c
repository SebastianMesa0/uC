#include "game.h"

#include "ST7920_Parallel.h"
#include "teclado.h"

#include <stdint.h>

#define HORIZON_Y                  GAME_HORIZON_Y
#define PLAYFIELD_BOTTOM           GAME_PLAY_BOTTOM
#define HUD_TOP                    GAME_HUD_TOP

#define MAX_OBSTACLES              5U

#define UPDATE_PERIOD_MS           32U
#define RENDER_PERIOD_MS           38U

#define KEY_REPEAT_DELAY_MS        140U
#define KEY_REPEAT_PERIOD_MS       28U

#define COLLISION_IMMUNITY_MS      650U
#define IMPACT_FLASH_MS            180U

#define SCORE_PER_PASS             50U
#define LEVEL2_SCORE               GAME_LEVEL_2_SCORE
#define LEVEL3_SCORE               GAME_LEVEL_3_SCORE
#define WIN_SCORE                  GAME_WIN_SCORE


#define CURVE_CHANGE_PERIOD        8000U
#define CURVE_STEP_PERIOD          85U

#define DAY_NIGHT_PERIOD           30000U
#define NIGHT_START                21000U

#define BUZZER_STEP_MS             22U
#define BUZZER_SCORE_PATTERN       0x01U
#define BUZZER_LEVEL_PATTERN       0x15U
#define BUZZER_HIT_PATTERN         0x55U
#define BUZZER_WIN_PATTERN         0x35U


#define VEHICLE_FAR_WIDTH          8U
#define VEHICLE_FAR_HEIGHT         4U

#define VEHICLE_MID_WIDTH          9U
#define VEHICLE_MID_HEIGHT         4U

#define VEHICLE_NEAR_WIDTH         10U
#define VEHICLE_NEAR_HEIGHT        6U

#define VEHICLE_CLOSE_WIDTH        11U
#define VEHICLE_CLOSE_HEIGHT       7U


typedef enum
{
    GAME_MENU = 0U,
    GAME_RUNNING,
    GAME_PAUSED,
    GAME_LEVEL_MESSAGE,
    GAME_OVER,
    GAME_WIN
} GameState_t;


typedef struct
{
    uint8_t lane;
    uint8_t depth;
    int8_t drift;
    uint8_t active;
} Obstacle_t;


static GameState_t game_state;

static int16_t player_x;

static uint8_t lives;
static uint8_t level;

static uint16_t score;
static uint16_t cars_passed;

static uint32_t last_update;
static uint32_t last_render;
static uint32_t level_message_start;

static char held_key;
static uint32_t key_pressed_at;
static uint32_t key_repeat_at;

static uint32_t random_seed;

static Obstacle_t obstacles[MAX_OBSTACLES];

/* Estado de la curva */
static int8_t curve_amplitude;
static uint8_t curve_period;
static uint8_t curve_phase;
static uint32_t curve_change_at;
static uint32_t last_curve_step;

/* Estados auxiliares */
static uint32_t collision_ignore_until;
static uint32_t impact_flash_until;

static uint32_t boost_until;
static uint32_t brake_until;

/* Estado del buzzer */
static uint16_t buzzer_pattern;
static uint8_t buzzer_steps;
static uint8_t buzzer_step;
static uint8_t buzzer_active;
static uint32_t buzzer_next_ms;


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

static const uint8_t font_colon[7] =
{
    0x00,
    0x04,
    0x04,
    0x00,
    0x04,
    0x04,
    0x00
};


static const uint16_t vehicle_sprite[GAME_VEHICLE_HEIGHT] =
{
    0x088U,
    0x1DCU,
    0x3FEU,
    0x7FFU,
    0x6DBU,
    0x7FFU,
    0x306U
};


static const int8_t sine32[32] =
{
      0,  25,  49,  71,
     90, 106, 118, 125,
    127, 125, 118, 106,
     90,  71,  49,  25,
      0, -25, -49, -71,
    -90,-106,-118,-125,
   -127,-125,-118,-106,
    -90, -71, -49, -25
};


static uint32_t random_number(void);
static void clear_graphics(void);
static void draw_safe_pixel(int16_t x, int16_t y);

static const uint8_t *get_glyph(char c);
static void draw_char5x7(uint8_t x, uint8_t y, char c);
static void draw_text5x7(
    uint8_t x,
    uint8_t y,
    const char *text
);
static void draw_char_scaled(
    uint8_t x,
    uint8_t y,
    char c,
    uint8_t scale
);
static void draw_text_scaled(
    uint8_t x,
    uint8_t y,
    const char *text,
    uint8_t scale
);
static void draw_number(
    uint8_t x,
    uint8_t y,
    uint16_t number,
    uint8_t digits
);

static void update_life_leds(void);
static void buzzer_trigger(
    uint16_t pattern,
    uint8_t steps,
    uint32_t now
);
static void buzzer_service(uint32_t now);

static int16_t interpolate_i16(
    int16_t start,
    int16_t end,
    uint8_t amount
);
static uint8_t get_current_speed(uint32_t now);
static void choose_curve(uint32_t now);
static int16_t curve_offset_for_y(
    uint8_t y,
    uint32_t now
);
static uint8_t road_left_at(
    uint8_t y,
    uint32_t now
);
static uint8_t road_right_at(
    uint8_t y,
    uint32_t now
);
static uint8_t lane_center_at(
    uint8_t lane,
    uint8_t y,
    uint32_t now
);

static uint8_t obstacle_count(void);
static void spawn_obstacle(
    uint8_t index,
    uint8_t initial
);
static void reset_obstacles(void);
static uint8_t depth_to_y(uint8_t depth);

static void vehicle_dimensions_for_depth(
    uint8_t depth,
    uint8_t *width,
    uint8_t *height
);

static void reset_game(uint32_t now);
static char read_raw_key(void);
static char poll_key(uint32_t now);
static uint8_t is_night(uint32_t now);

static void draw_menu(void);
static void draw_pause(void);
static void draw_level_message(void);
static void draw_game_over(void);
static void draw_win(void);

static void draw_sky(uint32_t now);
static void draw_road(uint32_t now);

static void draw_vehicle_sprite_scaled(
    int16_t x,
    int16_t y,
    uint8_t width,
    uint8_t height,
    uint8_t night
);
static void draw_vehicle_sprite(
    int16_t x,
    int16_t y,
    uint8_t night
);
static void draw_main_car(
    int16_t x,
    int16_t y
);
static void draw_traffic_car(
    const Obstacle_t *ob,
    uint32_t now
);
static void draw_player_vehicle(uint32_t now);
static void draw_hud(void);
static void draw_scene(uint32_t now);

static void update_player_position(
    char key,
    uint32_t now
);
static void update_obstacles(uint32_t now);
static void check_collision(uint32_t now);

/* =========================================================
 * Utilidades
 * ========================================================= */
static uint32_t random_number(void)
{
    random_seed =
        random_seed * 1664525UL +
        1013904223UL;

    return random_seed;
}

static void clear_graphics(void)
{
    ST7920_GraphicMode(1);
    ST7920_ClearBuffer();
}

static void draw_safe_pixel(int16_t x, int16_t y)
{
    if (x < 0 ||
        x >= (int16_t)GAME_WIDTH ||
        y < 0 ||
        y >= (int16_t)GAME_HEIGHT)
    {
        return;
    }

    SetPixel((uint8_t)x, (uint8_t)y);
}


static const uint8_t *get_glyph(char c)
{
    if (c >= '0' && c <= '9')
    {
        return font_digit[(uint8_t)(c - '0')];
    }

    if (c >= 'A' && c <= 'Z')
    {
        return font_letter[(uint8_t)(c - 'A')];
    }

    if (c == ':')
    {
        return font_colon;
    }

    return (const uint8_t *)0;
}

static void draw_char5x7(
    uint8_t x,
    uint8_t y,
    char c
)
{
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;

    glyph = get_glyph(c);

    if (glyph == (const uint8_t *)0)
    {
        return;
    }

    for (row = 0U; row < 7U; row++)
    {
        for (col = 0U; col < 5U; col++)
        {
            if ((glyph[row] &
                 (1U << (4U - col))) != 0U)
            {
                draw_safe_pixel(
                    (int16_t)x + col,
                    (int16_t)y + row
                );
            }
        }
    }
}

static void draw_text5x7(
    uint8_t x,
    uint8_t y,
    const char *text
)
{
    if (text == (const char *)0)
    {
        return;
    }

    while (*text != '\0')
    {
        if (*text != ' ')
        {
            draw_char5x7(x, y, *text);
        }

        x = (uint8_t)(x + 6U);
        text++;
    }
}

static void draw_char_scaled(
    uint8_t x,
    uint8_t y,
    char c,
    uint8_t scale
)
{
    const uint8_t *glyph;
    uint8_t row;
    uint8_t col;
    uint8_t sx;
    uint8_t sy;

    glyph = get_glyph(c);

    if (glyph == (const uint8_t *)0 ||
        scale == 0U)
    {
        return;
    }

    for (row = 0U; row < 7U; row++)
    {
        for (col = 0U; col < 5U; col++)
        {
            if ((glyph[row] &
                 (1U << (4U - col))) != 0U)
            {
                for (sy = 0U; sy < scale; sy++)
                {
                    for (sx = 0U; sx < scale; sx++)
                    {
                        draw_safe_pixel(
                            (int16_t)x +
                            ((int16_t)col * scale) +
                            sx,
                            (int16_t)y +
                            ((int16_t)row * scale) +
                            sy
                        );
                    }
                }
            }
        }
    }
}

static void draw_text_scaled(
    uint8_t x,
    uint8_t y,
    const char *text,
    uint8_t scale
)
{
    uint8_t advance;

    if (text == (const char *)0 ||
        scale == 0U)
    {
        return;
    }

    advance = (uint8_t)(6U * scale);

    while (*text != '\0')
    {
        if (*text != ' ')
        {
            draw_char_scaled(
                x,
                y,
                *text,
                scale
            );
        }

        x = (uint8_t)(x + advance);
        text++;
    }
}

static void draw_number(
    uint8_t x,
    uint8_t y,
    uint16_t number,
    uint8_t digits
)
{
    char buffer[7];
    uint8_t i;

    if (digits == 0U)
    {
        return;
    }

    if (digits > 6U)
    {
        digits = 6U;
    }

    for (i = 0U; i < digits; i++)
    {
        buffer[digits - 1U - i] =
            (char)('0' + (number % 10U));

        number /= 10U;
    }

    buffer[digits] = '\0';

    draw_text5x7(
        x,
        y,
        buffer
    );
}


static void update_life_leds(void)
{
    /*
     * Los LEDs están conectados entre 3.3 V y el pin.
     *
     * GPIO_PIN_SET   = LED apagado.
     * GPIO_PIN_RESET = LED encendido.
     */

    HAL_GPIO_WritePin(
        LIFE1_PORT,
        LIFE1_PIN,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LIFE2_PORT,
        LIFE2_PIN | LIFE3_PIN,
        GPIO_PIN_SET
    );

    if (lives >= 1U)
    {
        HAL_GPIO_WritePin(
            LIFE1_PORT,
            LIFE1_PIN,
            GPIO_PIN_RESET
        );
    }

    if (lives >= 2U)
    {
        HAL_GPIO_WritePin(
            LIFE2_PORT,
            LIFE2_PIN,
            GPIO_PIN_RESET
        );
    }

    if (lives >= 3U)
    {
        HAL_GPIO_WritePin(
            LIFE3_PORT,
            LIFE3_PIN,
            GPIO_PIN_RESET
        );
    }
}

static void buzzer_trigger(
    uint16_t pattern,
    uint8_t steps,
    uint32_t now
)
{
    if (steps == 0U)
    {
        buzzer_active = 0U;

        HAL_GPIO_WritePin(
            GAME_BUZZER_PORT,
            GAME_BUZZER_PIN,
            GPIO_PIN_RESET
        );

        return;
    }

    buzzer_pattern = pattern;
    buzzer_steps = steps;
    buzzer_step = 0U;
    buzzer_active = 1U;
    buzzer_next_ms = now + BUZZER_STEP_MS;

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        (pattern & 1U) != 0U
            ? GPIO_PIN_SET
            : GPIO_PIN_RESET
    );
}

static void buzzer_service(uint32_t now)
{
    uint8_t state;

    if (buzzer_active == 0U)
    {
        return;
    }

    if ((int32_t)(now - buzzer_next_ms) < 0)
    {
        return;
    }

    buzzer_step++;

    if (buzzer_step >= buzzer_steps)
    {
        buzzer_active = 0U;

        HAL_GPIO_WritePin(
            GAME_BUZZER_PORT,
            GAME_BUZZER_PIN,
            GPIO_PIN_RESET
        );

        return;
    }

    state =
        (uint8_t)(
            (buzzer_pattern >> buzzer_step) & 1U
        );

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        state != 0U
            ? GPIO_PIN_SET
            : GPIO_PIN_RESET
    );

    buzzer_next_ms += BUZZER_STEP_MS;
}


static int16_t interpolate_i16(
    int16_t start,
    int16_t end,
    uint8_t amount
)
{
    return (int16_t)(
        start +
        (((end - start) *
          (int16_t)amount) / 100)
    );
}

static uint8_t get_current_speed(uint32_t now)
{
    uint8_t speed;

    if (level == GAME_LEVEL_1)
    {
        speed = 1U;
    }
    else if (level == GAME_LEVEL_2)
    {
        speed = 2U;
    }
    else
    {
        speed = 3U;
    }

    if ((int32_t)(boost_until - now) > 0)
    {
        speed++;
    }

    if ((int32_t)(brake_until - now) > 0)
    {
        if (speed > 1U)
        {
            speed--;
        }
    }

    if (speed > 4U)
    {
        speed = 4U;
    }

    return speed;
}

static void choose_curve(uint32_t now)
{
    uint8_t type;

    type =
        (uint8_t)(random_number() % 100U);

    if (type < 40U)
    {
        curve_amplitude = 1;
        curve_period = 58U;
    }
    else if (type < 75U)
    {
        curve_amplitude = 2;
        curve_period = 50U;
    }
    else if (type < 95U)
    {
        curve_amplitude = 3;
        curve_period = 42U;
    }
    else
    {
        curve_amplitude = 4;
        curve_period = 36U;
    }

    curve_phase =
        (uint8_t)(random_number() & 31U);

    curve_change_at =
        now + CURVE_CHANGE_PERIOD;
}

static int16_t curve_offset_for_y(
    uint8_t y,
    uint32_t now
)
{
    uint8_t index;
    uint32_t segment;
    int16_t depth_factor;
    int16_t value;

    if (curve_change_at == 0U ||
        (int32_t)(now - curve_change_at) >= 0)
    {
        choose_curve(now);
    }

    if (y <= HORIZON_Y)
    {
        depth_factor = 15;
    }
    else
    {
        depth_factor =
            (int16_t)(
                15 +
                ((y - HORIZON_Y) * 85) /
                (PLAYFIELD_BOTTOM - HORIZON_Y)
            );
    }

    segment =
        ((uint32_t)(y - HORIZON_Y) * 32U) /
        (curve_period == 0U ? 1U : curve_period);

    index =
        (uint8_t)(
            curve_phase +
            (uint8_t)segment
        );

    index &= 31U;

    value =
        (int16_t)(
            sine32[index] *
            curve_amplitude /
            127
        );

    return (int16_t)(
        value *
        depth_factor /
        100
    );
}

static uint8_t road_left_at(
    uint8_t y,
    uint32_t now
)
{
    uint8_t t;
    int16_t half_width;
    int16_t result;

    if (y <= HORIZON_Y)
    {
        t = 0U;
    }
    else
    {
        t = (uint8_t)(
            ((uint16_t)(y - HORIZON_Y) * 100U) /
            (PLAYFIELD_BOTTOM - HORIZON_Y)
        );
    }

    half_width =
        interpolate_i16(9, 57, t);

    result =
        (int16_t)(
            64 +
            curve_offset_for_y(y, now) -
            half_width
        );

    if (result < 0)
    {
        result = 0;
    }

    if (result > 127)
    {
        result = 127;
    }

    return (uint8_t)result;
}

static uint8_t road_right_at(
    uint8_t y,
    uint32_t now
)
{
    uint8_t t;
    int16_t half_width;
    int16_t result;

    if (y <= HORIZON_Y)
    {
        t = 0U;
    }
    else
    {
        t = (uint8_t)(
            ((uint16_t)(y - HORIZON_Y) * 100U) /
            (PLAYFIELD_BOTTOM - HORIZON_Y)
        );
    }

    half_width =
        interpolate_i16(9, 57, t);

    result =
        (int16_t)(
            64 +
            curve_offset_for_y(y, now) +
            half_width
        );

    if (result < 0)
    {
        result = 0;
    }

    if (result > 127)
    {
        result = 127;
    }

    return (uint8_t)result;
}

static uint8_t lane_center_at(
    uint8_t lane,
    uint8_t y,
    uint32_t now
)
{
    uint8_t left;
    uint8_t right;
    uint8_t width;

    left = road_left_at(y, now);
    right = road_right_at(y, now);

    if (right <= left)
    {
        return 64U;
    }

    width =
        (uint8_t)(right - left);

    if (lane == 0U)
    {
        return (uint8_t)(
            left + width / 6U
        );
    }

    if (lane == 1U)
    {
        return (uint8_t)(
            left + width / 2U
        );
    }

    return (uint8_t)(
        left + (width * 5U) / 6U
    );
}


static uint8_t obstacle_count(void)
{
    if (level == GAME_LEVEL_1)
    {
        return 3U;
    }

    if (level == GAME_LEVEL_2)
    {
        return 4U;
    }

    return MAX_OBSTACLES;
}

static void spawn_obstacle(
    uint8_t index,
    uint8_t initial
)
{
    if (index >= MAX_OBSTACLES)
    {
        return;
    }

    obstacles[index].lane =
        (uint8_t)(random_number() % 3U);

    if (initial != 0U)
    {
        obstacles[index].depth =
            (uint8_t)(
                4U +
                index * 19U +
                (random_number() % 8U)
            );
    }
    else
    {
        obstacles[index].depth =
            (uint8_t)(random_number() % 10U);
    }

    if (obstacles[index].depth >= 96U)
    {
        obstacles[index].depth = 86U;
    }

    obstacles[index].drift =
        (int8_t)((random_number() % 3U) - 1);

    obstacles[index].active = 1U;
}

static void reset_obstacles(void)
{
    uint8_t i;
    uint8_t count;

    count = obstacle_count();

    for (i = 0U; i < MAX_OBSTACLES; i++)
    {
        if (i < count)
        {
            spawn_obstacle(i, 1U);
        }
        else
        {
            obstacles[i].active = 0U;
        }
    }
}

static uint8_t depth_to_y(uint8_t depth)
{
    if (depth == 0U)
    {
        return HORIZON_Y;
    }

    if (depth >= 100U)
    {
        return PLAYFIELD_BOTTOM;
    }

    return (uint8_t)(
        HORIZON_Y +
        (
            (uint16_t)depth *
            (PLAYFIELD_BOTTOM - HORIZON_Y)
        ) / 100U
    );
}

static void vehicle_dimensions_for_depth(
    uint8_t depth,
    uint8_t *width,
    uint8_t *height
)
{
    if (width == (uint8_t *)0 ||
        height == (uint8_t *)0)
    {
        return;
    }

    if (depth < 25U)
    {
        *width = VEHICLE_FAR_WIDTH;
        *height = VEHICLE_FAR_HEIGHT;
    }
    else if (depth < 55U)
    {
        *width = VEHICLE_MID_WIDTH;
        *height = VEHICLE_MID_HEIGHT;
    }
    else if (depth < 80U)
    {
        *width = VEHICLE_NEAR_WIDTH;
        *height = VEHICLE_NEAR_HEIGHT;
    }
    else
    {
        *width = VEHICLE_CLOSE_WIDTH;
        *height = VEHICLE_CLOSE_HEIGHT;
    }
}


static void reset_game(uint32_t now)
{
    game_state = GAME_RUNNING;

    player_x = GAME_PLAYER_START_X;

    lives = GAME_INITIAL_LIVES;
    level = GAME_LEVEL_1;

    score = 0U;
    cars_passed = 0U;

    last_update = now;
    last_render = 0U;
    level_message_start = 0U;

    held_key = '\0';
    key_pressed_at = 0U;
    key_repeat_at = 0U;

    collision_ignore_until = 0U;
    impact_flash_until = 0U;

    boost_until = 0U;
    brake_until = 0U;

    curve_amplitude = 1;
    curve_period = 58U;
    curve_phase = 0U;
    curve_change_at = 0U;
    last_curve_step = now;

    buzzer_pattern = 0U;
    buzzer_steps = 0U;
    buzzer_step = 0U;
    buzzer_active = 0U;
    buzzer_next_ms = 0U;

    reset_obstacles();
    update_life_leds();

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        GPIO_PIN_RESET
    );
}


static char read_raw_key(void)
{
    uint8_t column;

    for (column = 0U; column < 4U; column++)
    {
        HAL_GPIO_WritePin(
            GPIOE,
            TECLADO_COL_MASK,
            GPIO_PIN_RESET
        );

        if (column == 0U)
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL1_PIN,
                GPIO_PIN_SET
            );
        }
        else if (column == 1U)
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL2_PIN,
                GPIO_PIN_SET
            );
        }
        else if (column == 2U)
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL3_PIN,
                GPIO_PIN_SET
            );
        }
        else
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL4_PIN,
                GPIO_PIN_SET
            );
        }

        if (HAL_GPIO_ReadPin(
                GPIOE,
                TECLADO_ROW1_PIN
            ) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL_MASK,
                GPIO_PIN_RESET
            );

            return (column == 0U) ? '1' :
                   (column == 1U) ? '2' :
                   (column == 2U) ? '3' : 'A';
        }

        if (HAL_GPIO_ReadPin(
                GPIOF,
                TECLADO_ROW2_PIN
            ) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL_MASK,
                GPIO_PIN_RESET
            );

            return (column == 0U) ? '4' :
                   (column == 1U) ? '5' :
                   (column == 2U) ? '6' : 'B';
        }

        if (HAL_GPIO_ReadPin(
                GPIOF,
                TECLADO_ROW3_PIN
            ) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL_MASK,
                GPIO_PIN_RESET
            );

            return (column == 0U) ? '7' :
                   (column == 1U) ? '8' :
                   (column == 2U) ? '9' : 'C';
        }

        if (HAL_GPIO_ReadPin(
                GPIOF,
                TECLADO_ROW4_PIN
            ) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(
                GPIOE,
                TECLADO_COL_MASK,
                GPIO_PIN_RESET
            );

            return (column == 0U) ? '*' :
                   (column == 1U) ? '0' :
                   (column == 2U) ? '#' : 'D';
        }
    }

    HAL_GPIO_WritePin(
        GPIOE,
        TECLADO_COL_MASK,
        GPIO_PIN_RESET
    );

    return '\0';
}

static char poll_key(uint32_t now)
{
    char key;

    key = read_raw_key();

    if (key == '\0')
    {
        held_key = '\0';
        return '\0';
    }

    if (key != held_key)
    {
        held_key = key;
        key_pressed_at = now;
        key_repeat_at = now;
        return key;
    }

    if (
        (key == '4' || key == '6') &&
        (now - key_pressed_at) >= KEY_REPEAT_DELAY_MS &&
        (now - key_repeat_at) >= KEY_REPEAT_PERIOD_MS
    )
    {
        key_repeat_at = now;
        return key;
    }

    return '\0';
}

static uint8_t is_night(uint32_t now)
{
    return ((now % DAY_NIGHT_PERIOD) >= NIGHT_START)
        ? 1U
        : 0U;
}


static void draw_vehicle_sprite_scaled(
    int16_t x,
    int16_t y,
    uint8_t width,
    uint8_t height,
    uint8_t night
)
{
    uint8_t dest_x;
    uint8_t dest_y;
    uint8_t source_x;
    uint8_t source_y;


    (void)night;

    if (width == 0U ||
        height == 0U)
    {
        return;
    }

    for (dest_y = 0U; dest_y < height; dest_y++)
    {
        source_y =
            (uint8_t)(
                ((uint16_t)dest_y *
                 GAME_VEHICLE_HEIGHT) /
                height
            );

        if (source_y >= GAME_VEHICLE_HEIGHT)
        {
            source_y =
                GAME_VEHICLE_HEIGHT - 1U;
        }

        for (dest_x = 0U; dest_x < width; dest_x++)
        {
            source_x =
                (uint8_t)(
                    ((uint16_t)dest_x *
                     GAME_VEHICLE_WIDTH) /
                    width
                );

            if (source_x >= GAME_VEHICLE_WIDTH)
            {
                source_x =
                    GAME_VEHICLE_WIDTH - 1U;
            }

            if (
                (vehicle_sprite[source_y] &
                 (
                     1U <<
                     (GAME_VEHICLE_WIDTH -
                      1U -
                      source_x)
                 )) != 0U
            )
            {
                draw_safe_pixel(
                    x + dest_x,
                    y + dest_y
                );
            }
        }
    }
}

static void draw_vehicle_sprite(
    int16_t x,
    int16_t y,
    uint8_t night
)
{
    draw_vehicle_sprite_scaled(
        x,
        y,
        GAME_VEHICLE_WIDTH,
        GAME_VEHICLE_HEIGHT,
        night
    );
}


static void draw_sky(uint32_t now)
{
    uint8_t i;
    uint8_t phase;

    static const uint8_t stars_x[12] =
    {
         7U,18U,31U,43U,57U,68U,
        77U,89U,101U,113U,120U,26U
    };

    static const uint8_t stars_y[12] =
    {
         4U,8U,3U,10U,6U,2U,
         9U,5U,11U,3U,8U,13U
    };

    if (is_night(now) != 0U)
    {
        for (i = 0U; i < 12U; i++)
        {
            draw_safe_pixel(
                stars_x[i],
                stars_y[i]
            );
        }

        draw_safe_pixel(105, 5);
        draw_safe_pixel(106, 5);
        draw_safe_pixel(104, 6);
        draw_safe_pixel(106, 6);
        draw_safe_pixel(104, 7);
        draw_safe_pixel(106, 7);
        draw_safe_pixel(105, 8);
        draw_safe_pixel(106, 8);
    }
    else
    {
        draw_safe_pixel(105, 3);
        draw_safe_pixel(104, 4);
        draw_safe_pixel(105, 4);
        draw_safe_pixel(106, 4);
        draw_safe_pixel(103, 5);
        draw_safe_pixel(104, 5);
        draw_safe_pixel(105, 5);
        draw_safe_pixel(106, 5);
        draw_safe_pixel(107, 5);
        draw_safe_pixel(104, 6);
        draw_safe_pixel(105, 6);
        draw_safe_pixel(106, 6);
        draw_safe_pixel(105, 7);

        DrawLine(8U, 5U, 22U, 5U);
        DrawLine(12U, 4U, 18U, 4U);
        DrawLine(31U, 8U, 43U, 8U);
        DrawLine(35U, 7U, 41U, 7U);
    }

    phase = (uint8_t)((now / 400U) & 1U);

    DrawLine(
        0U,
        15U,
        13U,
        (uint8_t)(11U + phase)
    );

    DrawLine(
        13U,
        (uint8_t)(11U + phase),
        25U,
        15U
    );

    DrawLine(25U, 15U, 38U, 10U);
    DrawLine(38U, 10U, 52U, 16U);
    DrawLine(52U, 16U, 66U, 11U);
    DrawLine(66U, 11U, 81U, 16U);
    DrawLine(81U, 16U, 94U, 12U);
    DrawLine(94U, 12U, 108U, 15U);
    DrawLine(108U, 15U, 127U, 10U);

    DrawLine(
        0U,
        HORIZON_Y,
        127U,
        HORIZON_Y
    );
}


static void draw_road(uint32_t now)
{
    uint8_t y;
    uint8_t next_y;
    uint8_t left;
    uint8_t right;
    uint8_t left2;
    uint8_t right2;
    uint8_t dash;
    uint8_t phase;
    uint8_t center1;
    uint8_t center2;
    uint8_t dash_end;

    for (y = HORIZON_Y; y < PLAYFIELD_BOTTOM; y++)
    {
        next_y = (uint8_t)(y + 1U);

        if (next_y > PLAYFIELD_BOTTOM)
        {
            next_y = PLAYFIELD_BOTTOM;
        }

        left = road_left_at(y, now);
        right = road_right_at(y, now);
        left2 = road_left_at(next_y, now);
        right2 = road_right_at(next_y, now);

        DrawLine(
            left,
            y,
            left2,
            next_y
        );

        DrawLine(
            right,
            y,
            right2,
            next_y
        );

        if (left < 126U)
        {
            draw_safe_pixel(
                (int16_t)(left + 2U),
                y
            );
        }

        if (right > 1U)
        {
            draw_safe_pixel(
                (int16_t)(right - 2U),
                y
            );
        }
    }

    /*
     * Las marcas se mueven más lentamente que antes.
     */
    phase =
        (uint8_t)((now / 100U) % 8U);

    for (
        dash = (uint8_t)(18U + phase);
        dash < PLAYFIELD_BOTTOM;
        dash = (uint8_t)(dash + 8U)
    )
    {
        dash_end = (uint8_t)(dash + 3U);

        if (dash_end >= PLAYFIELD_BOTTOM)
        {
            dash_end =
                (uint8_t)(PLAYFIELD_BOTTOM - 1U);
        }

        center1 =
            (uint8_t)(
                (
                    road_left_at(dash, now) +
                    road_right_at(dash, now)
                ) / 2U
            );

        center2 =
            (uint8_t)(
                (
                    road_left_at(dash_end, now) +
                    road_right_at(dash_end, now)
                ) / 2U
            );

        DrawLine(
            center1,
            dash,
            center2,
            dash_end
        );
    }

    for (
        dash = (uint8_t)(20U + (now / 100U) % 7U);
        dash < PLAYFIELD_BOTTOM;
        dash = (uint8_t)(dash + 7U)
    )
    {
        left = road_left_at(dash, now);
        right = road_right_at(dash, now);

        if (left >= 5U)
        {
            draw_safe_pixel(
                (int16_t)(left - 4U),
                dash
            );

            draw_safe_pixel(
                (int16_t)(left - 3U),
                dash
            );
        }

        if (right <= 122U)
        {
            draw_safe_pixel(
                (int16_t)(right + 3U),
                dash
            );

            draw_safe_pixel(
                (int16_t)(right + 4U),
                dash
            );
        }
    }
}


static void draw_main_car(
    int16_t x,
    int16_t y
)
{
    draw_vehicle_sprite(
        x,
        y,
        0U
    );
}

static void draw_traffic_car(
    const Obstacle_t *ob,
    uint32_t now
)
{
    uint8_t y;
    uint8_t center;
    uint8_t width;
    uint8_t height;
    uint8_t night;

    int16_t center_x;
    int16_t sprite_x;
    int16_t sprite_y;

    if (ob == (const Obstacle_t *)0 ||
        ob->active == 0U)
    {
        return;
    }

    y =
        depth_to_y(ob->depth);

    center =
        lane_center_at(
            ob->lane,
            y,
            now
        );

    center_x =
        (int16_t)center +
        ob->drift;

    vehicle_dimensions_for_depth(
        ob->depth,
        &width,
        &height
    );

    sprite_x =
        center_x -
        (int16_t)(width / 2U);

    sprite_y =
        (int16_t)y -
        (int16_t)height;

    night = is_night(now);

    draw_vehicle_sprite_scaled(
        sprite_x,
        sprite_y,
        width,
        height,
        night
    );
}

static void draw_player_vehicle(uint32_t now)
{
    if (
        (int32_t)(now - collision_ignore_until) < 0 &&
        ((now / 70U) & 1U) != 0U
    )
    {
        return;
    }

    draw_main_car(
        player_x,
        GAME_PLAYER_Y
    );
}

/* =========================================================
 * HUD
 * ========================================================= */
static void draw_hud(void)
{
    DrawLine(
        0U,
        HUD_TOP,
        127U,
        HUD_TOP
    );

    draw_text5x7(1U, 56U, "S:");
    draw_number(13U, 56U, score, 4U);

    draw_text5x7(43U, 56U, "L:");
    draw_number(55U, 56U, level, 1U);

    draw_text5x7(67U, 56U, "V:");
    draw_number(79U, 56U, lives, 1U);

    draw_text5x7(91U, 56U, "P:");
    draw_number(103U, 56U, cars_passed, 3U);
}

/* =========================================================
 * Pantallas
 * ========================================================= */
static void draw_menu(void)
{
    clear_graphics();

    DrawRectangle(
        3U,
        2U,
        121U,
        61U
    );

    draw_text_scaled(
        33U,
        5U,
        "ENDURO",
        2U
    );

    DrawLine(
        14U,
        20U,
        113U,
        20U
    );

    draw_main_car(
        GAME_PLAYER_START_X,
        21
    );

    draw_text5x7(12U, 39U, "4:LEFT");
    draw_text5x7(76U, 39U, "6:RIGHT");
    draw_text5x7(13U, 47U, "8:GAS");
    draw_text5x7(70U, 47U, "2:BRAKE");
    draw_text5x7(10U, 55U, "5:START");
    draw_text5x7(58U, 55U, "*:PAUSE");
    draw_text5x7(90U, 55U, "#:MENU");

    ST7920_Update();
}

static void draw_pause(void)
{
    clear_graphics();

    DrawRectangle(
        7U,
        7U,
        114U,
        50U
    );

    draw_text_scaled(
        35U,
        11U,
        "PAUSE",
        2U
    );

    DrawLine(
        18U,
        27U,
        110U,
        27U
    );

    draw_text5x7(24U, 32U, "* RESUME");
    draw_text5x7(24U, 42U, "SCORE");
    draw_number(63U, 42U, score, 4U);
    draw_text5x7(24U, 51U, "# MENU");

    ST7920_Update();
}

static void draw_level_message(void)
{
    clear_graphics();

    DrawRectangle(
        4U,
        4U,
        120U,
        56U
    );

    draw_text_scaled(
        16U,
        8U,
        "LEVEL",
        2U
    );

    draw_char_scaled(
        82U,
        8U,
        (char)('0' + level),
        2U
    );

    DrawLine(
        15U,
        25U,
        112U,
        25U
    );

    draw_vehicle_sprite(
        59,
        29,
        0U
    );

    draw_text5x7(
        32U,
        42U,
        "FASTER"
    );

    draw_text5x7(
        18U,
        51U,
        "MORE CARS"
    );

    ST7920_Update();
}

static void draw_game_over(void)
{
    clear_graphics();

    DrawRectangle(
        3U,
        2U,
        122U,
        61U
    );

    DrawRectangle(
        7U,
        6U,
        114U,
        53U
    );

    draw_text_scaled(
        35U,
        9U,
        "GAME",
        2U
    );

    draw_text_scaled(
        35U,
        23U,
        "OVER",
        2U
    );

    DrawLine(
        16U,
        39U,
        111U,
        39U
    );

    draw_text5x7(
        19U,
        44U,
        "SCORE"
    );

    draw_number(
        60U,
        44U,
        score,
        4U
    );

    draw_text5x7(
        37U,
        53U,
        "5:RESTART"
    );

    ST7920_Update();
}

static void draw_win(void)
{
    clear_graphics();

    DrawRectangle(
        3U,
        2U,
        122U,
        61U
    );

    draw_text_scaled(
        25U,
        7U,
        "YOU WIN",
        2U
    );

    DrawLine(
        15U,
        24U,
        112U,
        24U
    );

    draw_main_car(
        GAME_PLAYER_START_X,
        27
    );

    draw_text5x7(
        20U,
        47U,
        "SCORE"
    );

    draw_number(
        61U,
        47U,
        score,
        4U
    );

    draw_text5x7(
        40U,
        56U,
        "5:NEW"
    );

    ST7920_Update();
}


static void draw_scene(uint32_t now)
{
    uint8_t i;

    clear_graphics();

    draw_sky(now);
    draw_road(now);

    for (i = 0U; i < MAX_OBSTACLES; i++)
    {
        draw_traffic_car(
            &obstacles[i],
            now
        );
    }

    draw_player_vehicle(now);
    draw_hud();

    if ((int32_t)(now - impact_flash_until) < 0)
    {
        DrawRectangle(
            1U,
            1U,
            14U,
            8U
        );

        draw_text5x7(
            4U,
            2U,
            "X"
        );
    }

    ST7920_Update();
}


static void update_player_position(
    char key,
    uint32_t now
)
{
    int16_t left;
    int16_t right;

    left =
        (int16_t)road_left_at(
            PLAYFIELD_BOTTOM,
            now
        ) + 2;

    right =
        (int16_t)road_right_at(
            PLAYFIELD_BOTTOM,
            now
        ) -
        GAME_VEHICLE_WIDTH -
        2;

    if (right < left)
    {
        left = 2;

        right =
            GAME_WIDTH -
            GAME_VEHICLE_WIDTH -
            2;
    }

    if (key == '4')
    {
        player_x -= GAME_PLAYER_STEP;
    }
    else if (key == '6')
    {
        player_x += GAME_PLAYER_STEP;
    }

    if (player_x < left)
    {
        player_x = left;
    }

    if (player_x > right)
    {
        player_x = right;
    }
}


static void update_obstacles(uint32_t now)
{
    uint8_t i;
    uint8_t count;
    uint8_t speed;

    count = obstacle_count();
    speed = get_current_speed(now);

    for (i = 0U; i < MAX_OBSTACLES; i++)
    {
        uint8_t y;
        uint8_t left;
        uint8_t right;
        uint8_t center;

        if (i >= count)
        {
            obstacles[i].active = 0U;
            continue;
        }

        if (obstacles[i].active == 0U)
        {
            spawn_obstacle(i, 0U);
            continue;
        }

        /*
         * Movimiento lateral pequeño de los obstáculos.
         */
        if (
            obstacles[i].depth > 30U &&
            (random_number() % 10U) < 3U
        )
        {
            if ((random_number() & 1U) != 0U)
            {
                obstacles[i].drift++;
            }
            else
            {
                obstacles[i].drift--;
            }

            if (obstacles[i].drift > 5)
            {
                obstacles[i].drift = 5;
            }

            if (obstacles[i].drift < -5)
            {
                obstacles[i].drift = -5;
            }
        }

        y =
            depth_to_y(
                obstacles[i].depth
            );

        left =
            road_left_at(
                y,
                now
            );

        right =
            road_right_at(
                y,
                now
            );

        center =
            lane_center_at(
                obstacles[i].lane,
                y,
                now
            );

        if (
            center < left + 3U &&
            obstacles[i].drift < 0
        )
        {
            obstacles[i].drift = 0;
        }

        if (
            center > right - 3U &&
            obstacles[i].drift > 0
        )
        {
            obstacles[i].drift = 0;
        }

        /*
         * Avance del obstáculo hacia el jugador.
         */
        if (
            (uint16_t)obstacles[i].depth +
            (uint16_t)speed >= 100U
        )
        {
            score =
                (uint16_t)(
                    score + SCORE_PER_PASS
                );

            cars_passed++;

            buzzer_trigger(
                BUZZER_SCORE_PATTERN,
                1U,
                now
            );

            if (
                level == GAME_LEVEL_1 &&
                score >= LEVEL2_SCORE
            )
            {
                level = GAME_LEVEL_2;
                game_state = GAME_LEVEL_MESSAGE;
                level_message_start = now;

                reset_obstacles();

                buzzer_trigger(
                    BUZZER_LEVEL_PATTERN,
                    5U,
                    now
                );

                return;
            }

            if (
                level == GAME_LEVEL_2 &&
                score >= LEVEL3_SCORE
            )
            {
                level = GAME_LEVEL_3;
                game_state = GAME_LEVEL_MESSAGE;
                level_message_start = now;

                reset_obstacles();

                buzzer_trigger(
                    BUZZER_LEVEL_PATTERN,
                    5U,
                    now
                );

                return;
            }

            if (
                level == GAME_LEVEL_3 &&
                score >= WIN_SCORE
            )
            {
                game_state = GAME_WIN;

                buzzer_trigger(
                    BUZZER_WIN_PATTERN,
                    6U,
                    now
                );

                return;
            }

            spawn_obstacle(
                i,
                0U
            );
        }
        else
        {
            obstacles[i].depth =
                (uint8_t)(
                    obstacles[i].depth + speed
                );
        }
    }
}


static void check_collision(uint32_t now)
{
    uint8_t i;

    int16_t player_left;
    int16_t player_right;
    int16_t player_top;
    int16_t player_bottom;

    if ((int32_t)(now - collision_ignore_until) < 0)
    {
        return;
    }

    /*
     * Hitbox ligeramente menor que el sprite.
     */
    player_left =
        player_x + 1;

    player_right =
        player_x +
        GAME_VEHICLE_WIDTH -
        2;

    player_top =
        GAME_PLAYER_Y + 1;

    player_bottom =
        GAME_PLAYER_Y +
        GAME_VEHICLE_HEIGHT -
        2;

    for (i = 0U; i < MAX_OBSTACLES; i++)
    {
        uint8_t depth;
        uint8_t y;
        uint8_t center;
        uint8_t width;
        uint8_t height;

        int16_t enemy_x;
        int16_t enemy_y;
        int16_t enemy_left;
        int16_t enemy_right;
        int16_t enemy_top;
        int16_t enemy_bottom;

        if (obstacles[i].active == 0U)
        {
            continue;
        }

        depth = obstacles[i].depth;


        if (depth < 82U)
        {
            continue;
        }

        y =
            depth_to_y(depth);

        center =
            lane_center_at(
                obstacles[i].lane,
                y,
                now
            );

        vehicle_dimensions_for_depth(
            depth,
            &width,
            &height
        );

        enemy_x =
            (int16_t)center +
            obstacles[i].drift -
            (int16_t)(width / 2U);

        enemy_y =
            (int16_t)y -
            (int16_t)height;

        enemy_left =
            enemy_x + 1;

        enemy_right =
            enemy_x +
            (int16_t)width -
            2;

        enemy_top =
            enemy_y + 1;

        enemy_bottom =
            enemy_y +
            (int16_t)height -
            2;

        if (
            player_right >= enemy_left &&
            player_left <= enemy_right &&
            player_bottom >= enemy_top &&
            player_top <= enemy_bottom
        )
        {
            if (lives > 0U)
            {
                lives--;
            }

            update_life_leds();

            impact_flash_until =
                now + IMPACT_FLASH_MS;

            collision_ignore_until =
                now + COLLISION_IMMUNITY_MS;

            buzzer_trigger(
                BUZZER_HIT_PATTERN,
                7U,
                now
            );

            player_x =
                GAME_PLAYER_START_X;

            spawn_obstacle(
                i,
                0U
            );

            if (lives == 0U)
            {
                game_state = GAME_OVER;
            }

            return;
        }
    }
}


void Game_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /*
     * Buzzer en PC8.
     */
    gpio.Pin = GAME_BUZZER_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GAME_BUZZER_PORT,
        &gpio
    );

    /*
     * LED 1 en PD2.
     */
    gpio.Pin = LIFE1_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        LIFE1_PORT,
        &gpio
    );

    /*
     * LED 2 y LED 3 en PG2 y PG3.
     */
    gpio.Pin = LIFE2_PIN | LIFE3_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GPIOG,
        &gpio
    );

    /*
     * Estado inicial:
     * buzzer apagado y LEDs apagados.
     */
    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        LIFE1_PORT,
        LIFE1_PIN,
        GPIO_PIN_SET
    );

    HAL_GPIO_WritePin(
        LIFE2_PORT,
        LIFE2_PIN | LIFE3_PIN,
        GPIO_PIN_SET
    );
}


void Game_Init(void)
{
    random_seed =
        0x12345678UL ^ HAL_GetTick();

    game_state = GAME_MENU;

    player_x =
        GAME_PLAYER_START_X;

    lives = GAME_INITIAL_LIVES;
    level = GAME_LEVEL_1;

    score = 0U;
    cars_passed = 0U;

    last_update = 0U;
    last_render = 0U;
    level_message_start = 0U;

    held_key = '\0';
    key_pressed_at = 0U;
    key_repeat_at = 0U;

    collision_ignore_until = 0U;
    impact_flash_until = 0U;

    boost_until = 0U;
    brake_until = 0U;

    curve_amplitude = 1;
    curve_period = 58U;
    curve_phase = 0U;
    curve_change_at = 0U;
    last_curve_step = 0U;

    buzzer_pattern = 0U;
    buzzer_steps = 0U;
    buzzer_step = 0U;
    buzzer_active = 0U;
    buzzer_next_ms = 0U;

    reset_obstacles();
    update_life_leds();

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        GPIO_PIN_RESET
    );
}


void Game_Update(uint32_t now)
{
    char key;

    buzzer_service(now);

    key = poll_key(now);

    if (game_state == GAME_MENU)
    {
        if (key == '5')
        {
            reset_game(now);
        }

        return;
    }

    if (
        game_state == GAME_OVER ||
        game_state == GAME_WIN
    )
    {
        if (key == '5')
        {
            reset_game(now);
        }

        return;
    }

    if (game_state == GAME_PAUSED)
    {
        if (key == '*')
        {
            game_state = GAME_RUNNING;
            last_update = now;
        }
        else if (key == '#')
        {
            game_state = GAME_MENU;
        }

        return;
    }

    if (game_state == GAME_LEVEL_MESSAGE)
    {
        if ((now - level_message_start) >= 1300U)
        {
            game_state = GAME_RUNNING;
            last_update = now;
        }

        return;
    }

    if (key == '*')
    {
        game_state = GAME_PAUSED;
        return;
    }

    if (key == '#')
    {
        game_state = GAME_MENU;
        return;
    }

    if (key == '8')
    {
        boost_until = now + 700U;
        brake_until = 0U;
    }

    if (key == '2')
    {
        brake_until = now + 450U;
        boost_until = 0U;
    }

    update_player_position(
        key,
        now
    );

    if ((now - last_update) < UPDATE_PERIOD_MS)
    {
        return;
    }

    last_update = now;

    /*
     * La fase de la carretera se actualiza más lentamente.
     */
    if ((now - last_curve_step) >= CURVE_STEP_PERIOD)
    {
        curve_phase =
            (uint8_t)(
                (curve_phase + 1U) & 31U
            );

        last_curve_step = now;
    }

    check_collision(now);

    if (game_state != GAME_RUNNING)
    {
        return;
    }

    update_obstacles(now);
}


void Game_Render(uint32_t now)
{
    if (
        last_render != 0U &&
        (now - last_render) < RENDER_PERIOD_MS
    )
    {
        return;
    }

    last_render = now;

    if (game_state == GAME_MENU)
    {
        draw_menu();
        return;
    }

    if (game_state == GAME_PAUSED)
    {
        draw_pause();
        return;
    }

    if (game_state == GAME_LEVEL_MESSAGE)
    {
        draw_level_message();
        return;
    }

    if (game_state == GAME_OVER)
    {
        draw_game_over();
        return;
    }

    if (game_state == GAME_WIN)
    {
        draw_win();
        return;
    }

    draw_scene(now);
}
