#include "stm32f4xx_hal.h"
#include "teclado.h"
#include "ST7920_parallel.h"
#include "game.h"

/* Buzzer en PC8 */
#define BUZZER_PIN GPIO_PIN_8

/* Prototipos */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void Error_Handler(void);

#define DISPLAY_PERIOD 120U

int main(void)
{
    uint32_t ahora;
    uint32_t ultimo_dibujo;
    char tecla;

    HAL_Init();

    SystemClock_Config();
    MX_GPIO_Init();

    /* Inicializar la pantalla */
    ST7920_Init();

    /* Activar el modo grafico GDRAM */
    ST7920_GraphicMode(1);

    /* Inicializar tablero de bolos */
    Game_Init();

    /* Dibujar menu inicial */
    Game_Draw();

    ultimo_dibujo = HAL_GetTick();

    while (1)
    {
        ahora = HAL_GetTick();

        /* Leer teclado */
        tecla = teclado();

        if (tecla != '\0')
        {
            Game_HandleKey(tecla);
        }

        /* Actualizar logica del juego */
        Game_Update(ahora);

        /* Actualizar pantalla */
        if ((ahora - ultimo_dibujo) >= DISPLAY_PERIOD)
        {
            Game_Draw();
            ultimo_dibujo = ahora;
        }
    }
}

/*====================================================
 * CONFIGURACION GPIO
 *====================================================*/
void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Habilitar relojes de los puertos */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    /*================================================
     * LCD Y BUZZER
     * LCD: PC9, PC10, PC11
     * BUZZER: PC8
     *================================================*/
    GPIO_InitStruct.Pin =
        RS_PIN | RW_PIN | E_PIN | BUZZER_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_GPIO_WritePin(
        GPIOC,
        RS_PIN | RW_PIN | E_PIN | BUZZER_PIN,
        GPIO_PIN_RESET
    );

    /*================================================
     * LCD
     * RST = PD3
     * DB7..DB4 = PD4..PD7
     * Se conserva PD2 como salida del montaje anterior.
     *================================================*/
    GPIO_InitStruct.Pin =
        GPIO_PIN_2 |
        RST_PIN |
        GPIO_PIN_4 |
        GPIO_PIN_5 |
        GPIO_PIN_6 |
        GPIO_PIN_7;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    HAL_GPIO_WritePin(
        GPIOD,
        GPIO_PIN_2 |
        RST_PIN |
        GPIO_PIN_4 |
        GPIO_PIN_5 |
        GPIO_PIN_6 |
        GPIO_PIN_7,
        GPIO_PIN_RESET
    );

    /*================================================
     * BARRA DE 8 LED
     * LED1 = PF13
     * LED2 = PE9
     * LED3 = PE11
     * LED4 = PF14
     * LED5 = PE13
     * LED6 = PF15
     * LED7 = PG14
     * LED8 = PG9
     *================================================*/

    /* LED1, LED4 y LED6: PF13, PF14 y PF15 */
    GPIO_InitStruct.Pin =
        GPIO_PIN_13 |
        GPIO_PIN_14 |
        GPIO_PIN_15;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    HAL_GPIO_WritePin(
        GPIOF,
        GPIO_PIN_13 |
        GPIO_PIN_14 |
        GPIO_PIN_15,
        GPIO_PIN_RESET
    );

    /* LED2, LED3 y LED5: PE9, PE11 y PE13 */
    GPIO_InitStruct.Pin =
        GPIO_PIN_9 |
        GPIO_PIN_11 |
        GPIO_PIN_13;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    HAL_GPIO_WritePin(
        GPIOE,
        GPIO_PIN_9 |
        GPIO_PIN_11 |
        GPIO_PIN_13,
        GPIO_PIN_RESET
    );

    /* LED7 y LED8: PG14 y PG9.
       PG2 y PG3 se conservan por compatibilidad
       con el montaje anterior. */
    GPIO_InitStruct.Pin =
        GPIO_PIN_2 |
        GPIO_PIN_3 |
        GPIO_PIN_9 |
        GPIO_PIN_14;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    HAL_GPIO_WritePin(
        GPIOG,
        GPIO_PIN_2 |
        GPIO_PIN_3 |
        GPIO_PIN_9 |
        GPIO_PIN_14,
        GPIO_PIN_RESET
    );

    /*================================================
     * TECLADO MATRICIAL
     * Columnas: PE2, PE4, PE5 y PE6
     *================================================*/
    GPIO_InitStruct.Pin =
        TECLADO_COL1_PIN |
        TECLADO_COL2_PIN |
        TECLADO_COL3_PIN |
        TECLADO_COL4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* Fila 1: PE3 */
    GPIO_InitStruct.Pin = TECLADO_ROW1_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* Filas 2, 3 y 4: PF8, PF7 y PF9 */
    GPIO_InitStruct.Pin =
        TECLADO_ROW2_PIN |
        TECLADO_ROW3_PIN |
        TECLADO_ROW4_PIN;

    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;

    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    /* Columnas del teclado inicialmente apagadas */
    HAL_GPIO_WritePin(
        GPIOE,
        TECLADO_COL_MASK,
        GPIO_PIN_RESET
    );

    /* Asegurar que todos los LED comiencen apagados */
    HAL_GPIO_WritePin(
        GPIOF,
        GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        GPIOE,
        GPIO_PIN_9 | GPIO_PIN_11 | GPIO_PIN_13,
        GPIO_PIN_RESET
    );

    HAL_GPIO_WritePin(
        GPIOG,
        GPIO_PIN_9 | GPIO_PIN_14,
        GPIO_PIN_RESET
    );
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

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI;

    RCC_OscInitStruct.HSIState = RCC_HSI_ON;

    RCC_OscInitStruct.HSICalibrationValue =
        RCC_HSICALIBRATION_DEFAULT;

    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_HSI;

    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_0
        ) != HAL_OK)
    {
        Error_Handler();
    }
}

/*====================================================
 * SYSTICK
 * Debe existir una sola definicion en el proyecto.
 *====================================================*/
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/*====================================================
 * ERROR FATAL
 *====================================================*/
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}