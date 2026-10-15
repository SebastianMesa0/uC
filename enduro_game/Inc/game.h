#ifndef GAME_H
#define GAME_H

#include "stm32f4xx_hal.h"
#include <stdint.h>


#define GAME_WIDTH              128U
#define GAME_HEIGHT             64U

#define GAME_HORIZON_Y          17U
#define GAME_HUD_TOP            54U
#define GAME_PLAY_BOTTOM        53U


#define GAME_BUZZER_PORT        GPIOB
#define GAME_BUZZER_PIN         GPIO_PIN_0

#define GAME_BUZZER_ON_STATE    GPIO_PIN_SET
#define GAME_BUZZER_OFF_STATE   GPIO_PIN_RESET


#define GAME_INITIAL_LIVES      3U

#define LIFE1_PORT              GPIOD
#define LIFE1_PIN               GPIO_PIN_2

#define LIFE2_PORT              GPIOG
#define LIFE2_PIN               GPIO_PIN_2

#define LIFE3_PORT              GPIOG
#define LIFE3_PIN               GPIO_PIN_3


#define GAME_LEVEL_1            1U
#define GAME_LEVEL_2            2U
#define GAME_LEVEL_3            3U

#define GAME_LEVEL_2_SCORE      500U
#define GAME_LEVEL_3_SCORE      1200U
#define GAME_WIN_SCORE          1500U


#define GAME_VEHICLE_WIDTH      11U
#define GAME_VEHICLE_HEIGHT     7U

#define GAME_PLAYER_Y           46
#define GAME_PLAYER_START_X     59
#define GAME_PLAYER_STEP        3


void Game_GPIO_Init(void);
void Game_Init(void);
void Game_Update(uint32_t now);
void Game_Render(uint32_t now);

#endif
