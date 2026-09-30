#include "stm32f4xx_hal.h"
#include "ST7920_parallel.h"
#include "teclado.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*
 * Constantes configurables de la actividad.
 *
 * Reemplace estos seis valores por los definidos en el enunciado oficial,
 * manteniendo el orden ascendente de limites (inferior/superior por rango).
 */
#define RANGO_1_LOWER_BOUND   100U
#define RANGO_1_UPPER_BOUND   200U
#define RANGO_2_LOWER_BOUND   300U
#define RANGO_2_UPPER_BOUND   400U
#define RANGO_3_LOWER_BOUND   500U
#define RANGO_3_UPPER_BOUND   600U

#define DISPLAY_COLS          16U
#define MAX_DIGITOS           3U
#define SALIDA_BLOQUEO_MS     3000U

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

static void mostrar_linea(uint8_t fila, const char *texto)
{
    char linea[DISPLAY_COLS + 1U];
    size_t longitud;

    memset(linea, ' ', DISPLAY_COLS);
    linea[DISPLAY_COLS] = '\0';

    longitud = strlen(texto);

    if (longitud > DISPLAY_COLS)
    {
        longitud = DISPLAY_COLS;
    }

    memcpy(linea, texto, longitud);

    ST7920_SendString(fila, 0U, linea);
}

static void actualizar_linea_numero(const char *entrada)
{
    char linea_numero[DISPLAY_COLS + 1U];

    snprintf(linea_numero, sizeof(linea_numero), "NUM = %s", entrada);
    mostrar_linea(3U, linea_numero);
}

static uint8_t es_digito(char tecla)
{
    if ((tecla >= '0') && (tecla <= '9'))
    {
        return 1U;
    }

    return 0U;
}

static uint32_t convertir_entrada(const char *entrada)
{
    return (uint32_t)strtoul(entrada, NULL, 10);
}

static void mostrar_pantalla_normal(void)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    mostrar_linea(0U, "CONTROL DE ACCESO");
    mostrar_linea(1U, "INGRESE NUMERO");
    mostrar_linea(2U, "*=BORRAR #=OK");
    actualizar_linea_numero("");
}

static void resetear_entrada(char *entrada)
{
    entrada[0] = '\0';
}

static void mostrar_salida_y_resetear(char *entrada)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    mostrar_linea(0U, "ACCESO DENEGADO");
    mostrar_linea(1U, "");
    mostrar_linea(2U, "");
    mostrar_linea(3U, "");

    HAL_Delay(SALIDA_BLOQUEO_MS);

    resetear_entrada(entrada);
    mostrar_pantalla_normal();
}

/*
 * Comparaciones estrictas por el criterio del enunciado:
 * lower < entrada < upper.
 */
static uint8_t evaluar_y_mostrar_rango(uint32_t valor)
{
    char rango_linea[DISPLAY_COLS + 1U];

    if ((valor > RANGO_1_LOWER_BOUND) &&
        (valor < RANGO_1_UPPER_BOUND))
    {
        snprintf(
            rango_linea,
            sizeof(rango_linea),
            "RANGO: [%u-%u]",
            (unsigned int)RANGO_1_LOWER_BOUND,
            (unsigned int)RANGO_1_UPPER_BOUND
        );

        mostrar_linea(0U, rango_linea);
        mostrar_linea(1U, "PERFIL OPERADOR");
        mostrar_linea(2U, "*=BORRAR #=OK");

        return 1U;
    }

    if ((valor > RANGO_2_LOWER_BOUND) &&
        (valor < RANGO_2_UPPER_BOUND))
    {
        snprintf(
            rango_linea,
            sizeof(rango_linea),
            "RANGO: [%u-%u]",
            (unsigned int)RANGO_2_LOWER_BOUND,
            (unsigned int)RANGO_2_UPPER_BOUND
        );

        mostrar_linea(0U, rango_linea);
        mostrar_linea(1U, "ADMINISTRADOR");
        mostrar_linea(2U, "*=BORRAR #=OK");

        return 1U;
    }

    if ((valor > RANGO_3_LOWER_BOUND) &&
        (valor < RANGO_3_UPPER_BOUND))
    {
        snprintf(
            rango_linea,
            sizeof(rango_linea),
            "RANGO: [%u-%u]",
            (unsigned int)RANGO_3_LOWER_BOUND,
            (unsigned int)RANGO_3_UPPER_BOUND
        );

        mostrar_linea(0U, rango_linea);
        mostrar_linea(1U, "PERFIL AUDITOR");
        mostrar_linea(2U, "*=BORRAR #=OK");

        return 1U;
    }

    return 0U;
}

int main(void)
{
    char tecla;
    char entrada[MAX_DIGITOS + 1U];
    uint8_t longitud;
    uint32_t valor;

    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();

    ST7920_Init();

    resetear_entrada(entrada);
    mostrar_pantalla_normal();

    while (1)
    {
        tecla = teclado();

        if (es_digito(tecla) != 0U)
        {
            longitud = (uint8_t)strlen(entrada);

            if (longitud >= MAX_DIGITOS)
            {
                mostrar_salida_y_resetear(entrada);
                HAL_Delay(10U);
                continue;
            }

            entrada[longitud] = tecla;
            entrada[longitud + 1U] = '\0';

            actualizar_linea_numero(entrada);
        }

        if (tecla == '*')
        {
            resetear_entrada(entrada);
            mostrar_pantalla_normal();
        }

        if (tecla == '#')
        {
            if (entrada[0] == '\0')
            {
                mostrar_salida_y_resetear(entrada);
                HAL_Delay(10U);
                continue;
            }

            valor = convertir_entrada(entrada);

            if (evaluar_y_mostrar_rango(valor) == 0U)
            {
                mostrar_salida_y_resetear(entrada);
                HAL_Delay(10U);
                continue;
            }

            actualizar_linea_numero(entrada);
        }

        HAL_Delay(10U);
    }
}

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /* ST7920: RS=PC9, RW=PC10, E=PC11 */
    GPIO_InitStruct.Pin = RS_PIN | RW_PIN | E_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ST7920: RST=PD3, DB7..DB4=PD4..PD7 */
    GPIO_InitStruct.Pin = RST_PIN |
                          GPIO_PIN_4 |
                          GPIO_PIN_5 |
                          GPIO_PIN_6 |
                          GPIO_PIN_7;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Teclado columnas: PE2, PE4, PE5, PE6 */
    GPIO_InitStruct.Pin = TECLADO_COL1_PIN |
                          TECLADO_COL2_PIN |
                          TECLADO_COL3_PIN |
                          TECLADO_COL4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

    /* Teclado fila 1: PE3 con pulldown */
    GPIO_InitStruct.Pin = TECLADO_ROW1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* Teclado filas 2, 3, 4: PF8, PF7, PF9 con pulldown */
    GPIO_InitStruct.Pin = TECLADO_ROW2_PIN |
                          TECLADO_ROW3_PIN |
                          TECLADO_ROW4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

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
