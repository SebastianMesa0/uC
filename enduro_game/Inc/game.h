#ifndef GAME_H
#define GAME_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* =========================================================
 * Pantalla
 * ========================================================= */
#define GAME_WIDTH                  128U
#define GAME_HEIGHT                  64U

/* Zona superior y HUD inferior. */
#define GAME_HORIZON_Y               17U
#define GAME_HUD_TOP                 54U
#define GAME_PLAY_BOTTOM             53U

/* =========================================================
 * Buzzer
 * ========================================================= */
#define GAME_BUZZER_PORT             GPIOB
#define GAME_BUZZER_PIN              GPIO_PIN_0

/* =========================================================
 * Vidas
 * ========================================================= */
#define GAME_INITIAL_LIVES            3U

#define LIFE1_PORT                   GPIOF
#define LIFE1_PIN                    GPIO_PIN_13
#define LIFE2_PORT                   GPIOF
#define LIFE2_PIN                    GPIO_PIN_14
#define LIFE3_PORT                   GPIOF
#define LIFE3_PIN                    GPIO_PIN_15

/* =========================================================
 * Niveles
 * ========================================================= */
#define GAME_LEVEL_1                  1U
#define GAME_LEVEL_2                  2U
#define GAME_LEVEL_3                  3U

#define GAME_LEVEL_2_SCORE          500U
#define GAME_LEVEL_3_SCORE         1200U
#define GAME_WIN_SCORE             1500U

/* =========================================================
 * Interfaz pública
 * ========================================================= */
void Game_GPIO_Init(void);
void Game_Init(void);
void Game_Update(uint32_t now);
void Game_Render(uint32_t now);

#endif
