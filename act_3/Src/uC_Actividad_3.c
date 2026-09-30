#include "stm32f4xx_hal.h"
#include "ST7920_parallel.h"
#include "teclado.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Defiinicion de Parametros */

#define MAX_DIGITOS             4U
#define MAX_CONTADOR            9999U

#define PERIODO_CONTADOR_MS     500U
#define PERIODO_LED_MS          80U
#define NUM_LEDS                10U

#define ESTADO_NUM_A            0U
#define ESTADO_NUM_B            1U
#define ESTADO_LISTO            2U
#define ESTADO_CONTEO           3U

#define MODO_A                  1U
#define MODO_B                  2U
#define MODO_C                  3U
#define MODO_D                  4U


static GPIO_TypeDef *const led_port[NUM_LEDS] =
{
    GPIOF, GPIOE, GPIOE, GPIOF, GPIOE,
    GPIOF, GPIOG, GPIOG, GPIOE, GPIOE
};

static const uint16_t led_pin[NUM_LEDS] =
{
    GPIO_PIN_13, GPIO_PIN_9,  GPIO_PIN_11,
    GPIO_PIN_14, GPIO_PIN_13, GPIO_PIN_15,
    GPIO_PIN_14, GPIO_PIN_9,  GPIO_PIN_8,
    GPIO_PIN_7
};


static uint8_t estado = ESTADO_NUM_A;
static uint8_t modo = 0U;

static uint8_t conteo_activo = 0U;
static uint8_t pausa = 0U;

static uint16_t num_a = 0U;
static uint16_t num_b = 0U;
static uint16_t contador = 0U;

static char entrada[MAX_DIGITOS + 1U] = "";

static uint32_t tiempo_contador = 0U;
static uint32_t tiempo_led = 0U;

static uint8_t secuencia_led_activa = 0U;
static uint8_t led_actual = 0U;

/* Funciones */

void SystemClock_Config(void);
void MX_GPIO_Init(void);

static void pantalla_linea(uint8_t fila, const char *texto);
static void pantalla_inicial(void);
static void pantalla_num_a(void);
static void pantalla_num_b(void);
static void pantalla_contador(void);
static void pantalla_error(void);

static void led_apagar_todos(void);
static void led_encender(uint8_t posicion);
static void iniciar_secuencia_led(uint32_t ahora);
static void actualizar_secuencia_led(uint32_t ahora);

static void iniciar_conteo(uint8_t nuevo_modo, uint32_t ahora);
static void actualizar_contador(uint32_t ahora);
static void cancelar_conteo(void);

static void procesar_configuracion(char tecla);
static void procesar_comando(char tecla, uint32_t ahora);


void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* Pantalla */

static void pantalla_linea(uint8_t fila, const char *texto)
{
    char linea[17];

    snprintf(linea, sizeof(linea), "%-16s", texto);
    ST7920_SendString(fila, 0U, linea);
}

static void pantalla_inicial(void)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    pantalla_linea(0U, "A/B/C/D INICIO");
    pantalla_linea(1U, "CONTADOR: 0000");
    pantalla_linea(2U, "*=PAUSA");
    pantalla_linea(3U, "#=CANCELAR");
}

static void pantalla_num_a(void)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    pantalla_linea(0U, "CONFIGURACION");
    pantalla_linea(1U, "Ingrese NUM_A:");
    pantalla_linea(2U, entrada);
    pantalla_linea(3U, "#=CONFIRMAR");
}

static void pantalla_num_b(void)
{
    char linea[17];

    ST7920_GraphicMode(0);
    ST7920_Clear();

    pantalla_linea(0U, "CONFIGURACION");
    pantalla_linea(1U, "Ingrese NUM_B:");
    pantalla_linea(2U, entrada);

    snprintf(
        linea,
        sizeof(linea),
        "A=%04u #=OK",
        (unsigned int)num_a
    );

    pantalla_linea(3U, linea);
}

static void pantalla_contador(void)
{
    char linea[17];

    snprintf(
        linea,
        sizeof(linea),
        "A:%04u B:%04u",
        (unsigned int)num_a,
        (unsigned int)num_b
    );

    pantalla_linea(0U, linea);

    snprintf(
        linea,
        sizeof(linea),
        "CONTADOR: %04u",
        (unsigned int)contador
    );

    pantalla_linea(1U, linea);

    if (pausa != 0U)
    {
        pantalla_linea(2U, "ESTADO: PAUSA");
    }
    else
    {
        if (modo == MODO_A)
        {
            pantalla_linea(2U, "MODO A: +1");
        }

        if (modo == MODO_B)
        {
            pantalla_linea(2U, "MODO B: -1");
        }

        if (modo == MODO_C)
        {
            pantalla_linea(2U, "MODO C: +2");
        }

        if (modo == MODO_D)
        {
            pantalla_linea(2U, "MODO D: -2");
        }
    }

    pantalla_linea(3U, "*=PAUSA #=RESET");
}

static void pantalla_error(void)
{
    ST7920_GraphicMode(0);
    ST7920_Clear();

    pantalla_linea(0U, "ERROR DE DATOS");
    pantalla_linea(1U, "NUM_A > NUM_B");
    pantalla_linea(2U, "DATOS INVALIDOS");
    pantalla_linea(3U, "INTENTE DE NUEVO");

    HAL_Delay(1500U);

    entrada[0] = '\0';
    num_a = 0U;
    num_b = 0U;
    estado = ESTADO_NUM_A;

    pantalla_num_a();
}

/* Barra de Leds */

static void led_apagar_todos(void)
{
    HAL_GPIO_WritePin(GPIOF,
                      GPIO_PIN_13 |
                      GPIO_PIN_14 |
                      GPIO_PIN_15,
                      GPIO_PIN_SET);

    HAL_GPIO_WritePin(GPIOE,
                      GPIO_PIN_7  |
                      GPIO_PIN_8  |
                      GPIO_PIN_9  |
                      GPIO_PIN_11 |
                      GPIO_PIN_13,
                      GPIO_PIN_SET);

    HAL_GPIO_WritePin(GPIOG,
                      GPIO_PIN_9 |
                      GPIO_PIN_14,
                      GPIO_PIN_SET);
}

static void led_encender(uint8_t posicion)
{
    if (posicion < NUM_LEDS)
    {
        HAL_GPIO_WritePin(
            led_port[posicion],
            led_pin[posicion],
            GPIO_PIN_RESET
        );
    }
}

static void iniciar_secuencia_led(uint32_t ahora)
{
    secuencia_led_activa = 1U;
    led_actual = 0U;
    tiempo_led = ahora;

    led_apagar_todos();
    led_encender(led_actual);
}

static void actualizar_secuencia_led(uint32_t ahora)
{
    if (secuencia_led_activa == 0U)
    {
        return;
    }

    if ((ahora - tiempo_led) < PERIODO_LED_MS)
    {
        return;
    }

    tiempo_led = ahora;
    led_apagar_todos();
    led_actual++;

    if (led_actual >= NUM_LEDS)
    {
        secuencia_led_activa = 0U;
        led_actual = 0U;
        led_apagar_todos();
    }
    else
    {
        led_encender(led_actual);
    }
}

/* Contador */

static void iniciar_conteo(uint8_t nuevo_modo, uint32_t ahora)
{
    modo = nuevo_modo;
    conteo_activo = 1U;
    pausa = 0U;
    tiempo_contador = ahora;

    if ((modo == MODO_A) || (modo == MODO_C))
    {
        contador = num_b;
    }

    if ((modo == MODO_B) || (modo == MODO_D))
    {
        contador = num_a;
    }

    estado = ESTADO_CONTEO;
    pantalla_contador();
}

static void cancelar_conteo(void)
{
    conteo_activo = 0U;
    pausa = 0U;
    modo = 0U;
    contador = 0U;

    secuencia_led_activa = 0U;
    led_apagar_todos();

    estado = ESTADO_LISTO;
    pantalla_inicial();
}

static void actualizar_contador(uint32_t ahora)
{
    uint16_t paso = 1U;
    uint16_t siguiente = contador;
    uint8_t ascendente = 0U;
    uint8_t finalizado = 0U;

    if (conteo_activo == 0U)
    {
        return;
    }

    if (pausa != 0U)
    {
        return;
    }

    if ((ahora - tiempo_contador) < PERIODO_CONTADOR_MS)
    {
        return;
    }

    tiempo_contador = ahora;

    if ((modo == MODO_C) || (modo == MODO_D))
    {
        paso = 2U;
    }

    if ((modo == MODO_A) || (modo == MODO_C))
    {
        ascendente = 1U;
    }

    if (ascendente != 0U)
    {
        if (contador >= num_a)
        {
            siguiente = num_a;
            finalizado = 1U;
        }
        else
        {
            siguiente = contador + paso;

            if (siguiente >= num_a)
            {
                siguiente = num_a;
                finalizado = 1U;
            }
        }
    }
    else
    {
        if (contador <= num_b)
        {
            siguiente = num_b;
            finalizado = 1U;
        }
        else
        {
            siguiente = contador - paso;

            if (siguiente <= num_b)
            {
                siguiente = num_b;
                finalizado = 1U;
            }
        }
    }

    contador = siguiente;

    if ((contador % 10U) == 0U)
    {
        iniciar_secuencia_led(ahora);
    }

    pantalla_contador();

    if (finalizado != 0U)
    {
        conteo_activo = 0U;
        pausa = 0U;
    }
}

/* Configurar Teclado Input */

static void procesar_configuracion(char tecla)
{
    uint8_t longitud;

    if (estado == ESTADO_NUM_A)
    {
        if ((tecla >= '0') && (tecla <= '9'))
        {
            longitud = (uint8_t)strlen(entrada);

            if (longitud < MAX_DIGITOS)
            {
                entrada[longitud] = tecla;
                entrada[longitud + 1U] = '\0';
                pantalla_num_a();
            }
        }

        if (tecla == '*')
        {
            entrada[0] = '\0';
            pantalla_num_a();
        }

        if (tecla == '#')
        {
            if (strlen(entrada) > 0U)
            {
                num_a = (uint16_t)strtoul(
                    entrada,
                    NULL,
                    10
                );

                if (num_a <= MAX_CONTADOR)
                {
                    entrada[0] = '\0';
                    estado = ESTADO_NUM_B;
                    pantalla_num_b();
                }
                else
                {
                    pantalla_error();
                }
            }
        }

        return;
    }

    if (estado == ESTADO_NUM_B)
    {
        if ((tecla >= '0') && (tecla <= '9'))
        {
            longitud = (uint8_t)strlen(entrada);

            if (longitud < MAX_DIGITOS)
            {
                entrada[longitud] = tecla;
                entrada[longitud + 1U] = '\0';
                pantalla_num_b();
            }
        }

        if (tecla == '*')
        {
            entrada[0] = '\0';
            pantalla_num_b();
        }

        if (tecla == '#')
        {
            if (strlen(entrada) > 0U)
            {
                num_b = (uint16_t)strtoul(
                    entrada,
                    NULL,
                    10
                );

                if ((num_a > num_b) &&
                    (num_b <= MAX_CONTADOR))
                {
                    entrada[0] = '\0';
                    estado = ESTADO_LISTO;
                    pantalla_inicial();
                }
                else
                {
                    pantalla_error();
                }
            }
        }
    }
}

static void procesar_comando(char tecla, uint32_t ahora)
{
    if (estado == ESTADO_LISTO)
    {
        if (tecla == 'A')
        {
            iniciar_conteo(MODO_A, ahora);
        }

        if (tecla == 'B')
        {
            iniciar_conteo(MODO_B, ahora);
        }

        if (tecla == 'C')
        {
            iniciar_conteo(MODO_C, ahora);
        }

        if (tecla == 'D')
        {
            iniciar_conteo(MODO_D, ahora);
        }

        if (tecla == '*')
        {
            entrada[0] = '\0';
            num_a = 0U;
            num_b = 0U;
            estado = ESTADO_NUM_A;
            pantalla_num_a();
        }

        return;
    }

    if (estado == ESTADO_CONTEO)
    {
        if (tecla == '*')
        {
            pausa = pausa == 0U ? 1U : 0U;

            if (pausa == 0U)
            {
                tiempo_contador = ahora;
            }

            pantalla_contador();
        }

        if (tecla == '#')
        {
            cancelar_conteo();
        }
    }
}

/* GPIO */

void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /* LED de estado */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

    /* ST7920: RS, RW y E */
    GPIO_InitStruct.Pin = RS_PIN | RW_PIN | E_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* ST7920: RST, DB7, DB6, DB5 y DB4 */
    GPIO_InitStruct.Pin = RST_PIN |
                          GPIO_PIN_4 |
                          GPIO_PIN_5 |
                          GPIO_PIN_6 |
                          GPIO_PIN_7;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* Columnas del teclado */
    GPIO_InitStruct.Pin = TECLADO_COL1_PIN |
                          TECLADO_COL2_PIN |
                          TECLADO_COL3_PIN |
                          TECLADO_COL4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
    HAL_GPIO_WritePin(GPIOE, TECLADO_COL_MASK, GPIO_PIN_RESET);

    /* Fila 1 del teclado */
    GPIO_InitStruct.Pin = TECLADO_ROW1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* Filas 2, 3 y 4 del teclado */
    GPIO_InitStruct.Pin = TECLADO_ROW2_PIN |
                          TECLADO_ROW3_PIN |
                          TECLADO_ROW4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* Leds PF13, PF14 y PF15 */
    GPIO_InitStruct.Pin = GPIO_PIN_13 |
                          GPIO_PIN_14 |
                          GPIO_PIN_15;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* Leds PE7, PE8, PE9, PE11 y PE13 */
    GPIO_InitStruct.Pin = GPIO_PIN_7 |
                          GPIO_PIN_8 |
                          GPIO_PIN_9 |
                          GPIO_PIN_11 |
                          GPIO_PIN_13;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* Leds PG9 y PG14 */
    GPIO_InitStruct.Pin = GPIO_PIN_9 |
                          GPIO_PIN_14;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    led_apagar_todos();
}

/* Clock */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};

    __HAL_RCC_PWR_CLK_ENABLE();

    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

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

    HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0);
}

/* main */

int main(void)
{
    char tecla;
    uint32_t ahora;

    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    ST7920_Init();

    pantalla_num_a();

principal:

    ahora = HAL_GetTick();
    tecla = teclado();

    if ((estado == ESTADO_NUM_A) ||
        (estado == ESTADO_NUM_B))
    {
        procesar_configuracion(tecla);
    }

    if ((estado == ESTADO_LISTO) ||
        (estado == ESTADO_CONTEO))
    {
        procesar_comando(tecla, ahora);
    }

    ahora = HAL_GetTick();
    actualizar_contador(ahora);

    ahora = HAL_GetTick();
    actualizar_secuencia_led(ahora);

    HAL_Delay(1U);

    goto principal;
}
