#include "stm32f4xx_hal.h"

#include "ST7920_Parallel.h"
#include "teclado.h"
#include "bowling_game.h"

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    gpio.Pin = RS_PIN | RW_PIN | E_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOC, &gpio);

    gpio.Pin = RST_PIN |
               GPIO_PIN_4 |
               GPIO_PIN_5 |
               GPIO_PIN_6 |
               GPIO_PIN_7;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOD, &gpio);

    HAL_GPIO_WritePin(GPIOC,
                      RS_PIN | RW_PIN | E_PIN,
                      GPIO_PIN_RESET);

    HAL_GPIO_WritePin(GPIOD,
                      RST_PIN |
                      GPIO_PIN_4 |
                      GPIO_PIN_5 |
                      GPIO_PIN_6 |
                      GPIO_PIN_7,
                      GPIO_PIN_RESET);

    gpio.Pin = TECLADO_COL1_PIN |
               TECLADO_COL2_PIN |
               TECLADO_COL3_PIN |
               TECLADO_COL4_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);

    HAL_GPIO_WritePin(GPIOE,
                      TECLADO_COL_MASK,
                      GPIO_PIN_RESET);

    gpio.Pin = TECLADO_ROW1_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLDOWN;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &gpio);

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

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        while (1)
        {
        }
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
        while (1)
        {
        }
    }
}

int main(void)
{
    uint32_t now;

    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();
    Bowling_GPIO_Init();

    ST7920_Init();
    ST7920_GraphicMode(1);
    ST7920_ClearBuffer();
    ST7920_Update();

    Bowling_Init();

    while (1)
    {
        now = HAL_GetTick();

        Bowling_Update(now);
        Bowling_Render(now);

        HAL_Delay(1U);
    }
}
