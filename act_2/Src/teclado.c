#include "teclado.h"

#define TECLA_SIN_PULSAR    '\0'
#define FILA_NO_DETECTADA   255

static const char mapa_teclado[4][4] =
{
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

/*
 * Desactiva todas las columnas.
 */
static void desactivar_columnas(void)
{
    HAL_GPIO_WritePin(GPIOE,
                      TECLADO_COL_MASK,
                      GPIO_PIN_RESET);
}

/*
 * Activa una columna usando únicamente if.
 */
static void activar_columna(uint8_t columna)
{
    desactivar_columnas();

    if (columna == 0)
    {
        HAL_GPIO_WritePin(GPIOE,
                          TECLADO_COL1_PIN,
                          GPIO_PIN_SET);
    }

    if (columna == 1)
    {
        HAL_GPIO_WritePin(GPIOE,
                          TECLADO_COL2_PIN,
                          GPIO_PIN_SET);
    }

    if (columna == 2)
    {
        HAL_GPIO_WritePin(GPIOE,
                          TECLADO_COL3_PIN,
                          GPIO_PIN_SET);
    }

    if (columna == 3)
    {
        HAL_GPIO_WritePin(GPIOE,
                          TECLADO_COL4_PIN,
                          GPIO_PIN_SET);
    }
}

/*
 * Lee la fila activa.
 *
 * Retorna:
 *   0, 1, 2 o 3 si detecta una fila.
 *   255 si no hay ninguna tecla presionada.
 */
static uint8_t leer_fila(void)
{
    if (HAL_GPIO_ReadPin(GPIOE, TECLADO_ROW1_PIN) == GPIO_PIN_SET)
    {
        return 0;
    }

    if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW2_PIN) == GPIO_PIN_SET)
    {
        return 1;
    }

    if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW3_PIN) == GPIO_PIN_SET)
    {
        return 2;
    }

    if (HAL_GPIO_ReadPin(GPIOF, TECLADO_ROW4_PIN) == GPIO_PIN_SET)
    {
        return 3;
    }

    return FILA_NO_DETECTADA;
}

/*
 * Realiza una lectura instantánea del teclado.
 *
 * No tiene antirrebote ni espera de liberación.
 */
static char leer_tecla_inmediata(void)
{
    uint8_t columna = 0;
    uint8_t fila = FILA_NO_DETECTADA;
    char tecla = TECLA_SIN_PULSAR;

    while (columna < 4)
    {
        activar_columna(columna);

        /*
         * Pequeño tiempo para estabilizar la señal.
         */
        HAL_Delay(1);

        fila = leer_fila();

        if (fila != FILA_NO_DETECTADA)
        {
            tecla = mapa_teclado[fila][columna];

            desactivar_columnas();

            return tecla;
        }

        columna++;
    }

    desactivar_columnas();

    return TECLA_SIN_PULSAR;
}

/*
 * Lee una tecla nueva una sola vez.
 *
 * La función:
 * 1. Detecta una tecla.
 * 2. Espera 30 ms para eliminar rebotes.
 * 3. Confirma que sigue siendo la misma tecla.
 * 4. Espera hasta que el usuario la libere.
 * 5. Devuelve la tecla solamente una vez.
 */
char teclado(void)
{
    char tecla_detectada;
    char tecla_confirmada;

    tecla_detectada = leer_tecla_inmediata();

    if (tecla_detectada == TECLA_SIN_PULSAR)
    {
        return TECLA_SIN_PULSAR;
    }

    /*
     * Antirrebote.
     */
    HAL_Delay(30);

    tecla_confirmada = leer_tecla_inmediata();

    if (tecla_confirmada != tecla_detectada)
    {
        return TECLA_SIN_PULSAR;
    }

    /*
     * Esperar a que la tecla sea liberada.
     *
     * Esto evita que una tecla mantenida produzca múltiples valores.
     */
    while (leer_tecla_inmediata() != TECLA_SIN_PULSAR)
    {
        HAL_Delay(10);
    }

    return tecla_confirmada;
}
