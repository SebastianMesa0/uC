#include "stm32f4xx_hal.h"

#include "ST7920_Parallel.h"
#include "teclado.h"
#include "game.h"

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* Relojes de los puertos utilizados. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* =========================================================
     * ST7920
     * RS=PC9, RW=PC10, E=PC11
     * ========================================================= */
    gpio.Pin = RS_PIN | RW_PIN | E_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOC, &gpio);

    /* =========================================================
     * ST7920
     * RST=PD3, DB7=PD4, DB6=PD5, DB5=PD6, DB4=PD7
     * ========================================================= */
    gpio.Pin = RST_PIN |
               GPIO_PIN_4 |
               GPIO_PIN_5 |
               GPIO_PIN_6 |
               GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOD, &gpio);

    /* =========================================================
     * Teclado: columnas PE2, PE4, PE5, PE6
     * ========================================================= */
    gpio.Pin = TECLADO_COL1_PIN |
               TECLADO_COL2_PIN |
               TECLADO_COL3_PIN |
               TECLADO_COL4_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);

    HAL_GPIO_WritePin(
        GPIOE,
        TECLADO_COL_MASK,
        GPIO_PIN_RESET
    );

    /* Fila 1 = PE3 */
    gpio.Pin = TECLADO_ROW1_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);

    /* F2=PF8, F3=PF7, F4=PF9 */
    gpio.Pin = TECLADO_ROW2_PIN |
               TECLADO_ROW3_PIN |
               TECLADO_ROW4_PIN;
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

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    /* HSI = 16 MHz, sin PLL. */
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;

    HAL_RCC_OscConfig(&osc);

    clk.ClockType = RCC_CLOCKTYPE_HCLK |
                    RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2;

    clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(
        &clk,
        FLASH_LATENCY_0
    );
}

int main(void)
{
    uint32_t now;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    Game_GPIO_Init();

    /* El LCD se utiliza exclusivamente en GDRAM. */
    ST7920_Init();
    ST7920_GraphicMode(1);
    ST7920_ClearBuffer();
    ST7920_Update();

    Game_Init();

    while (1)
    {
        now = HAL_GetTick();

        Game_Update(now);
        Game_Render(now);

        /* El control temporal real está dentro de game.c. */
        HAL_Delay(1U);
    }
}
