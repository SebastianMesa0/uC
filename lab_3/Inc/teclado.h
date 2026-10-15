#ifndef TECLADO_H_
#define TECLADO_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* Columnas del teclado matricial */
#define TECLADO_COL1_PIN    GPIO_PIN_2   // PE2
#define TECLADO_COL2_PIN    GPIO_PIN_4   // PE4
#define TECLADO_COL3_PIN    GPIO_PIN_5   // PE5
#define TECLADO_COL4_PIN    GPIO_PIN_6   // PE6

/* Filas del teclado matricial */
#define TECLADO_ROW1_PIN    GPIO_PIN_3   // PE3
#define TECLADO_ROW2_PIN    GPIO_PIN_8   // PF8
#define TECLADO_ROW3_PIN    GPIO_PIN_7   // PF7
#define TECLADO_ROW4_PIN    GPIO_PIN_9   // PF9

#define TECLADO_COL_MASK    (TECLADO_COL1_PIN | \
                             TECLADO_COL2_PIN | \
                             TECLADO_COL3_PIN | \
                             TECLADO_COL4_PIN)

/*
 * Devuelve:
 *   '0' a '9', 'A' a 'D', '*' o '#'
 *   '\0' cuando no hay una tecla nueva presionada
 */
char teclado(void);

#endif /* TECLADO_H_ */
