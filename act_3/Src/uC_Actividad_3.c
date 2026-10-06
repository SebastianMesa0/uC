#include "stm32f4xx_hal.h"
#include "ST7920_parallel.h"
#include "teclado.h"

#include <stdint.h>
#include <stdio.h>

#define SEG_A (1U << 0)
#define SEG_B (1U << 1)
#define SEG_C (1U << 2)
#define SEG_D (1U << 3)
#define SEG_E (1U << 4)
#define SEG_F (1U << 5)
#define SEG_G (1U << 6)

#define SEG_PORT GPIOB
#define SEG_A_PIN GPIO_PIN_0
#define SEG_B_PIN GPIO_PIN_1
#define SEG_C_PIN GPIO_PIN_2
#define SEG_D_PIN GPIO_PIN_3
#define SEG_E_PIN GPIO_PIN_4
#define SEG_F_PIN GPIO_PIN_5
#define SEG_G_PIN GPIO_PIN_6
#define SEG_DP_PIN GPIO_PIN_7
#define SEG_ALL_PINS (SEG_A_PIN | SEG_B_PIN | SEG_C_PIN | SEG_D_PIN | \
                      SEG_E_PIN | SEG_F_PIN | SEG_G_PIN | SEG_DP_PIN)

#define DIGIT_PORT GPIOE
#define DIGIT_1_PIN GPIO_PIN_7
#define DIGIT_2_PIN GPIO_PIN_8
#define DIGIT_3_PIN GPIO_PIN_9
#define DIGIT_4_PIN GPIO_PIN_11
#define DIGIT_ALL_PINS (DIGIT_1_PIN | DIGIT_2_PIN | DIGIT_3_PIN | DIGIT_4_PIN)

#define TOTAL_PASOS 8U
#define TOTAL_DIGITOS 4U
#define REFRESCO_MS 2U
#define PASO_MS 250U

typedef struct
{
    uint8_t digitos[TOTAL_DIGITOS];
} paso_t;

static const paso_t secuencia[TOTAL_PASOS] =
{
    {{SEG_A | SEG_D, SEG_A | SEG_D, 0U, 0U}},
    {{0U, SEG_A | SEG_D, SEG_A | SEG_D, 0U}},
    {{0U, 0U, SEG_A | SEG_D, SEG_A | SEG_D}},
    {{0U, 0U, 0U, SEG_A | SEG_B | SEG_C | SEG_D}},
    {{0U, 0U, 0U, SEG_B | SEG_C | SEG_G}},
    {{0U, 0U, SEG_G, SEG_G}},
    {{0U, SEG_G, SEG_G, 0U}},
    {{SEG_E | SEG_F | SEG_G, 0U, 0U, 0U}}
};

static uint8_t paso_actual = 0U;
static uint8_t digito_actual = 0U;
static int8_t direccion = 1;
static uint8_t en_marcha = 1U;
static uint8_t caso_actual = 0U;

static uint32_t t_refresco = 0U;
static uint32_t t_paso = 0U;

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static void lcd_linea(uint8_t fila, const char *texto)
{
    char linea[17];
    snprintf(linea, sizeof(linea), "%-16s", texto);
    ST7920_SendString(fila, 0U, linea);
}

static void lcd_estado(void)
{
    char l2[17];
    char l3[17];

    snprintf(l2, sizeof(l2), "CASO: %u", (unsigned int)caso_actual);
    snprintf(l3, sizeof(l3), "PASO: %u", (unsigned int)(paso_actual + 1U));

    ST7920_GraphicMode(0);
    lcd_linea(0U, "1:CW 2:CCW");
    lcd_linea(1U, "3:STOP 4:RST");
    lcd_linea(2U, l2);
    lcd_linea(3U, l3);
}

static void apagar_digitos(void)
{
    HAL_GPIO_WritePin(DIGIT_PORT, DIGIT_ALL_PINS, GPIO_PIN_RESET);
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

    HAL_GPIO_WritePin(DIGIT_PORT, pin, GPIO_PIN_SET);
}

static void refrescar_displays(void)
{
    uint32_t ahora = HAL_GetTick();

    if ((ahora - t_refresco) < REFRESCO_MS)
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

static void avanzar_paso(void)
{
    uint32_t ahora = HAL_GetTick();

    if (en_marcha == 0U)
    {
        return;
    }

    if ((ahora - t_paso) < PASO_MS)
    {
        return;
    }

    t_paso = ahora;

    if (direccion > 0)
    {
        paso_actual++;
        if (paso_actual >= TOTAL_PASOS)
        {
            paso_actual = 0U;
        }
    }
    else
    {
        if (paso_actual == 0U)
        {
            paso_actual = (TOTAL_PASOS - 1U);
        }
        else
        {
            paso_actual--;
        }
    }

    lcd_estado();
}

static void procesar_tecla(char tecla)
{
    if ((tecla == '1') || (tecla == 'A'))
    {
        caso_actual = 0U;
        direccion = 1;
        en_marcha = 1U;
        t_paso = HAL_GetTick();
        lcd_estado();
        return;
    }

    if ((tecla == '2') || (tecla == 'B'))
    {
        caso_actual = 1U;
        direccion = -1;
        en_marcha = 1U;
        t_paso = HAL_GetTick();
        lcd_estado();
        return;
    }

    if ((tecla == '3') || (tecla == 'C'))
    {
        caso_actual = 2U;
        en_marcha = 0U;
        lcd_estado();
        return;
    }

    if ((tecla == '4') || (tecla == 'D'))
    {
        caso_actual = 3U;
        paso_actual = 0U;
        direccion = 1;
        en_marcha = 1U;
        t_paso = HAL_GetTick();
        lcd_estado();
    }
}

int main(void)
{
    char tecla;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    ST7920_Init();
    ST7920_Clear();

    apagar_digitos();
    escribir_segmentos(0U);
    lcd_estado();

    while (1)
    {
        tecla = teclado();

        if (tecla != '\0')
        {
            procesar_tecla(tecla);
        }

        avanzar_paso();
        refrescar_displays();
    }
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    GPIO_InitStruct.Pin = RS_PIN | RW_PIN | E_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = RST_PIN |
                          GPIO_PIN_4 |
                          GPIO_PIN_5 |
                          GPIO_PIN_6 |
                          GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

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
