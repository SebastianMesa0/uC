#ifndef MAIN_H_
#define MAIN_H_

#include "stm32f4xx_hal.h"

/* Segmentos (A-B-C-D-E-F-G-DP) */
#define SEG_PORT            GPIOC
#define SEG_A_PIN           GPIO_PIN_0
#define SEG_B_PIN           GPIO_PIN_1
#define SEG_C_PIN           GPIO_PIN_2
#define SEG_D_PIN           GPIO_PIN_3
#define SEG_E_PIN           GPIO_PIN_4
#define SEG_F_PIN           GPIO_PIN_5
#define SEG_G_PIN           GPIO_PIN_6
#define SEG_DP_PIN          GPIO_PIN_7
#define SEG_ALL_PINS        (SEG_A_PIN | SEG_B_PIN | SEG_C_PIN | SEG_D_PIN | \
                             SEG_E_PIN | SEG_F_PIN | SEG_G_PIN | SEG_DP_PIN)

/* Selección de dígitos D1..D4 */
#define DIGIT_PORT          GPIOD
#define DIGIT_1_PIN         GPIO_PIN_0
#define DIGIT_2_PIN         GPIO_PIN_1
#define DIGIT_3_PIN         GPIO_PIN_2
#define DIGIT_4_PIN         GPIO_PIN_3
#define DIGIT_ALL_PINS      (DIGIT_1_PIN | DIGIT_2_PIN | DIGIT_3_PIN | DIGIT_4_PIN)

/* Tipo de módulo 4x7 segmentos */
#define DISPLAY_ACTIVE_HIGH 1U

#endif /* MAIN_H_ */
