#include "stm32f4xx_hal.h"

#include "ST7920_parallel.h"
#include "teclado.h"

#include "AnimacionUno.h"
#include "AnimacionDos.h"
#include "AnimacionTres.h"
#include "AnimacionCuatro.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_DIGITOS             15U
#define FRAME_SIZE_ST7920       1024U
#define TIEMPO_ENTRE_FRAMES_MS  100U
#define REPETICIONES_ANIMACION  2U

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/*
 * Verifica si un carácter es un dígito decimal.
 */
static uint8_t es_digito(char caracter)
{
    if ((caracter >= '0') && (caracter <= '9'))
    {
        return 1U;
    }

    return 0U;
}

/*
 * Calcula la longitud de una cadena.
 */
static uint8_t longitud_cadena(const char *cadena)
{
    uint8_t longitud = 0U;

    while (cadena[longitud] != '\0')
    {
        longitud++;
    }

    return longitud;
}

/*
 * Limpia la cadena del número.
 */
static void limpiar_numero(char *numero)
{
    numero[0] = '\0';
}

/*
 * Agrega un dígito al número.
 */
static void agregar_digito(char *numero, char digito)
{
    uint8_t longitud;

    longitud = longitud_cadena(numero);

    if (longitud < MAX_DIGITOS)
    {
        numero[longitud] = digito;
        numero[longitud + 1U] = '\0';
    }
}

/*
 * Muestra una línea completa de 16 caracteres.
 *
 * Los espacios al final limpian cualquier carácter
 * que haya quedado de un mensaje anterior.
 */
static void mostrar_linea(uint8_t fila, const char *texto)
{
    char linea[17];

    snprintf(linea, sizeof(linea), "%-16s", texto);

    ST7920_SendString(fila, 0U, linea);
}

/*
 * Muestra la pantalla inicial en modo texto.
 */
static void mostrar_pantalla_inicial(void)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    mostrar_linea(0U, "TECLADO 4x4");
    mostrar_linea(1U, "Ingrese numero");
    mostrar_linea(2U, "");
    mostrar_linea(3U, "Confirme con #");
}

/*
 * Reproduce una animación dos veces.
 *
 * Cada frame tiene 1024 bytes:
 *
 * 128 x 64 / 8 = 1024 bytes
 */
static void reproducir_animacion(
    const uint8_t (*frames)[FRAME_SIZE_ST7920],
    uint16_t cantidad_frames)
{
    uint16_t frame;
    uint16_t repeticion;

    /*
     * Asegurarse de estar en modo texto antes de comenzar.
     */
    ST7920_GraphicMode(0);
    ST7920_Clear();

    /*
     * Entrar en modo gráfico.
     */
    ST7920_GraphicMode(1);

    /*
     * Limpiar el buffer y actualizar físicamente la pantalla.
     * Esto evita que quede visible el texto anterior.
     */
    ST7920_ClearBuffer();
    ST7920_Update();

    repeticion = 0U;

    /*
     * Reproducir la animación dos veces.
     */
    while (repeticion < REPETICIONES_ANIMACION)
    {
        frame = 0U;

        while (frame < cantidad_frames)
        {
            ST7920_DrawBitmap(frames[frame]);

            HAL_Delay(TIEMPO_ENTRE_FRAMES_MS);

            frame++;
        }

        repeticion++;
    }

    /*
     * Regresar a modo texto.
     */
    ST7920_GraphicMode(0);
    ST7920_Clear();
}

/*
 * Muestra el marco de advertencia DATA ERROR.
 */
static void mostrar_data_error(void)
{
    /*
     * Dibujar marco en modo gráfico.
     */
    ST7920_GraphicMode(1);
    ST7920_ClearBuffer();

    /*
     * Marco exterior.
     */
    DrawRectangle(0U, 0U, 127U, 63U);

    /*
     * Marco interior.
     */
    DrawRectangle(3U, 3U, 121U, 57U);

    ST7920_Update();

    HAL_Delay(500U);

    /*
     * Mostrar mensaje en modo texto.
     */
    ST7920_GraphicMode(0);
    ST7920_Clear();

    mostrar_linea(0U, "+--------------");
    mostrar_linea(1U, "|  DATA ERROR  |");
    mostrar_linea(2U, "+--------------");
    mostrar_linea(3U, "Dato no valido");

    HAL_Delay(1500U);

    /*
     * Limpiar la pantalla antes de regresar
     * al menú inicial.
     */
    ST7920_Clear();
}

/*
 * Selecciona la animación según el número confirmado.
 *
 * 460 -> AnimacionUno
 * 445 -> AnimacionDos
 * 753 -> AnimacionTres
 * 711 -> AnimacionCuatro
 */
static void procesar_numero(const char *numero)
{
    if (strcmp(numero, "460") == 0)
    {
        reproducir_animacion(
            AnimacionUno,
            ANIMACIONUNO_NUM_FRAMES
        );

        return;
    }

    if (strcmp(numero, "445") == 0)
    {
        reproducir_animacion(
            AnimacionDos,
            ANIMACIONDOS_NUM_FRAMES
        );

        return;
    }

    if (strcmp(numero, "753") == 0)
    {
        reproducir_animacion(
            AnimacionTres,
            ANIMACIONTRES_NUM_FRAMES
        );

        return;
    }

    if (strcmp(numero, "711") == 0)
    {
        reproducir_animacion(
            AnimacionCuatro,
            ANIMACIONCUATRO_NUM_FRAMES
        );

        return;
    }

    /*
     * Cualquier otro número genera DATA ERROR.
     */
    mostrar_data_error();
}

int main(void)
{
    char tecla;
    char numero[MAX_DIGITOS + 1U];

    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();

    limpiar_numero(numero);

    /*
     * Inicializar pantalla ST7920.
     */
    ST7920_Init();

    /*
     * Mostrar pantalla inicial.
     */
    mostrar_pantalla_inicial();

    while (1)
    {
        /*
         * Leer una tecla nueva.
         *
         * teclado() devuelve '\0' si no hay
         * una tecla nueva disponible.
         */
        tecla = teclado();

        /*
         * Agregar dígitos al número.
         */
        if (es_digito(tecla) != 0U)
        {
            agregar_digito(numero, tecla);

            mostrar_linea(0U, "TECLADO 4x4");
            mostrar_linea(1U, "Numero:");
            mostrar_linea(2U, numero);
            mostrar_linea(3U, "Confirme con #");

            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
        }

        /*
         * Borrar el número utilizando '*'.
         */
        if (tecla == '*')
        {
            limpiar_numero(numero);

            mostrar_linea(0U, "TECLADO 4x4");
            mostrar_linea(1U, "Numero borrado");
            mostrar_linea(2U, "");
            mostrar_linea(3U, "Confirme con #");

            HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);
        }

        /*
         * Confirmar el número utilizando '#'.
         */
        if (tecla == '#')
        {
            if (longitud_cadena(numero) > 0U)
            {
                mostrar_linea(0U, "TECLADO 4x4");
                mostrar_linea(1U, "Procesando:");
                mostrar_linea(2U, numero);
                mostrar_linea(3U, "Espere...");

                /*
                 * Seleccionar y reproducir la animación.
                 */
                procesar_numero(numero);

                /*
                 * Limpiar la entrada.
                 */
                limpiar_numero(numero);

                /*
                 * Mostrar nuevamente la pantalla inicial.
                 */
                mostrar_pantalla_inicial();
            }
            else
            {
                /*
                 * No se permite confirmar una entrada vacía.
                 */
                mostrar_data_error();

                limpiar_numero(numero);

                mostrar_pantalla_inicial();
            }
        }

        HAL_Delay(10U);
    }
}

/*
 * Inicialización de GPIO.
 */
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /*
     * Habilitar relojes de los puertos utilizados.
     */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();

    /*
     * LED de estado en PB0.
     */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(
        GPIOB,
        GPIO_PIN_0,
        GPIO_PIN_RESET
    );

    /*
     * Señales de control del ST7920:
     *
     * RS = PC9
     * RW = PC10
     * E  = PC11
     */
    GPIO_InitStruct.Pin = RS_PIN | RW_PIN | E_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /*
     * Señales de datos y reset del ST7920:
     *
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

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /*
     * Columnas del teclado:
     *
     * C1 = PE2
     * C2 = PE4
     * C3 = PE5
     * C4 = PE6
     */
    GPIO_InitStruct.Pin = TECLADO_COL1_PIN |
                          TECLADO_COL2_PIN |
                          TECLADO_COL3_PIN |
                          TECLADO_COL4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /*
     * Todas las columnas empiezan desactivadas.
     */
    HAL_GPIO_WritePin(
        GPIOE,
        TECLADO_COL_MASK,
        GPIO_PIN_RESET
    );

    /*
     * Fila 1 = PE3.
     */
    GPIO_InitStruct.Pin = TECLADO_ROW1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /*
     * Fila 2 = PF8
     * Fila 3 = PF7
     * Fila 4 = PF9
     */
    GPIO_InitStruct.Pin = TECLADO_ROW2_PIN |
                          TECLADO_ROW3_PIN |
                          TECLADO_ROW4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);
}

/*
 * Configuración del reloj del sistema.
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    /*
     * Reloj HSI de 16 MHz sin PLL.
     */
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