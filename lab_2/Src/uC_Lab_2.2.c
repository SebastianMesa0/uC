#include "stm32f4xx_hal.h"

#include "ST7920_parallel.h"
#include "teclado.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define MAX_DIGITOS             16U
#define ANCHO_BARRA             6U
#define ESPACIO_BARRA          2U
#define ESCALA_NORMAL           1U
#define ESCALA_AMPLIADA         2U

#define AREA_GRAFICA_Y_INICIO  32U
#define AREA_GRAFICA_Y_FINAL   63U
#define LINEA_BASE             63U

void SystemClock_Config(void);
void MX_GPIO_Init(void);

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* --------------------------------------------------------- */
/* Funciones auxiliares para el vector de datos              */
/* --------------------------------------------------------- */

static uint8_t es_digito(char caracter)
{
    if ((caracter >= '0') && (caracter <= '9'))
    {
        return 1U;
    }

    return 0U;
}

static uint8_t longitud_vector(const char *vector)
{
    uint8_t longitud = 0U;

    while (vector[longitud] != '\0')
    {
        longitud++;
    }

    return longitud;
}

static void limpiar_vector(char *vector)
{
    uint8_t i;

    for (i = 0U; i < (MAX_DIGITOS + 1U); i++)
    {
        vector[i] = '\0';
    }
}

static void agregar_digito(char *vector, char digito)
{
    uint8_t longitud;

    longitud = longitud_vector(vector);

    if (longitud < MAX_DIGITOS)
    {
        vector[longitud] = digito;
        vector[longitud + 1U] = '\0';
    }
}

static void borrar_ultimo_digito(char *vector)
{
    uint8_t longitud;

    longitud = longitud_vector(vector);

    if (longitud > 0U)
    {
        vector[longitud - 1U] = '\0';
    }
}

static void completar_con_ceros(char *vector)
{
    uint8_t longitud;

    longitud = longitud_vector(vector);

    while (longitud < MAX_DIGITOS)
    {
        vector[longitud] = '0';
        longitud++;
    }

    vector[MAX_DIGITOS] = '\0';
}

/* --------------------------------------------------------- */
/* Funciones de texto                                        */
/* --------------------------------------------------------- */

static void mostrar_linea(uint8_t fila, const char *texto)
{
    char linea[17];

    snprintf(linea, sizeof(linea), "%-16s", texto);

    ST7920_SendString(fila, 0U, linea);
}

/*
 * Distribuye los 16 dígitos en dos líneas de 8 caracteres.
 *
 * Línea 0: dígitos 0 a 7
 * Línea 1: dígitos 8 a 15
 *
 * Los espacios representan posiciones todavía no ingresadas.
 */
static void mostrar_vector_texto(const char *vector)
{
    char linea_superior[17];
    char linea_inferior[17];
    uint8_t longitud;
    uint8_t i;

    for (i = 0U; i < 16U; i++)
    {
        linea_superior[i] = ' ';
        linea_inferior[i] = ' ';
    }

    linea_superior[16] = '\0';
    linea_inferior[16] = '\0';

    longitud = longitud_vector(vector);

    for (i = 0U; i < longitud; i++)
    {
        if (i < 8U)
        {
            linea_superior[i] = vector[i];
        }
        else
        {
            linea_inferior[i - 8U] = vector[i];
        }
    }

    /*
     * Las dos primeras líneas se utilizan para la cadena de 16 dígitos.
     */
    ST7920_GraphicMode(0);

    mostrar_linea(0U, linea_superior);
    mostrar_linea(1U, linea_inferior);
}

/* --------------------------------------------------------- */
/* Funciones del gráfico de barras                           */
/* --------------------------------------------------------- */

/*
 * Dibuja el gráfico utilizando solamente la zona inferior:
 *
 * y = 32 hasta y = 63
 *
 * Cada dígito se convierte en una barra:
 *
 *     altura = dígito x escala
 *
 * Por ejemplo:
 *
 *     dígito 9, escala x1 -> altura 9 píxeles
 *     dígito 9, escala x2 -> altura 18 píxeles
 */
static void dibujar_bargraph(const char *vector, uint8_t escala)
{
    uint8_t i;
    uint8_t longitud;
    uint8_t digito;
    uint8_t altura;
    uint16_t posicion_x;
    uint16_t posicion_y;

    longitud = longitud_vector(vector);

    ST7920_GraphicMode(1);
    ST7920_ClearBuffer();

    /*
     * Línea divisoria entre texto y gráfico.
     */
    DrawLine(
        0U,
        AREA_GRAFICA_Y_INICIO,
        127U,
        AREA_GRAFICA_Y_INICIO
    );

    /*
     * Línea base del gráfico.
     */
    DrawLine(
        0U,
        LINEA_BASE,
        127U,
        LINEA_BASE
    );

    /*
     * Dibujo de las 16 barras.
     *
     * Cada posición ocupa 8 píxeles:
     *
     *     barra de 6 píxeles
     *     separación de 2 píxeles
     */
    for (i = 0U; i < MAX_DIGITOS; i++)
    {
        digito = 0U;

        if (i < longitud)
        {
            digito = (uint8_t)(vector[i] - '0');
        }

        altura = (uint8_t)(digito * escala);

        posicion_x = (uint16_t)(1U + (i * 8U));

        if (altura > 0U)
        {
            posicion_y = (uint16_t)(LINEA_BASE - altura);

            DrawFilledRectangle(
                posicion_x,
                posicion_y,
                ANCHO_BARRA,
                altura
            );
        }
    }

    ST7920_Update();
}

/* --------------------------------------------------------- */
/* Actualización completa del dashboard                     */
/* --------------------------------------------------------- */

static void actualizar_dashboard(
    const char *vector,
    uint8_t escala)
{
    mostrar_vector_texto(vector);
    dibujar_bargraph(vector, escala);
}

/* --------------------------------------------------------- */
/* Inicialización GPIO                                       */
/* --------------------------------------------------------- */

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

/* --------------------------------------------------------- */
/* Configuración del reloj                                  */
/* --------------------------------------------------------- */

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

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        while (1)
        {
        }
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0
        ) != HAL_OK)
    {
        while (1)
        {
        }
    }
}

/* --------------------------------------------------------- */
/* Programa principal                                       */
/* --------------------------------------------------------- */

int main(void)
{
    char tecla;
    char vector[MAX_DIGITOS + 1U];
    uint8_t escala;

    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();

    limpiar_vector(vector);

    escala = ESCALA_NORMAL;

    /*
     * Inicializar pantalla ST7920.
     */
    ST7920_Init();

    /*
     * Inicialización inicial del dashboard.
     */
    actualizar_dashboard(vector, escala);

    while (1)
    {
        /*
         * La función teclado() devuelve '\0'
         * si no hay una tecla nueva.
         */
        tecla = teclado();

        /*
         * Teclas numéricas:
         * agregan dígitos hasta completar 16 posiciones.
         */
        if (es_digito(tecla) != 0U)
        {
            agregar_digito(vector, tecla);

            HAL_GPIO_TogglePin(
                GPIOB,
                GPIO_PIN_0
            );

            actualizar_dashboard(vector, escala);
        }

        /*
         * '*' borra el último dígito ingresado.
         */
        if (tecla == '*')
        {
            borrar_ultimo_digito(vector);

            HAL_GPIO_TogglePin(
                GPIOB,
                GPIO_PIN_0
            );

            actualizar_dashboard(vector, escala);
        }

        /*
         * '#' confirma el vector actual.
         *
         * En este ejercicio la confirmación
         * simplemente mantiene y redibuja los datos.
         */
        if (tecla == '#')
        {
            actualizar_dashboard(vector, escala);
        }

        /*
         * 'A' limpia completamente la pantalla y el vector.
         */
        if (tecla == 'A')
        {
            limpiar_vector(vector);

            actualizar_dashboard(vector, escala);
        }

        /*
         * 'B' cambia la escala del gráfico:
         *
         *     x1 -> x2 -> x1
         */
        if (tecla == 'B')
        {
            if (escala == ESCALA_NORMAL)
            {
                escala = ESCALA_AMPLIADA;
            }
            else
            {
                escala = ESCALA_NORMAL;
            }

            actualizar_dashboard(vector, escala);
        }

        /*
         * 'C' completa las posiciones restantes con ceros.
         */
        if (tecla == 'C')
        {
            completar_con_ceros(vector);

            actualizar_dashboard(vector, escala);
        }

        /*
         * 'D' reinicia vector y escala.
         */
        if (tecla == 'D')
        {
            limpiar_vector(vector);
            escala = ESCALA_NORMAL;

            actualizar_dashboard(vector, escala);
        }

        HAL_Delay(10U);
    }
}