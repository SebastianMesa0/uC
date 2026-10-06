#include "main.h"
#include "teclado.h"

#include <stdint.h>

#define SEG_A   (1U << 0)
#define SEG_B   (1U << 1)
#define SEG_C   (1U << 2)
#define SEG_D   (1U << 3)
#define SEG_E   (1U << 4)
#define SEG_F   (1U << 5)
#define SEG_G   (1U << 6)

#define TOTAL_PASOS         8U
#define TOTAL_DIGITOS       4U
#define REFRESCO_DIGITO_MS  2U

typedef struct
{
    uint8_t digitos[TOTAL_DIGITOS];
} paso_t;

static const paso_t secuencia[TOTAL_PASOS] =
{
    /* 1: on d1ad, d2ad */
    {{SEG_A | SEG_D, SEG_A | SEG_D, 0U, 0U}},
    /* 2: on d2ad, d3ad */
    {{0U, SEG_A | SEG_D, SEG_A | SEG_D, 0U}},
    /* 3: on d3ad, d4ad */
    {{0U, 0U, SEG_A | SEG_D, SEG_A | SEG_D}},
    /* 4: on d4abcd */
    {{0U, 0U, 0U, SEG_A | SEG_B | SEG_C | SEG_D}},
    /* 5: on d4bcg */
    {{0U, 0U, 0U, SEG_B | SEG_C | SEG_G}},
    /* 6: on d4g, d3g */
    {{0U, 0U, SEG_G, SEG_G}},
    /* 7: on d3g, d2g */
    {{0U, SEG_G, SEG_G, 0U}},
    /* 8: on d1efg */
    {{SEG_E | SEG_F | SEG_G, 0U, 0U, 0U}}
};

static uint8_t paso_actual = 0U;
static uint8_t digito_actual = 0U;
static uint32_t t_refresco = 0U;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static void apagar_digitos(void)
{
#if DISPLAY_ACTIVE_HIGH
    HAL_GPIO_WritePin(DIGIT_PORT, DIGIT_ALL_PINS, GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(DIGIT_PORT, DIGIT_ALL_PINS, GPIO_PIN_SET);
#endif
}

static void escribir_segmentos(uint8_t mascara)
{
    HAL_GPIO_WritePin(SEG_PORT, SEG_A_PIN, (mascara & SEG_A) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_B_PIN, (mascara & SEG_B) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_C_PIN, (mascara & SEG_C) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_D_PIN, (mascara & SEG_D) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_E_PIN, (mascara & SEG_E) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_F_PIN, (mascara & SEG_F) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_G_PIN, (mascara & SEG_G) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(SEG_PORT, SEG_DP_PIN, GPIO_PIN_RESET);
}

static void habilitar_digito(uint8_t indice)
{
    uint16_t pin = DIGIT_1_PIN;

    if (indice == 1U)
    {
        pin = DIGIT_2_PIN;
    }
    else if (indice == 2U)
    {
        pin = DIGIT_3_PIN;
    }
    else if (indice == 3U)
    {
        pin = DIGIT_4_PIN;
    }

#if DISPLAY_ACTIVE_HIGH
    HAL_GPIO_WritePin(DIGIT_PORT, pin, GPIO_PIN_SET);
#else
    HAL_GPIO_WritePin(DIGIT_PORT, pin, GPIO_PIN_RESET);
#endif
}

static void refrescar_display(void)
{
    uint32_t ahora = HAL_GetTick();

    if ((ahora - t_refresco) < REFRESCO_DIGITO_MS)
    {
        return;
    }

    t_refresco = ahora;

    apagar_digitos();
    escribir_segmentos(secuencia[paso_actual].digitos[digito_actual]);
    habilitar_digito(digito_actual);

    digito_actual++;

    if (digito_actual >= TOTAL_DIGITOS)
    {
        digito_actual = 0U;
    }
}

static void procesar_tecla(char tecla)
{
    if ((tecla >= '1') && (tecla <= '8'))
    {
        paso_actual = (uint8_t)(tecla - '1');
    }

    if (tecla == '*')
    {
        paso_actual = 0U;
    }
}

int main(void)
{
    char tecla;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    apagar_digitos();
    escribir_segmentos(0U);

    while (1)
    {
        tecla = teclado();

        if (tecla != '\0')
        {
            procesar_tecla(tecla);
        }

        refrescar_display();
    }
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    GPIO_InitStruct.Pin = SEG_ALL_PINS;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SEG_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = DIGIT_ALL_PINS;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DIGIT_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TECLADO_COL1_PIN |
                          TECLADO_COL2_PIN |
                          TECLADO_COL3_PIN |
                          TECLADO_COL4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = TECLADO_ROW1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TECLADO_ROW2_PIN |
                          TECLADO_ROW3_PIN |
                          TECLADO_ROW4_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK |
                                  RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 |
                                  RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}
