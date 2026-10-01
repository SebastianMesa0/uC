#include "stm32f4xx_hal.h"
#include "ST7920_parallel.h"
#include "teclado.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_DIGITOS 3U

#define LIM_A 2U
#define LIM_B 101U
#define LIM_C 445U
#define LIM_D 460U
#define LIM_E 711U
#define LIM_F 753U

static char numero[MAX_DIGITOS + 1U];
static uint8_t indice = 0U;

void SystemClock_Config(void);
void MX_GPIO_Init(void);


/*====================================================
 * LCD
 *====================================================*/

static void LCD_Linea(uint8_t fila, const char *texto)
{
    char linea[17];

    snprintf(linea, sizeof(linea), "%-16s", texto);
    ST7920_SendString(fila, 0U, linea);
}


static void LCD_Inicio(void)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    LCD_Linea(0U, "INGRESE NUMERO");
    LCD_Linea(1U, "# BORRAR");
    LCD_Linea(2U, "* CONFIRMAR");

    {
        char texto[17];

        snprintf(texto, sizeof(texto), "NUM = %s", numero);
        LCD_Linea(3U, texto);
    }
}


static void LCD_Numero(void)
{
    char texto[17];

    snprintf(texto, sizeof(texto), "NUM = %s", numero);
    LCD_Linea(3U, texto);
}


static void LCD_Resultado(const char *rango, const char *perfil)
{
    LCD_Linea(0U, rango);
    LCD_Linea(1U, perfil);
    LCD_Linea(2U, "");
    LCD_Numero();
}


/*====================================================
 * CONTROL DE ENTRADA
 *====================================================*/

static void LimpiarNumero(void)
{
    numero[0] = '\0';
    indice = 0U;
}


static void AccesoDenegado(void)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    LCD_Linea(0U, "ACCESO DENEGADO");
    LCD_Linea(1U, "");
    LCD_Linea(2U, "");
    LCD_Linea(3U, "");

    HAL_Delay(3000U);

    LimpiarNumero();
    LCD_Inicio();
}


static void EvaluarNumero(void)
{
    uint16_t valor = (uint16_t)atoi(numero);

    if ((valor > LIM_A) && (valor < LIM_B))
    {
        LCD_Resultado("RANGO: [A - B]", "PERFIL OPERADOR");
    }
    else if ((valor > LIM_C) && (valor < LIM_D))
    {
        LCD_Resultado("RANGO: [C - D]", "ADMINISTRADOR");
    }
    else if ((valor > LIM_E) && (valor < LIM_F))
    {
        LCD_Resultado("RANGO: [E - F]", "PERFIL AUDITOR");
    }
    else
    {
        AccesoDenegado();
    }
}


/*====================================================
 * TECLADO
 *====================================================*/

static void ProcesarTecla(char tecla)
{
    if ((tecla >= '0') && (tecla <= '9'))
    {
        if (indice < MAX_DIGITOS)
        {
            numero[indice++] = tecla;
            numero[indice] = '\0';

            LCD_Numero();
        }
        else
        {
            AccesoDenegado();
        }

        return;
    }

    if (tecla == '#')
    {
        if (indice > 0U)
        {
            numero[--indice] = '\0';
            LCD_Numero();
        }

        return;
    }

    if ((tecla == '*') && (indice > 0U))
    {
        EvaluarNumero();
    }

    /*
     * Las teclas A, B, C y D no realizan ninguna acción.
     */
}


/*====================================================
 * MAIN
 *====================================================*/

int main(void)
{
    char tecla;

    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();

    ST7920_Init();

    LimpiarNumero();
    LCD_Inicio();

    while (1)
    {
        tecla = teclado();

        if (tecla != '\0')
        {
            ProcesarTecla(tecla);
        }

        HAL_Delay(10U);
    }
}


/*====================================================
 * GPIO
 *====================================================*/

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /*
     * Control del LCD:
     * RS = PC9, RW = PC10, E = PC11
     */
    GPIO_InitStruct.Pin = RS_PIN | RW_PIN | E_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*
     * LCD:
     * RST = PD3
     * DB7 = PD4
     * DB6 = PD5
     * DB5 = PD6
     * DB4 = PD7
     */
    GPIO_InitStruct.Pin = RST_PIN |
                          GPIO_PIN_4 |
                          GPIO_PIN_5 |
                          GPIO_PIN_6 |
                          GPIO_PIN_7;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /*
     * Columnas del teclado:
     * C1 = PE2, C2 = PE4, C3 = PE5, C4 = PE6
     */
    GPIO_InitStruct.Pin = TECLADO_COL1_PIN |
                          TECLADO_COL2_PIN |
                          TECLADO_COL3_PIN |
                          TECLADO_COL4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    HAL_GPIO_WritePin(
        GPIOE,
        TECLADO_COL_MASK,
        GPIO_PIN_RESET
    );

    /*
     * Fila 1 = PE3
     */
    GPIO_InitStruct.Pin = TECLADO_ROW1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /*
     * Fila 2 = PF8
     * Fila 3 = PF7
     * Fila 4 = PF9
     */
    GPIO_InitStruct.Pin = TECLADO_ROW2_PIN |
                          TECLADO_ROW3_PIN |
                          TECLADO_ROW4_PIN;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}


/*====================================================
 * RELOJ DEL SISTEMA
 *====================================================*/

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(
        &RCC_ClkInitStruct,
        FLASH_LATENCY_0
    );
}


/*====================================================
 * SYSTICK
 *====================================================*/

void SysTick_Handler(void)
{
    HAL_IncTick();
}
