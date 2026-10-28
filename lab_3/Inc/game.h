#ifndef GAME_H_
#define GAME_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

void Game_Init(void);
void Game_HandleKey(char tecla);
void Game_Update(uint32_t ahora);
void Game_Draw(void);

#endif