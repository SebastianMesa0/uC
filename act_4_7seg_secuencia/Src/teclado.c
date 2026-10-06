#include "teclado.h"

#define TECLA_SIN_PULSAR    '\0'
#define FILA_NO_DETECTADA   255U

static const char mapa_teclado[4][4] =
{
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static void desactivar_columnas(void)
{
    HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);
}

static void activar_columna(uint8_t columna)
{
    desactivar_columnas();

    if (columna == 0U)
    {
        HAL_GPIO_WritePin(GPIOE, TECLADO_COL1_PIN, GPIO_PIN_SET);
    }

    if (columna == 1U)
    {
        HAL_GPIO_WritePin(GPIOE, TECLADO_COL2_PIN, GPIO_PIN_SET);
    }

    if (columna == 2U)
    {
        HAL_GPIO_WritePin(GPIOE, TECLADO_COL3_PIN, GPIO_PIN_SET);
    }

    if (columna == 3U)
    {
        HAL_GPIO_WritePin(GPIOE, TECLADO_COL4_PIN, GPIO_PIN_SET);
    }
}

static uint8_t leer_fila(void)
{
    if (HAL_GPIO_ReadPin(GPIOE, TECLADO_ROW1_PIN) == GPIO_PIN_SET)
    {
        return 0U;
    }

    if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW2_PIN) == GPIO_PIN_SET)
    {
        return 1U;
    }

    if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW3_PIN) == GPIO_PIN_SET)
    {
        return 2U;
    }

    if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW4_PIN) == GPIO_PIN_SET)
    {
        return 3U;
    }

    return FILA_NO_DETECTADA;
}

static char leer_tecla_inmediata(void)
{
    uint8_t columna = 0U;
    uint8_t fila;

    while (columna < 4U)
    {
        activar_columna(columna);
        HAL_Delay(1U);

        fila = leer_fila();

        if (fila != FILA_NO_DETECTADA)
        {
            desactivar_columnas();
            return mapa_teclado[fila][columna];
        }

        columna++;
    }

    desactivar_columnas();
    return TECLA_SIN_PULSAR;
}

char teclado(void)
{
    char tecla_detectada;

    tecla_detectada = leer_tecla_inmediata();

    if (tecla_detectada == TECLA_SIN_PULSAR)
    {
        return TECLA_SIN_PULSAR;
    }

    HAL_Delay(30U);

    if (leer_tecla_inmediata() != tecla_detectada)
    {
        return TECLA_SIN_PULSAR;
    }

    while (leer_tecla_inmediata() != TECLA_SIN_PULSAR)
    {
        HAL_Delay(10U);
    }

    return tecla_detectada;
}
