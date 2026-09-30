#include "stm32f429xx.h"

//====================================================
// PROTOTIPOS
//====================================================
void SystemClock_Config(void);
void GPIO_Init(void);
void Delay(uint32_t count);

uint32_t deco_display(uint32_t simbolo);
void salida_display(uint32_t display);

//====================================================
// FUNCIÓN PRINCIPAL
//====================================================
int main(void)
{
    // Límites solicitados
    const uint32_t LIM_INF   = 1446;
    const uint32_t LIM_SUP   = 1566;
    const uint32_t ODD_INI   = 1565;
    const uint32_t ODD_FIN   = 1445;

    uint32_t valor = 0;
    uint32_t d1, d2, d3, d4;

    // Contadores por modo
    uint32_t cont_m1 = LIM_INF; // +1
    uint32_t cont_m2 = LIM_SUP; // -1
    uint32_t cont_m3 = LIM_INF; // +2 pares
    uint32_t cont_m4 = ODD_INI; // -2 impares

    uint32_t modo_actual = 0;
    uint32_t modo_anterior = 99;

    // Velocidad de conteo
    uint32_t tick = 0;
    uint32_t UMBRAL = 140; // más alto = más lento

    SystemClock_Config();
    GPIO_Init();

    while (1)
    {
        //================================================
        // LECTURA PULSADORES (ACTIVO EN BAJO)
        // PB4 -> modo 1
        // PA4 -> modo 2
        // PB3 -> modo 3
        // PA6 -> modo 4
        //================================================
        if ((GPIOB->IDR & (1 << 4)) == 0)       modo_actual = 1;
        else if ((GPIOA->IDR & (1 << 4)) == 0)  modo_actual = 2;
        else if ((GPIOB->IDR & (1 << 3)) == 0)  modo_actual = 3;
        else if ((GPIOA->IDR & (1 << 6)) == 0)  modo_actual = 4;
        else                                    modo_actual = 0;

        // Reinicio al cambiar modo
        if (modo_actual != modo_anterior)
        {
            if (modo_actual == 1) cont_m1 = LIM_INF; // 1446
            if (modo_actual == 2) cont_m2 = LIM_SUP; // 1566
            if (modo_actual == 3) cont_m3 = LIM_INF; // 1446
            if (modo_actual == 4) cont_m4 = ODD_INI; // 1565

            tick = 0;
            modo_anterior = modo_actual;
        }

        //================================================
        // ACTUALIZAR VALOR A VELOCIDAD CONTROLADA
        //================================================
        tick++;
        if (tick >= UMBRAL)
        {
            tick = 0;

            if (modo_actual == 1)
            {
                // 1446 -> 1566 de 1 en 1
                valor = cont_m1;
                if (cont_m1 < LIM_SUP) cont_m1++;
                else cont_m1 = LIM_INF;
            }
            else if (modo_actual == 2)
            {
                // 1566 -> 1446 de 1 en 1
                valor = cont_m2;
                if (cont_m2 > LIM_INF) cont_m2--;
                else cont_m2 = LIM_SUP;
            }
            else if (modo_actual == 3)
            {
                // 1446 -> 1566 en pares (+2)
                valor = cont_m3;
                if (cont_m3 + 2 <= LIM_SUP) cont_m3 += 2;
                else cont_m3 = LIM_INF;
            }
            else if (modo_actual == 4)
            {
                // 1565 -> 1445 en impares (-2)
                valor = cont_m4;
                if (cont_m4 >= ODD_FIN + 2) cont_m4 -= 2;
                else cont_m4 = ODD_INI;
            }
            else
            {
                // Ninguno activo
                valor = 0;
            }
        }

        // Forzar 0000 cuando no hay pulsador
        if (modo_actual == 0) valor = 0;

        //================================================
        // Separar en 4 dígitos
        //================================================
        d1 = (valor / 1000) % 10;
        d2 = (valor / 100) % 10;
        d3 = (valor / 10) % 10;
        d4 = valor % 10;

        //================================================
        // MULTIPLEXADO (ON=LOW / OFF=HIGH)
        //================================================

        // D0
        GPIOF->ODR |=  (1 << 12);
        GPIOD->ODR |= (1 << 15);
        GPIOD->ODR |= (1 << 14);
        GPIOA->ODR |=  (1 << 7);
        salida_display(0xFF);
        salida_display(deco_display(d1));
        GPIOF->ODR &= ~(1 << 12);
        Delay(900);

        // D1
        GPIOF->ODR |=  (1 << 12);
        GPIOD->ODR |= (1 << 15);
        GPIOD->ODR |= (1 << 14);
        GPIOA->ODR |=  (1 << 7);
        salida_display(0xFF);
        salida_display(deco_display(d2));
        GPIOD->ODR &= ~(1 << 15);
        Delay(900);

        // D2
        GPIOF->ODR |=  (1 << 12);
        GPIOD->ODR |= (1 << 15);
        GPIOD->ODR |= (1 << 14);
        GPIOA->ODR |=  (1 << 7);
        salida_display(0xFF);
        salida_display(deco_display(d3));
        GPIOD->ODR &= ~(1 << 14);
        Delay(900);

        // D3
        GPIOF->ODR |=  (1 << 12);
        GPIOD->ODR |= (1 << 15);
        GPIOD->ODR |= (1 << 14);
        GPIOA->ODR |=  (1 << 7);
        salida_display(0xFF);
        salida_display(deco_display(d4));
        GPIOA->ODR &= ~(1 << 7);
        Delay(900);
    }
}

//====================================================
// DECODIFICADOR DISPLAY (ÁNODO COMÚN)
//====================================================
uint32_t deco_display(uint32_t simbolo)
{
    if(simbolo == 0)      return (uint32_t)(~0xBF) & 0xFF;
    else if(simbolo == 1) return (uint32_t)(~0x86) & 0xFF;
    else if(simbolo == 2) return (uint32_t)(~0xDB) & 0xFF;
    else if(simbolo == 3) return (uint32_t)(~0xCF) & 0xFF;
    else if(simbolo == 4) return (uint32_t)(~0xE6) & 0xFF;
    else if(simbolo == 5) return (uint32_t)(~0xED) & 0xFF;
    else if(simbolo == 6) return (uint32_t)(~0xFD) & 0xFF;
    else if(simbolo == 7) return (uint32_t)(~0x87) & 0xFF;
    else if(simbolo == 8) return (uint32_t)(~0xFF) & 0xFF;
    else if(simbolo == 9) return (uint32_t)(~0xEF) & 0xFF;
    else return 0xFF;
}

//====================================================
// SALIDA SEGMENTOS
//====================================================
void salida_display(uint32_t display)
{
    uint32_t SA = 0, SB = 0, SC = 0, SD = 0;
    uint32_t SE = 0, SF = 0, SG = 0, DP = 0;

    DP = (display & 0x80) >> 0x06; // PB1
    SG = (display & 0x40) >> 0x04; // PC2
    SF = (display & 0x20) << 0x07; // PD12
    SE = (display & 0x10) >> 0x02; // PB2
    SD = (display & 0x08) << 0x03; // PB6
    SC = (display & 0x04) << 0x02; // PF4
    SB = (display & 0x02) << 0x0C; // PD13
    SA = (display & 0x01) << 0x0B; // PD11

    GPIOB->ODR &= ~((1 << 1) + (1 << 2) + (1 << 6));
    GPIOB->ODR |= DP + SE + SD;

    GPIOC->ODR &= ~((1 << 2));
    GPIOC->ODR |= SG;

    GPIOD->ODR &= ~((1 << 11) + (1 << 12) + (1 << 13));
    GPIOD->ODR |= SA + SB + SF;

    GPIOF->ODR &= ~((1 << 4));
    GPIOF->ODR |= SC;
}

//====================================================
// GPIO INIT
//====================================================
void GPIO_Init(void)
{
    // Clocks
    RCC->AHB1ENR |= (1 << 0); // GPIOA
    RCC->AHB1ENR |= (1 << 1); // GPIOB
    RCC->AHB1ENR |= (1 << 2); // GPIOC
    RCC->AHB1ENR |= (1 << 3); // GPIOD
    RCC->AHB1ENR |= (1 << 4); // GPIOE
    RCC->AHB1ENR |= (1 << 5); // GPIOF
    RCC->AHB1ENR |= (1 << 6); // GPIOG

    // Pulsadores (input): PB4, PA4, PB3, PA6
    GPIOB->MODER &= ~(3 << (4 * 2)); // PB4
    GPIOA->MODER &= ~(3 << (4 * 2)); // PA4
    GPIOB->MODER &= ~(3 << (3 * 2)); // PB3
    GPIOA->MODER &= ~(3 << (6 * 2)); // PA6

    // Displays select (output)
    GPIOA->MODER &= ~(3 << (7 * 2));   GPIOA->MODER |= (1 << (7 * 2));   // PA7 D3
    GPIOD->MODER &= ~(3 << (14 * 2));  GPIOD->MODER |= (1 << (14 * 2));  // PD14 D2
    GPIOD->MODER &= ~(3 << (15 * 2));  GPIOD->MODER |= (1 << (15 * 2));  // PD15 D1
    GPIOF->MODER &= ~(3 << (12 * 2));  GPIOF->MODER |= (1 << (12 * 2));  // PF12 D0

    // Segmentos (output)
    GPIOB->MODER &= ~(3 << (1 * 2));   GPIOB->MODER |= (1 << (1 * 2));   // PB1 DP
    GPIOB->MODER &= ~(3 << (2 * 2));   GPIOB->MODER |= (1 << (2 * 2));   // PB2 E
    GPIOB->MODER &= ~(3 << (6 * 2));   GPIOB->MODER |= (1 << (6 * 2));   // PB6 D
    GPIOC->MODER &= ~(3 << (2 * 2));   GPIOC->MODER |= (1 << (2 * 2));   // PC2 G
    GPIOD->MODER &= ~(3 << (11 * 2));  GPIOD->MODER |= (1 << (11 * 2));  // PD11 A
    GPIOD->MODER &= ~(3 << (12 * 2));  GPIOD->MODER |= (1 << (12 * 2));  // PD12 F
    GPIOD->MODER &= ~(3 << (13 * 2));  GPIOD->MODER |= (1 << (13 * 2));  // PD13 B
    GPIOF->MODER &= ~(3 << (4 * 2));   GPIOF->MODER |= (1 << (4 * 2));   // PF4 C

    // Estado inicial
    salida_display(0xFF);

    // Displays OFF (ON=LOW / OFF=HIGH)
    GPIOF->ODR |=  (1 << 12);
    GPIOD->ODR |= (1 << 15);
    GPIOD->ODR |= (1 << 14);
    GPIOA->ODR |=  (1 << 7);
}

//====================================================
// DELAY
//====================================================
void Delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0; i < count; i++) { __NOP(); }
}

void SystemClock_Config(void)
{
    // Conservado según tu proyecto
}