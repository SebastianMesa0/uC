#include "game.h"

#include "ST7920_Parallel.h"
#include "teclado.h"

#include <stdint.h>

/* =========================================================
 * ENDUR0 - VERSION STM32 / ST7920
 * =========================================================
 *
 * La lógica se inspira en el port en C del repositorio de
 * referencia: carretera con perspectiva, curvas variables,
 * tráfico con tres escalas, movimiento lateral de los autos,
 * ciclo día/noche y un marcador inferior. La implementación se
 * adapta al framebuffer monocromático 128x64 y al teclado 4x4.
 *
 * Referencia conceptual utilizada:
 * - carretera con coeficientes que dependen de la profundidad;
 * - tres tamaños de tráfico;
 * - línea central discontinua animada;
 * - curvas que cambian periódicamente;
 * - autos que se reciclan al salir del campo;
 * - visibilidad nocturna mediante luces.
 * =========================================================
 */

/* =========================================================
 * ZONAS DE LA PANTALLA
 * =========================================================
 */

#define HORIZON_Y                  17U
#define PLAYFIELD_BOTTOM           53U
#define HUD_TOP                    54U

/* =========================================================
 * GEOMETRIA DE LA CARRETERA
 * =========================================================
 */

#define ROAD_HORIZON_LEFT          55U
#define ROAD_HORIZON_RIGHT         73U
#define ROAD_BOTTOM_LEFT            7U
#define ROAD_BOTTOM_RIGHT         120U

/* =========================================================
 * AUTO DEL JUGADOR
 * =========================================================
 */

#define PLAYER_WIDTH               32U
#define PLAYER_HEIGHT              16U
#define PLAYER_Y                   37
#define PLAYER_START_X             48
#define PLAYER_STEP                 3

/* =========================================================
 * TEMPORIZACION
 * =========================================================
 */

#define UPDATE_PERIOD_MS            32U
#define RENDER_PERIOD_MS            38U

#define KEY_REPEAT_DELAY_MS        140U
#define KEY_REPEAT_PERIOD_MS        28U

#define COLLISION_IMMUNITY_MS      650U
#define IMPACT_FLASH_MS            180U

/* =========================================================
 * PUNTAJE Y NIVELES
 * =========================================================
 */

#define MAX_OBSTACLES                5U

#define SCORE_PER_PASS              50U
#define LEVEL2_SCORE              500U
#define LEVEL3_SCORE             1200U
#define WIN_SCORE                 1500U

/* =========================================================
 * DINAMICA DE CARRETERA
 * =========================================================
 */

#define CURVE_CHANGE_PERIOD       5000U
#define DAY_NIGHT_PERIOD         30000U
#define NIGHT_START              21000U

/* =========================================================
 * BUZZER
 * =========================================================
 */

#define BUZZER_STEP_MS             22U
#define BUZZER_SCORE_PATTERN       0x01U
#define BUZZER_LEVEL_PATTERN       0x15U
#define BUZZER_HIT_PATTERN         0x55U
#define BUZZER_WIN_PATTERN         0x35U

/* =========================================================
 * ESTADO DEL JUEGO
 * =========================================================
 */

typedef enum
{
    GAME_MENU = 0U,
    GAME_RUNNING,
    GAME_PAUSED,
    GAME_LEVEL_MESSAGE,
    GAME_OVER,
    GAME_WIN
} GameState_t;

/* =========================================================
 * VEHICULO DE TRAFICO
 * =========================================================
 *
 * depth = 0       -> horizonte
 * depth = 100     -> cerca del jugador
 * =========================================================
 */

typedef struct
{
    uint8_t lane;
    uint8_t depth;
    int8_t drift;
    uint8_t active;
} Obstacle_t;

/* =========================================================
 * VARIABLES GLOBALES DEL MODULO
 * =========================================================
 */

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

/* =========================================================
 * CURVA
 * =========================================================
 */

static int8_t curve_amplitude;
static uint8_t curve_period;
static uint8_t curve_phase;
static uint32_t curve_change_at;

/* =========================================================
 * ESTADOS AUXILIARES
 * =========================================================
 */

static uint32_t collision_ignore_until;
static uint32_t impact_flash_until;

static uint32_t boost_until;
static uint32_t brake_until;

/* =========================================================
 * BUZZER
 * =========================================================
 */

static uint16_t buzzer_pattern;
static uint8_t buzzer_steps;
static uint8_t buzzer_step;
static uint8_t buzzer_active;
static uint32_t buzzer_next_ms;

/* =========================================================
 * FUENTE 5x7
 * =========================================================
 *
 * Cada glifo contiene 7 filas y cinco bits por fila.
 * Esta fuente es utilizada en menú, HUD y pantallas de estado.
 * =========================================================
 */

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

/* =========================================================
 * ASSET PRINCIPAL 32x16
 * =========================================================
 *
 * Es el asset diseñado anteriormente para el automóvil visto
 * desde atrás. Se conserva como sprite principal sin reducción
 * para el jugador.
 * =========================================================
 */

static const uint8_t car_sprite[16][4] =
{
    {0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00},
    {0x00,0x38,0x44,0x00},
    {0x00,0x7C,0xFE,0x00},
    {0x00,0xFE,0xFF,0x80},
    {0x00,0xFE,0xFF,0x80},
    {0x00,0xFE,0xFF,0xC0},
    {0x00,0xF8,0xFF,0xC0},
    {0x00,0xF8,0xFF,0xC0},
    {0x00,0xFC,0xFF,0xC0},
    {0x00,0xFE,0xFF,0x80},
    {0x00,0xFE,0xFF,0x80},
    {0x00,0x7C,0xFE,0x00},
    {0x00,0x38,0x44,0x00},
    {0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00}
};

/* =========================================================
 * TRAFICO: TRES ESCALAS
 * =========================================================
 *
 * El port de referencia utiliza 4x2, 6x3 y 11x5 para los autos.
 * En lugar de reducir el sprite grande a tamaños extremos, se
 * dibujan patrones dedicados, conservando una silueta reconocible.
 * =========================================================
 */

static const uint8_t enemy_small[2] =
{
    0x09,
    0x0F
};

static const uint8_t enemy_medium[3] =
{
    0x21,
    0x3F,
    0x1E
};

static const uint16_t enemy_large[5] =
{
    0x6FB,
    0x070,
    0x6FB,
    0x6FB,
    0x6FB
};

/* =========================================================
 * SENO DE 32 PUNTOS
 * =========================================================
 * Valores aproximados de -127 a +127.
 * =========================================================
 */

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

/* =========================================================
 * PROTOTIPOS
 * =========================================================
 */

static uint32_t random_number(void);
static void clear_graphics(void);
static void draw_safe_pixel(int16_t x, int16_t y);

static const uint8_t *get_glyph(char c);
static void draw_char5x7(uint8_t x, uint8_t y, char c);
static void draw_text5x7(uint8_t x, uint8_t y, const char *text);
static void draw_char_scaled(uint8_t x, uint8_t y, char c, uint8_t scale);
static void draw_text_scaled(uint8_t x, uint8_t y, const char *text, uint8_t scale);
static void draw_number(uint8_t x, uint8_t y, uint16_t number, uint8_t digits);

static void update_life_leds(void);
static void buzzer_trigger(uint16_t pattern, uint8_t steps, uint32_t now);
static void buzzer_service(uint32_t now);

static int16_t interpolate_i16(int16_t start, int16_t end, uint8_t amount);
static uint8_t get_current_speed(uint32_t now);
static void choose_curve(uint32_t now);
static int16_t curve_offset_for_y(uint8_t y, uint32_t now);
static uint8_t road_left_at(uint8_t y, uint32_t now);
static uint8_t road_right_at(uint8_t y, uint32_t now);
static uint8_t lane_center_at(uint8_t lane, uint8_t y, uint32_t now);

static uint8_t obstacle_count(void);
static void spawn_obstacle(uint8_t index, uint8_t initial);
static void reset_obstacles(void);
static uint8_t depth_to_y(uint8_t depth);

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

static uint8_t sprite_pixel(uint8_t sx, uint8_t sy);
static void draw_main_car(int16_t x, int16_t y);

static void draw_enemy_small(int16_t x, int16_t y, uint8_t night);
static void draw_enemy_medium(int16_t x, int16_t y, uint8_t night);
static void draw_enemy_large(int16_t x, int16_t y, uint8_t night);

static void draw_traffic_car(const Obstacle_t *ob, uint32_t now);
static void draw_player_vehicle(uint32_t now);
static void draw_hud(void);
static void draw_scene(uint32_t now);

static void update_player_position(char key, uint32_t now);
static void update_obstacles(uint32_t now);
static void check_collision(uint32_t now);

/* =========================================================
 * RANDOM
 * =========================================================
 */

static uint32_t random_number(void)
{
    random_seed =
        random_seed * 1664525UL +
        1013904223UL;

    return random_seed;
}

/* =========================================================
 * LIMPIEZA
 * =========================================================
 */

static void clear_graphics(void)
{
    ST7920_GraphicMode(1);
    ST7920_ClearBuffer();
}

/* =========================================================
 * PIXEL SEGURO
 * =========================================================
 */

static void draw_safe_pixel(int16_t x, int16_t y)
{
    if (x < 0 || x >= (int16_t)GAME_WIDTH)
    {
        return;
    }

    if (y < 0 || y >= (int16_t)GAME_HEIGHT)
    {
        return;
    }

    SetPixel((uint8_t)x, (uint8_t)y);
}

/* =========================================================
 * FUENTE
 * =========================================================
 */

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

static void draw_char5x7(uint8_t x, uint8_t y, char c)
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
            if ((glyph[row] & (1U << (4U - col))) != 0U)
            {
                draw_safe_pixel(
                    (int16_t)x + col,
                    (int16_t)y + row
                );
            }
        }
    }
}

static void draw_text5x7(uint8_t x, uint8_t y, const char *text)
{
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

    if (glyph == (const uint8_t *)0 || scale == 0U)
    {
        return;
    }

    for (row = 0U; row < 7U; row++)
    {
        for (col = 0U; col < 5U; col++)
        {
            if ((glyph[row] & (1U << (4U - col))) != 0U)
            {
                for (sy = 0U; sy < scale; sy++)
                {
                    for (sx = 0U; sx < scale; sx++)
                    {
                        draw_safe_pixel(
                            (int16_t)x +
                            (int16_t)col * scale +
                            sx,
                            (int16_t)y +
                            (int16_t)row * scale +
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

    if (scale == 0U)
    {
        return;
    }

    advance = (uint8_t)(6U * scale);

    while (*text != '\0')
    {
        if (*text != ' ')
        {
            draw_char_scaled(x, y, *text, scale);
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

    draw_text5x7(x, y, buffer);
}

/* =========================================================
 * LEDS DE VIDAS
 * =========================================================
 */

static void update_life_leds(void)
{
    /* SET = apagado, RESET = encendido para el esquema usado. */
    HAL_GPIO_WritePin(
        GPIOF,
        LIFE1_PIN | LIFE2_PIN | LIFE3_PIN,
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

/* =========================================================
 * BUZZER NO BLOQUEANTE
 * =========================================================
 */

static void buzzer_trigger(
    uint16_t pattern,
    uint8_t steps,
    uint32_t now
)
{
    buzzer_pattern = pattern;
    buzzer_steps = steps;
    buzzer_step = 0U;
    buzzer_active = 1U;
    buzzer_next_ms = now + BUZZER_STEP_MS;

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        (pattern & 1U) ? GPIO_PIN_SET : GPIO_PIN_RESET
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
        (uint8_t)((buzzer_pattern >> buzzer_step) & 1U);

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        state ? GPIO_PIN_SET : GPIO_PIN_RESET
    );

    buzzer_next_ms += BUZZER_STEP_MS;
}

/* =========================================================
 * INTERPOLACION
 * =========================================================
 */

static int16_t interpolate_i16(
    int16_t start,
    int16_t end,
    uint8_t amount
)
{
    int16_t difference;

    difference = (int16_t)(end - start);

    return (int16_t)(
        start +
        (difference * (int16_t)amount) / 100
    );
}

/* =========================================================
 * VELOCIDAD
 * =========================================================
 */

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

/* =========================================================
 * CAMBIO DE CURVA
 * =========================================================
 */

static void choose_curve(uint32_t now)
{
    uint8_t type;

    type = (uint8_t)(random_number() % 100U);

    /* Basado en el port de referencia: curvas suaves, medias,
     * rápidas y ocasionalmente cerradas. */
    if (type < 30U)
    {
        curve_amplitude = 2;
        curve_period = 46U;
    }
    else if (type < 60U)
    {
        curve_amplitude = 4;
        curve_period = 36U;
    }
    else if (type < 90U)
    {
        curve_amplitude = 6;
        curve_period = 28U;
    }
    else
    {
        curve_amplitude = 8;
        curve_period = 22U;
    }

    curve_phase =
        (uint8_t)(random_number() & 31U);

    curve_change_at =
        now + CURVE_CHANGE_PERIOD;
}

/* =========================================================
 * OFFSET DE CURVA
 * =========================================================
 */

static int16_t curve_offset_for_y(
    uint8_t y,
    uint32_t now
)
{
    uint8_t index;
    uint32_t segment;
    int16_t depth_factor;
    int16_t value;

    if (curve_change_at == 0U)
    {
        choose_curve(now);
    }
    else if ((int32_t)(now - curve_change_at) >= 0)
    {
        choose_curve(now);
    }

    /* El movimiento es mínimo en el horizonte y mayor cerca del
     * jugador, igual que una curva de carretera en perspectiva. */
    if (y <= HORIZON_Y)
    {
        depth_factor = 15;
    }
    else
    {
        depth_factor =
            (int16_t)(
                15 +
                (y - HORIZON_Y) * 85 /
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

    value =
        (int16_t)(
            value *
            depth_factor / 100
        );

    return value;
}

/* =========================================================
 * BORDE CARRETERA
 * =========================================================
 */

static uint8_t road_left_at(
    uint8_t y,
    uint32_t now
)
{
    uint8_t t;
    int16_t half_width;
    int16_t curve;
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

    half_width = interpolate_i16(
        9,
        57,
        t
    );

    curve = curve_offset_for_y(y, now);

    result =
        (int16_t)(
            64 + curve - half_width
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
    int16_t curve;
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

    half_width = interpolate_i16(
        9,
        57,
        t
    );

    curve = curve_offset_for_y(y, now);

    result =
        (int16_t)(
            64 + curve + half_width
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

/* =========================================================
 * CENTRO DE CARRIL
 * =========================================================
 */

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

    width = (uint8_t)(right - left);

    if (lane == 0U)
    {
        return (uint8_t)(left + width / 6U);
    }

    if (lane == 1U)
    {
        return (uint8_t)(left + width / 2U);
    }

    return (uint8_t)(left + (width * 5U) / 6U);
}

/* =========================================================
 * CANTIDAD DE OBSTACULOS
 * =========================================================
 */

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

    return 5U;
}

/* =========================================================
 * CREAR OBSTACULO
 * =========================================================
 */

static void spawn_obstacle(
    uint8_t index,
    uint8_t initial
)
{
    uint8_t lane;

    if (index >= MAX_OBSTACLES)
    {
        return;
    }

    lane =
        (uint8_t)(random_number() % 3U);

    obstacles[index].lane = lane;

    if (initial != 0U)
    {
        /* Separaciones similares a la distribución irregular del
         * port de referencia. */
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

/* =========================================================
 * REINICIAR OBSTACULOS
 * =========================================================
 */

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

/* =========================================================
 * PROFUNDIDAD -> Y
 * =========================================================
 */

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

/* =========================================================
 * RESET DEL JUEGO
 * =========================================================
 */

static void reset_game(uint32_t now)
{
    game_state = GAME_RUNNING;

    player_x = PLAYER_START_X;

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

    curve_amplitude = 3;
    curve_period = 40U;
    curve_phase = 0U;
    curve_change_at = 0U;

    buzzer_pattern = 0U;
    buzzer_steps = 0U;
    buzzer_step = 0U;
    buzzer_active = 0U;
    buzzer_next_ms = 0U;

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        GPIO_PIN_RESET
    );

    reset_obstacles();
    update_life_leds();
}

/* =========================================================
 * TECLADO
 * =========================================================
 *
 * Mapa conservado:
 *
 * 1 2 3 A
 * 4 5 6 B
 * 7 8 9 C
 * * 0 # D
 *
 * 4 -> izquierda
 * 6 -> derecha
 * 8 -> acelerador
 * 2 -> freno
 * 5 -> iniciar/reiniciar
 * * -> pausa
 * # -> menú
 * =========================================================
 */

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

        if (HAL_GPIO_ReadPin(GPIOE, TECLADO_ROW1_PIN) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

            return (column == 0U) ? '1' :
                   (column == 1U) ? '2' :
                   (column == 2U) ? '3' : 'A';
        }

        if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW2_PIN) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

            return (column == 0U) ? '4' :
                   (column == 1U) ? '5' :
                   (column == 2U) ? '6' : 'B';
        }

        if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW3_PIN) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

            return (column == 0U) ? '7' :
                   (column == 1U) ? '8' :
                   (column == 2U) ? '9' : 'C';
        }

        if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW4_PIN) == GPIO_PIN_SET)
        {
            HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

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

    if (key == '4' || key == '6')
    {
        if ((now - key_pressed_at) >= KEY_REPEAT_DELAY_MS)
        {
            if ((now - key_repeat_at) >= KEY_REPEAT_PERIOD_MS)
            {
                key_repeat_at = now;
                return key;
            }
        }
    }

    return '\0';
}

/* =========================================================
 * NOCHE
 * =========================================================
 */

static uint8_t is_night(uint32_t now)
{
    uint32_t phase;

    phase = now % DAY_NIGHT_PERIOD;

    return (phase >= NIGHT_START) ? 1U : 0U;
}

/* =========================================================
 * MENU
 * =========================================================
 */

static void draw_menu(void)
{
    clear_graphics();

    /* Marco simple. */
    DrawRectangle(
        3U,
        2U,
        121U,
        61U
    );

    /* Título. */
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

    /* Auto principal. */
    draw_main_car(
        48,
        21
    );

    /* Controles en dos columnas. */
    draw_text5x7(
        12U,
        39U,
        "4:LEFT"
    );

    draw_text5x7(
        76U,
        39U,
        "6:RIGHT"
    );

    draw_text5x7(
        13U,
        47U,
        "8:GAS"
    );

    draw_text5x7(
        70U,
        47U,
        "2:BRAKE"
    );

    draw_text5x7(
        10U,
        55U,
        "5:START"
    );

    draw_text5x7(
        58U,
        55U,
        "*:PAUSE"
    );

    draw_text5x7(
        90U,
        55U,
        "#:MENU"
    );

    ST7920_Update();
}

/* =========================================================
 * PAUSA
 * =========================================================
 */

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

    draw_text5x7(
        24U,
        32U,
        "* RESUME"
    );

    draw_text5x7(
        24U,
        42U,
        "SCORE"
    );

    draw_number(
        63U,
        42U,
        score,
        4U
    );

    draw_text5x7(
        24U,
        51U,
        "# MENU"
    );

    ST7920_Update();
}

/* =========================================================
 * CAMBIO DE NIVEL
 * =========================================================
 */

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

    draw_enemy_large(
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

/* =========================================================
 * GAME OVER
 * =========================================================
 */

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

/* =========================================================
 * VICTORIA
 * =========================================================
 */

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
        48,
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

/* =========================================================
 * CIELO
 * =========================================================
 */

static void draw_sky(uint32_t now)
{
    uint8_t night;
    uint8_t phase;
    uint8_t i;

    night = is_night(now);

    if (night != 0U)
    {
        static const uint8_t stars_x[12] =
        {
             7U,18U,31U,43U,57U,68U,
            77U,89U,101U,113U,120U,26U
        };

        static const uint8_t stars_y[12] =
        {
             4U, 8U, 3U,10U, 6U, 2U,
             9U, 5U,11U, 3U, 8U,13U
        };

        for (i = 0U; i < 12U; i++)
        {
            draw_safe_pixel(
                stars_x[i],
                stars_y[i]
            );
        }

        /* Luna pixelada. */
        draw_safe_pixel(105,5);
        draw_safe_pixel(106,5);
        draw_safe_pixel(104,6);
        draw_safe_pixel(106,6);
        draw_safe_pixel(104,7);
        draw_safe_pixel(106,7);
        draw_safe_pixel(105,8);
        draw_safe_pixel(106,8);
    }
    else
    {
        /* Sol. */
        draw_safe_pixel(105,3);
        draw_safe_pixel(104,4);
        draw_safe_pixel(105,4);
        draw_safe_pixel(106,4);
        draw_safe_pixel(103,5);
        draw_safe_pixel(104,5);
        draw_safe_pixel(105,5);
        draw_safe_pixel(106,5);
        draw_safe_pixel(107,5);
        draw_safe_pixel(104,6);
        draw_safe_pixel(105,6);
        draw_safe_pixel(106,6);
        draw_safe_pixel(105,7);

        /* Nubes. */
        DrawLine(8U,5U,22U,5U);
        DrawLine(12U,4U,18U,4U);
        DrawLine(31U,8U,43U,8U);
        DrawLine(35U,7U,41U,7U);
    }

    /* Montañas. */
    phase = (uint8_t)((now / 400U) & 1U);

    DrawLine(0U,15U,13U,(uint8_t)(11U + phase));
    DrawLine(13U,(uint8_t)(11U + phase),25U,15U);
    DrawLine(25U,15U,38U,10U);
    DrawLine(38U,10U,52U,16U);
    DrawLine(52U,16U,66U,11U);
    DrawLine(66U,11U,81U,16U);
    DrawLine(81U,16U,94U,12U);
    DrawLine(94U,12U,108U,15U);
    DrawLine(108U,15U,127U,10U);

    DrawLine(
        0U,
        HORIZON_Y,
        127U,
        HORIZON_Y
    );
}

/* =========================================================
 * CARRETERA
 * =========================================================
 *
 * Se utilizan bordes continuos, un doble trazo de arcén,
 * línea central discontinua y pequeñas marcas laterales.
 * La superficie de la carretera permanece negra para que el
 * automóvil blanco tenga máximo contraste.
 * =========================================================
 */

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

    /* Bordes. */
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

        /* Línea interior de cada arcén. */
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

    /* Línea central desplazándose hacia el jugador. */
    phase = (uint8_t)((now / 45U) % 8U);

    for (
        dash = (uint8_t)(18U + phase);
        dash < PLAYFIELD_BOTTOM;
        dash = (uint8_t)(dash + 8U)
    )
    {
        dash_end = (uint8_t)(dash + 3U);

        if (dash_end >= PLAYFIELD_BOTTOM)
        {
            dash_end = (uint8_t)(PLAYFIELD_BOTTOM - 1U);
        }

        center1 = (uint8_t)(
            (
                road_left_at(dash, now) +
                road_right_at(dash, now)
            ) / 2U
        );

        center2 = (uint8_t)(
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

    /* Marcas de la cuneta. */
    for (
        dash = (uint8_t)(20U + (now / 55U) % 7U);
        dash < PLAYFIELD_BOTTOM;
        dash = (uint8_t)(dash + 7U)
    )
    {
        left = road_left_at(dash, now);
        right = road_right_at(dash, now);

        if (left >= 5U)
        {
            draw_safe_pixel((int16_t)(left - 4U), dash);
            draw_safe_pixel((int16_t)(left - 3U), dash);
        }

        if (right <= 122U)
        {
            draw_safe_pixel((int16_t)(right + 3U), dash);
            draw_safe_pixel((int16_t)(right + 4U), dash);
        }
    }
}

/* =========================================================
 * SPRITE PRINCIPAL
 * =========================================================
 */

static uint8_t sprite_pixel(
    uint8_t sx,
    uint8_t sy
)
{
    uint8_t byte_index;
    uint8_t bit_index;

    if (sx >= 32U || sy >= 16U)
    {
        return 0U;
    }

    byte_index = (uint8_t)(sx / 8U);
    bit_index = (uint8_t)(7U - (sx % 8U));

    if ((car_sprite[sy][byte_index] &
         (1U << bit_index)) != 0U)
    {
        return 1U;
    }

    return 0U;
}

static void draw_main_car(
    int16_t x,
    int16_t y
)
{
    uint8_t sx;
    uint8_t sy;

    for (sy = 0U; sy < 16U; sy++)
    {
        for (sx = 0U; sx < 32U; sx++)
        {
            if (sprite_pixel(sx, sy) != 0U)
            {
                draw_safe_pixel(
                    x + sx,
                    y + sy
                );
            }
        }
    }
}

/* =========================================================
 * TRAFICO PEQUEÑO
 * =========================================================
 */

static void draw_enemy_small(
    int16_t x,
    int16_t y,
    uint8_t night
)
{
    uint8_t row;
    uint8_t col;

    if (night != 0U)
    {
        draw_safe_pixel(x, y + 1);
        draw_safe_pixel(x + 3, y + 1);
        return;
    }

    for (row = 0U; row < 2U; row++)
    {
        for (col = 0U; col < 4U; col++)
        {
            if ((enemy_small[row] &
                 (1U << (3U - col))) != 0U)
            {
                draw_safe_pixel(
                    x + col,
                    y + row
                );
            }
        }
    }
}

/* =========================================================
 * TRAFICO MEDIANO
 * =========================================================
 */

static void draw_enemy_medium(
    int16_t x,
    int16_t y,
    uint8_t night
)
{
    uint8_t row;
    uint8_t col;

    if (night != 0U)
    {
        draw_safe_pixel(x, y + 1);
        draw_safe_pixel(x + 5, y + 1);
        draw_safe_pixel(x, y + 2);
        draw_safe_pixel(x + 5, y + 2);
        return;
    }

    for (row = 0U; row < 3U; row++)
    {
        for (col = 0U; col < 6U; col++)
        {
            if ((enemy_medium[row] &
                 (1U << (5U - col))) != 0U)
            {
                draw_safe_pixel(
                    x + col,
                    y + row
                );
            }
        }
    }
}

/* =========================================================
 * TRAFICO GRANDE
 * =========================================================
 */

static void draw_enemy_large(
    int16_t x,
    int16_t y,
    uint8_t night
)
{
    uint8_t row;
    uint8_t col;

    if (night != 0U)
    {
        draw_safe_pixel(x + 1, y + 3);
        draw_safe_pixel(x + 9, y + 3);
        draw_safe_pixel(x + 1, y + 4);
        draw_safe_pixel(x + 9, y + 4);
        return;
    }

    for (row = 0U; row < 5U; row++)
    {
        for (col = 0U; col < 11U; col++)
        {
            if ((enemy_large[row] &
                 (1U << (10U - col))) != 0U)
            {
                draw_safe_pixel(
                    x + col,
                    y + row
                );
            }
        }
    }
}

/* =========================================================
 * DIBUJAR TRAFICO
 * =========================================================
 */

static void draw_traffic_car(
    const Obstacle_t *ob,
    uint32_t now
)
{
    uint8_t y;
    uint8_t center;
    uint8_t night;

    int16_t x;

    if (ob == (const Obstacle_t *)0 || ob->active == 0U)
    {
        return;
    }

    y = depth_to_y(ob->depth);

    center = lane_center_at(
        ob->lane,
        y,
        now
    );

    x =
        (int16_t)center +
        ob->drift;

    night = is_night(now);

    /* =====================================================
     * Las escalas se eligen con la profundidad.
     * ===================================================== */
    if (ob->depth < 22U)
    {
        draw_enemy_small(
            x - 2,
            (int16_t)y - 2,
            night
        );
    }
    else if (ob->depth < 50U)
    {
        draw_enemy_medium(
            x - 3,
            (int16_t)y - 3,
            night
        );
    }
    else
    {
        draw_enemy_large(
            x - 5,
            (int16_t)y - 5,
            night
        );
    }
}

/* =========================================================
 * PLAYER
 * =========================================================
 */

static void draw_player_vehicle(uint32_t now)
{
    /* Parpadeo después de una colisión. */
    if ((int32_t)(now - collision_ignore_until) < 0)
    {
        if (((now / 70U) & 1U) != 0U)
        {
            return;
        }
    }

    draw_main_car(
        player_x,
        PLAYER_Y
    );
}

/* =========================================================
 * HUD
 * =========================================================
 *
 * No se rellena la zona inferior. Se muestra texto claro sobre
 * fondo negro. De izquierda a derecha:
 *
 * S:score  L:level  V:lives  P:cars passed
 * =========================================================
 */

static void draw_hud(void)
{
    DrawLine(
        0U,
        HUD_TOP,
        127U,
        HUD_TOP
    );

    draw_text5x7(1U,56U,"S:");
    draw_number(13U,56U,score,4U);

    draw_text5x7(43U,56U,"L:");
    draw_number(55U,56U,level,1U);

    draw_text5x7(67U,56U,"V:");
    draw_number(79U,56U,lives,1U);

    draw_text5x7(91U,56U,"P:");
    draw_number(103U,56U,cars_passed,3U);
}

/* =========================================================
 * ESCENA
 * =========================================================
 */

static void draw_scene(uint32_t now)
{
    uint8_t i;

    clear_graphics();

    draw_sky(now);
    draw_road(now);

    /* Autos enemigos detrás del jugador. */
    for (i = 0U; i < MAX_OBSTACLES; i++)
    {
        draw_traffic_car(
            &obstacles[i],
            now
        );
    }

    draw_player_vehicle(now);
    draw_hud();

    /* Indicador de impacto. */
    if ((int32_t)(now - impact_flash_until) < 0)
    {
        DrawRectangle(1U,1U,14U,8U);
        draw_text5x7(4U,2U,"X");
    }

    ST7920_Update();
}

/* =========================================================
 * MOVIMIENTO PLAYER
 * =========================================================
 */

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
        ) - PLAYER_WIDTH - 2;

    if (right < left)
    {
        left = 2;
        right = GAME_WIDTH - PLAYER_WIDTH - 2;
    }

    if (key == '4')
    {
        player_x -= PLAYER_STEP;
    }
    else if (key == '6')
    {
        player_x += PLAYER_STEP;
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

/* =========================================================
 * UPDATE TRAFICO
 * =========================================================
 */

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

        /* Movimiento lateral pequeño y aleatorio de los rivales. */
        if (obstacles[i].depth > 30U)
        {
            if ((random_number() % 10U) < 3U)
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
        }

        /* Mantener el rival razonablemente dentro de su carril. */
        y = depth_to_y(obstacles[i].depth);
        left = road_left_at(y, now);
        right = road_right_at(y, now);
        center = lane_center_at(obstacles[i].lane, y, now);

        if (center < left + 3U && obstacles[i].drift < 0)
        {
            obstacles[i].drift = 0;
        }

        if (center > right - 3U && obstacles[i].drift > 0)
        {
            obstacles[i].drift = 0;
        }

        /* Avance del tráfico hacia el jugador. */
        if (
            (uint16_t)obstacles[i].depth +
            (uint16_t)speed >= 100U
        )
        {
            /* Auto adelantado. */
            score =
                (uint16_t)(score + SCORE_PER_PASS);

            cars_passed++;

            buzzer_trigger(
                BUZZER_SCORE_PATTERN,
                1U,
                now
            );

            /* Nivel 1 -> 2. */
            if (level == GAME_LEVEL_1 && score >= LEVEL2_SCORE)
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

            /* Nivel 2 -> 3. */
            if (level == GAME_LEVEL_2 && score >= LEVEL3_SCORE)
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

            /* Victoria. */
            if (level == GAME_LEVEL_3 && score >= WIN_SCORE)
            {
                game_state = GAME_WIN;

                buzzer_trigger(
                    BUZZER_WIN_PATTERN,
                    6U,
                    now
                );

                return;
            }

            spawn_obstacle(i, 0U);
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

/* =========================================================
 * COLISIONES
 * =========================================================
 */

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

    /* Hitbox ligeramente menor al sprite para evitar colisiones
     * excesivamente estrictas. */
    player_left = player_x + 5;
    player_right = player_x + PLAYER_WIDTH - 6;
    player_top = PLAYER_Y + 4;
    player_bottom = PLAYER_Y + PLAYER_HEIGHT - 2;

    for (i = 0U; i < MAX_OBSTACLES; i++)
    {
        uint8_t depth;
        uint8_t y;
        uint8_t center;
        uint8_t width;
        uint8_t height;

        int16_t enemy_x;
        int16_t enemy_top;
        int16_t enemy_left;
        int16_t enemy_right;
        int16_t enemy_bottom;

        if (obstacles[i].active == 0U)
        {
            continue;
        }

        depth = obstacles[i].depth;

        /* La zona de impacto empieza muy cerca del jugador. */
        if (depth < 82U)
        {
            continue;
        }

        y = depth_to_y(depth);

        center = lane_center_at(
            obstacles[i].lane,
            y,
            now
        );

        enemy_x =
            (int16_t)center +
            obstacles[i].drift;

        /* Dimensiones correspondientes al sprite mostrado. */
        if (depth < 50U)
        {
            width = 6U;
            height = 3U;
        }
        else
        {
            width = 11U;
            height = 5U;
        }

        enemy_x -= (int16_t)(width / 2U);

        enemy_top =
            (int16_t)y - height;

        enemy_bottom =
            (int16_t)y - 1;

        enemy_left =
            enemy_x + 1;

        enemy_right =
            enemy_x + width - 2;

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

            /* Recuperación: volver al centro y reciclar el rival. */
            player_x = PLAYER_START_X;

            spawn_obstacle(i, 0U);

            if (lives == 0U)
            {
                game_state = GAME_OVER;
            }

            return;
        }
    }
}

/* =========================================================
 * GPIO DEL JUEGO
 * =========================================================
 */

void Game_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Buzzer. */
    gpio.Pin = GAME_BUZZER_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GAME_BUZZER_PORT,
        &gpio
    );

    /* LEDs de vidas. */
    gpio.Pin = LIFE1_PIN |
               LIFE2_PIN |
               LIFE3_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(
        GPIOF,
        &gpio
    );

    HAL_GPIO_WritePin(
        GAME_BUZZER_PORT,
        GAME_BUZZER_PIN,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        GPIOF,
        LIFE1_PIN | LIFE2_PIN | LIFE3_PIN,
        GPIO_PIN_SET
    );
}

/* =========================================================
 * INIT
 * =========================================================
 */

void Game_Init(void)
{
    random_seed =
        0x12345678UL ^ HAL_GetTick();

    game_state = GAME_MENU;

    player_x = PLAYER_START_X;

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

    curve_amplitude = 3;
    curve_period = 40U;
    curve_phase = 0U;
    curve_change_at = 0U;

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

/* =========================================================
 * UPDATE PRINCIPAL
 * =========================================================
 */

void Game_Update(uint32_t now)
{
    char key;

    buzzer_service(now);

    key = poll_key(now);

    /* =====================================================
     * MENU
     * =====================================================
     */
    if (game_state == GAME_MENU)
    {
        if (key == '5')
        {
            reset_game(now);
        }

        return;
    }

    /* =====================================================
     * GAME OVER
     * =====================================================
     */
    if (game_state == GAME_OVER)
    {
        if (key == '5')
        {
            reset_game(now);
        }

        return;
    }

    /* =====================================================
     * VICTORIA
     * =====================================================
     */
    if (game_state == GAME_WIN)
    {
        if (key == '5')
        {
            reset_game(now);
        }

        return;
    }

    /* =====================================================
     * PAUSA
     * =====================================================
     */
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

    /* =====================================================
     * MENSAJE DE NIVEL
     * =====================================================
     */
    if (game_state == GAME_LEVEL_MESSAGE)
    {
        if ((now - level_message_start) >= 1300U)
        {
            game_state = GAME_RUNNING;
            last_update = now;
        }

        return;
    }

    /* =====================================================
     * JUEGO NORMAL
     * =====================================================
     */

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

    /* Acelerador. */
    if (key == '8')
    {
        boost_until = now + 700U;
        brake_until = 0U;
    }

    /* Freno. */
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

    /* Desplazamiento temporal de la carretera. */
    curve_phase =
        (uint8_t)((curve_phase + 1U) & 31U);

    /* Primero colisión, luego avance de tráfico. */
    check_collision(now);

    if (game_state != GAME_RUNNING)
    {
        return;
    }

    update_obstacles(now);
}

/* =========================================================
 * RENDER PRINCIPAL
 * =========================================================
 */

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
