#include "stm32f4xx_hal.h"
#include "ST7920_Parallel.h"
#include "teclado.h"

#include <stdint.h>

/*
 * Mapa de controles:
 *   4 : mover bola a la izquierda
 *   6 : mover bola a la derecha
 *   5 : lanzar bola
 *   * : reiniciar tablero
 *   # : centrar bola
 */

#define SCREEN_W                128U
#define SCREEN_H                64U

#define LANE_X                  18U
#define LANE_Y                  12U
#define LANE_W                  92U
#define LANE_H                  50U

#define BALL_RADIUS             2
#define BALL_START_Y            58
#define BALL_STEP_X             4

#define BALL_MIN_X              ((int16_t)(LANE_X + 3U))
#define BALL_MAX_X              ((int16_t)(LANE_X + LANE_W - 3U))
#define BALL_CENTER_X           ((int16_t)(LANE_X + (LANE_W / 2U)))

#define PINS_COUNT              10U

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

typedef struct
{
    uint8_t x;
    uint8_t y;
} PinPos;

static const PinPos kPins[PINS_COUNT] = {
    {49U, 34U}, {59U, 34U}, {69U, 34U}, {79U, 34U},
    {54U, 28U}, {64U, 28U}, {74U, 28U},
    {59U, 22U}, {69U, 22U},
    {64U, 16U}
};

static uint8_t gPinStanding[PINS_COUNT];
static int16_t gBallX = BALL_CENTER_X;

static const uint8_t FONT_SPACE[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t FONT_HASH[5]  = {0x14, 0x3E, 0x14, 0x3E, 0x14};
static const uint8_t FONT_STAR[5]  = {0x08, 0x2A, 0x1C, 0x2A, 0x08};
static const uint8_t FONT_LT[5]    = {0x00, 0x08, 0x14, 0x22, 0x00};
static const uint8_t FONT_GT[5]    = {0x00, 0x22, 0x14, 0x08, 0x00};
static const uint8_t FONT_EQ[5]    = {0x00, 0x14, 0x14, 0x14, 0x00};
static const uint8_t FONT_CARET[5] = {0x04, 0x02, 0x01, 0x02, 0x04};
static const uint8_t FONT_COLON[5] = {0x00, 0x00, 0x14, 0x00, 0x00};
static const uint8_t FONT_SLASH[5] = {0x20, 0x10, 0x08, 0x04, 0x02};

static const uint8_t FONT_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
static const uint8_t FONT_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
static const uint8_t FONT_2[5] = {0x62, 0x51, 0x49, 0x49, 0x46};
static const uint8_t FONT_3[5] = {0x22, 0x49, 0x49, 0x49, 0x36};
static const uint8_t FONT_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
static const uint8_t FONT_5[5] = {0x2F, 0x49, 0x49, 0x49, 0x31};
static const uint8_t FONT_6[5] = {0x3E, 0x49, 0x49, 0x49, 0x32};
static const uint8_t FONT_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
static const uint8_t FONT_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t FONT_9[5] = {0x26, 0x49, 0x49, 0x49, 0x3E};

static const uint8_t FONT_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};
static const uint8_t FONT_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t FONT_P[5] = {0x7F, 0x09, 0x09, 0x09, 0x06};
static const uint8_t FONT_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
static const uint8_t FONT_S[5] = {0x26, 0x49, 0x49, 0x49, 0x32};
static const uint8_t FONT_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};

static const uint8_t *get_glyph(char c)
{
    switch (c)
    {
    case '0': return FONT_0;
    case '1': return FONT_1;
    case '2': return FONT_2;
    case '3': return FONT_3;
    case '4': return FONT_4;
    case '5': return FONT_5;
    case '6': return FONT_6;
    case '7': return FONT_7;
    case '8': return FONT_8;
    case '9': return FONT_9;
    case 'C': return FONT_C;
    case 'O': return FONT_O;
    case 'P': return FONT_P;
    case 'R': return FONT_R;
    case 'S': return FONT_S;
    case 'T': return FONT_T;
    case '#': return FONT_HASH;
    case '*': return FONT_STAR;
    case '<': return FONT_LT;
    case '>': return FONT_GT;
    case '=': return FONT_EQ;
    case '^': return FONT_CARET;
    case ':': return FONT_COLON;
    case '/': return FONT_SLASH;
    case ' ': return FONT_SPACE;
    default:  return FONT_SPACE;
    }
}

static void draw_char5x7(uint8_t x, uint8_t y, char c)
{
    const uint8_t *glyph = get_glyph(c);
    uint8_t col;
    uint8_t row;

    for (col = 0U; col < 5U; col++)
    {
        for (row = 0U; row < 7U; row++)
        {
            if (((glyph[col] >> row) & 0x01U) != 0U)
            {
                if ((x + col) < SCREEN_W && (y + row) < SCREEN_H)
                {
                    SetPixel((uint8_t)(x + col), (uint8_t)(y + row));
                }
            }
        }
    }
}

static void draw_text5x7(uint8_t x, uint8_t y, const char *text)
{
    while (*text != '\0')
    {
        draw_char5x7(x, y, *text);
        x = (uint8_t)(x + 6U);
        text++;
    }
}

static uint8_t knocked_count(void)
{
    uint8_t i;
    uint8_t count = 0U;

    for (i = 0U; i < PINS_COUNT; i++)
    {
        if (gPinStanding[i] == 0U)
        {
            count++;
        }
    }

    return count;
}

static uint8_t abs_i16(int16_t v)
{
    if (v < 0)
    {
        return (uint8_t)(-v);
    }

    return (uint8_t)v;
}

static void u8_to_text(uint8_t value, char *out)
{
    if (value >= 10U)
    {
        out[0] = '1';
        out[1] = (char)('0' + (value - 10U));
        out[2] = '\0';
    }
    else
    {
        out[0] = (char)('0' + value);
        out[1] = '\0';
    }
}

static void reset_board(void)
{
    uint8_t i;

    for (i = 0U; i < PINS_COUNT; i++)
    {
        gPinStanding[i] = 1U;
    }

    gBallX = BALL_CENTER_X;
}

static void draw_pin(uint8_t x, uint8_t y)
{
    DrawFilledCircle((int16_t)x, (int16_t)y, 2);
    DrawLine(x, (uint8_t)(y + 2U), x, (uint8_t)(y + 4U));
}

static void draw_pin_id(uint8_t pin_index, uint8_t x, uint8_t y)
{
    char text[3];

    u8_to_text((uint8_t)(pin_index + 1U), text);
    if (pin_index < 9U)
    {
        draw_text5x7((uint8_t)(x - 2U), (uint8_t)(y - 9U), text);
    }
    else
    {
        draw_text5x7((uint8_t)(x - 5U), (uint8_t)(y - 9U), text);
    }
}

static void draw_lane(void)
{
    DrawRectangle(LANE_X, LANE_Y, LANE_W, LANE_H);
    DrawLine((uint8_t)(LANE_X + 1U), (uint8_t)(LANE_Y + 1U),
             (uint8_t)(LANE_X + 1U), (uint8_t)(LANE_Y + LANE_H - 1U));
    DrawLine((uint8_t)(LANE_X + LANE_W - 1U), (uint8_t)(LANE_Y + 1U),
             (uint8_t)(LANE_X + LANE_W - 1U), (uint8_t)(LANE_Y + LANE_H - 1U));
}

static void draw_hud(void)
{
    char score_text[3];

    u8_to_text(knocked_count(), score_text);

    draw_text5x7(0U, 0U, "P:S");
    draw_text5x7(18U, 0U, score_text);
    draw_text5x7(36U, 0U, "/10");

    draw_text5x7(0U, 7U, "4<6> 5^ *=R #=C");
}

static void draw_scene(int16_t ball_x, uint8_t ball_y)
{
    uint8_t i;

    ST7920_ClearBuffer();

    draw_hud();
    draw_lane();

    for (i = 0U; i < PINS_COUNT; i++)
    {
        if (gPinStanding[i] != 0U)
        {
            draw_pin(kPins[i].x, kPins[i].y);
            draw_pin_id(i, kPins[i].x, kPins[i].y);
        }
    }

    DrawFilledCircle(ball_x, ball_y, BALL_RADIUS);
}

static void apply_knockdown(int16_t ball_x)
{
    uint8_t i;
    uint8_t j;
    uint8_t hit[PINS_COUNT] = {0};

    for (i = 0U; i < PINS_COUNT; i++)
    {
        uint8_t dx;

        if (gPinStanding[i] == 0U)
        {
            continue;
        }

        dx = abs_i16((int16_t)kPins[i].x - ball_x);

        if (dx <= 3U)
        {
            hit[i] = 1U;
        }
        else if (dx <= 6U && kPins[i].y >= 28U)
        {
            hit[i] = 1U;
        }
    }

    for (i = 0U; i < PINS_COUNT; i++)
    {
        if (gPinStanding[i] == 0U || hit[i] != 0U)
        {
            continue;
        }

        for (j = 0U; j < PINS_COUNT; j++)
        {
            uint8_t dx;
            uint8_t dy;

            if (hit[j] == 0U)
            {
                continue;
            }

            dx = abs_i16((int16_t)kPins[i].x - (int16_t)kPins[j].x);
            dy = abs_i16((int16_t)kPins[i].y - (int16_t)kPins[j].y);

            if (dx <= 6U && dy <= 7U)
            {
                hit[i] = 1U;
                break;
            }
        }
    }

    for (i = 0U; i < PINS_COUNT; i++)
    {
        if (hit[i] != 0U)
        {
            gPinStanding[i] = 0U;
        }
    }
}

static void launch_ball(void)
{
    int16_t y;

    for (y = BALL_START_Y; y >= 16; y -= 3)
    {
        draw_scene(gBallX, (uint8_t)y);
        ST7920_Update();
        HAL_Delay(45U);
    }

    apply_knockdown(gBallX);

    draw_scene(gBallX, 16U);
    ST7920_Update();
    HAL_Delay(100U);
}

int main(void)
{
    char key;
    uint8_t need_redraw = 1U;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    ST7920_Init();
    ST7920_GraphicMode(1);
    ST7920_ClearBuffer();
    ST7920_Update();

    reset_board();

    while (1)
    {
        key = teclado();

        if (key == '4')
        {
            if (gBallX > BALL_MIN_X)
            {
                gBallX = (int16_t)(gBallX - BALL_STEP_X);
                if (gBallX < BALL_MIN_X)
                {
                    gBallX = BALL_MIN_X;
                }
                need_redraw = 1U;
            }
        }
        else if (key == '6')
        {
            if (gBallX < BALL_MAX_X)
            {
                gBallX = (int16_t)(gBallX + BALL_STEP_X);
                if (gBallX > BALL_MAX_X)
                {
                    gBallX = BALL_MAX_X;
                }
                need_redraw = 1U;
            }
        }
        else if (key == '5')
        {
            launch_ball();
            need_redraw = 1U;
        }
        else if (key == '*')
        {
            reset_board();
            need_redraw = 1U;
        }
        else if (key == '#')
        {
            gBallX = BALL_CENTER_X;
            need_redraw = 1U;
        }

        if (need_redraw != 0U)
        {
            draw_scene(gBallX, BALL_START_Y);
            ST7920_Update();
            need_redraw = 0U;
        }

        HAL_Delay(8U);
    }
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    gpio.Pin = RS_PIN | RW_PIN | E_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = RST_PIN | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOD, &gpio);

    HAL_GPIO_WritePin(GPIOC, RS_PIN | RW_PIN | E_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, RST_PIN | GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7, GPIO_PIN_RESET);

    gpio.Pin = TECLADO_COL1_PIN | TECLADO_COL2_PIN | TECLADO_COL3_PIN | TECLADO_COL4_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);
    HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

    gpio.Pin = TECLADO_ROW1_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);

    gpio.Pin = TECLADO_ROW2_PIN | TECLADO_ROW3_PIN | TECLADO_ROW4_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOF, &gpio);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        while (1) { }
    }

    clk.ClockType = RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK)
    {
        while (1) { }
    }
}
