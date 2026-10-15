#ifndef BOWLING_GAME_H_
#define BOWLING_GAME_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

void Bowling_GPIO_Init(void);
void Bowling_Init(void);
void Bowling_Update(uint32_t now);
void Bowling_Render(uint32_t now);

#endif
