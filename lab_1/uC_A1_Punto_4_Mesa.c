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
    // Acotación requerida
    const uint32_t MIN_CONT = 1446;
    const uint32_t MAX_CONT = 1566;

    // Inicia en 1446 y se congela cuando no hay pulsación
    uint32_t contador = MIN_CONT;

    uint32_t d1, d2, d3, d4;

    // Estados previos (activo en bajo: 1 suelto, 0 presionado)
    uint8_t p1_prev = 1, p2_prev = 1, p3_prev = 1, p4_prev = 1;
    uint8_t p1_now, p2_now, p3_now, p4_now;

    SystemClock_Config();
    GPIO_Init();

    while (1)
    {
        //================================================
        // LECTURA ACTUAL DE PULSADORES
        // P1 -> PB4 : +1
        // P2 -> PA4 : -1
        // P3 -> PB3 : +2
        // P4 -> PA6 : -2
        // Activo en bajo
        //================================================
        p1_now = (GPIOB->IDR & (1 << 4)) ? 1 : 0;
        p2_now = (GPIOA->IDR & (1 << 4)) ? 1 : 0;
        p3_now = (GPIOB->IDR & (1 << 3)) ? 1 : 0;
        p4_now = (GPIOA->IDR & (1 << 6)) ? 1 : 0;

        //================================================
        // FLANCO DE BAJADA + DEBOUNCE (una acción por pulsación)
        //================================================

        // P1: +1
        if ((p1_prev == 1) && (p1_now == 0))
        {
            Delay(25000); // debounce
            if ((GPIOB->IDR & (1 << 4)) == 0)
            {
                if (contador < MAX_CONT) contador++;
                if (contador > MAX_CONT) contador = MAX_CONT;
            }
        }

        // P2: -1
        if ((p2_prev == 1) && (p2_now == 0))
        {
            Delay(25000); // debounce
            if ((GPIOA->IDR & (1 << 4)) == 0)
            {
                if (contador > MIN_CONT) contador--;
                if (contador < MIN_CONT) contador = MIN_CONT;
            }
        }

        // P3: +2
        if ((p3_prev == 1) && (p3_now == 0))
        {
            Delay(25000); // debounce
            if ((GPIOB->IDR & (1 << 3)) == 0)
            {
                if (contador <= (MAX_CONT - 2)) contador += 2;
                else contador = MAX_CONT;
            }
        }

        // P4: -2
        if ((p4_prev == 1) && (p4_now == 0))
        {
            Delay(25000); // debounce
            if ((GPIOA->IDR & (1 << 6)) == 0)
            {
                if (contador >= (MIN_CONT + 2)) contador -= 2;
                else contador = MIN_CONT;
            }
        }

        // Actualizar estados previos
        p1_prev = p1_now;
        p2_prev = p2_now;
        p3_prev = p3_now;
        p4_prev = p4_now;

        //================================================
        // Sin pulsación: no cambiar contador (queda congelado)
        //================================================

        // Separar en 4 dígitos
        d1 = (contador / 1000) % 10;
        d2 = (contador / 100) % 10;
        d3 = (contador / 10) % 10;
        d4 = contador % 10;

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

    // Pulsadores entrada: PB4, PA4, PB3, PA6
    GPIOB->MODER &= ~(3 << (4 * 2)); // PB4
    GPIOA->MODER &= ~(3 << (4 * 2)); // PA4
    GPIOB->MODER &= ~(3 << (3 * 2)); // PB3
    GPIOA->MODER &= ~(3 << (6 * 2)); // PA6

    // Pull-up interno (recomendado para activo en bajo)
    GPIOB->PUPDR &= ~(3 << (4 * 2));
    GPIOA->PUPDR &= ~(3 << (4 * 2));
    GPIOB->PUPDR &= ~(3 << (3 * 2));
    GPIOA->PUPDR &= ~(3 << (6 * 2));

    GPIOB->PUPDR |=  (1 << (4 * 2)); // PB4 pull-up
    GPIOA->PUPDR |=  (1 << (4 * 2)); // PA4 pull-up
    GPIOB->PUPDR |=  (1 << (3 * 2)); // PB3 pull-up
    GPIOA->PUPDR |=  (1 << (6 * 2)); // PA6 pull-up

    // Displays select salida
    GPIOA->MODER &= ~(3 << (7 * 2));   GPIOA->MODER |= (1 << (7 * 2));   // PA7 D3
    GPIOD->MODER &= ~(3 << (14 * 2));  GPIOD->MODER |= (1 << (14 * 2));  // PD14 D2
    GPIOD->MODER &= ~(3 << (15 * 2));  GPIOD->MODER |= (1 << (15 * 2));  // PD15 D1
    GPIOF->MODER &= ~(3 << (12 * 2));  GPIOF->MODER |= (1 << (12 * 2));  // PF12 D0

    // Segmentos salida
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

void Delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0; i < count; i++) { __NOP(); }
}

void SystemClock_Config(void)
{
    // Conservado según tu proyecto
}
