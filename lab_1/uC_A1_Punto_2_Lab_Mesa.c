#include "stm32f429xx.h"

// Prototipos
void SystemClock_Config(void);
void GPIO_Init(void);
void Delay(uint32_t count);
uint32_t entrada_sw(void);
uint32_t deco_display(uint32_t simbolo);
void salida_display(uint32_t display);

int main(void)
{
    uint32_t entrada = 0;

    // Punto 2
    uint32_t A = 2;
    uint32_t B = 4;
    uint32_t C = 10;
    uint32_t D = 45;
    uint32_t E = 46;

    SystemClock_Config();
    GPIO_Init();

    while (1)
    {
        uint32_t p1 = 0xFF, p2 = 0xFF, p3 = 0xFF, p4 = 0xFF; // apagado por defecto

        entrada = entrada_sw();

        // Caso A<=Entrada<=B -> [Ab]
        if ((entrada >= A) && (entrada <= B))
        {
            p1 = deco_display(10); // [
            p2 = deco_display(11); // A
            p3 = deco_display(12); // b
            p4 = deco_display(13); // ]
        }
        // Caso Entrada==C -> [CC]
        else if (entrada == C)
        {
            p1 = deco_display(10); // [
            p2 = deco_display(14); // C
            p3 = deco_display(14); // C
            p4 = deco_display(13); // ]
        }
        // Caso D<=Entrada<=E -> [DE]
        else if ((entrada >= D) && (entrada <= E))
        {
            p1 = deco_display(10); // [
            p2 = deco_display(15); // D
            p3 = deco_display(16); // E
            p4 = deco_display(13); // ]
        }
        // otro valor -> todo apagado (ya está en 0xFF)

        //================================================
        // MULTIPLEXADO RÁPIDO (ON=LOW / OFF=HIGH)
        //================================================

        // D0
        GPIOF->ODR |=  (1 << 12);   // D0 OFF
        GPIOD->ODR |= (1 << 15);    // D1 OFF
        GPIOD->ODR |= (1 << 14);    // D2 OFF
        GPIOA->ODR |=  (1 << 7);    // D3 OFF
        salida_display(0xFF);       // segmentos OFF
        salida_display(p1);         // patrón D0
        GPIOF->ODR &= ~(1 << 12);   // D0 ON
        Delay(900);

        // D1
        GPIOF->ODR |=  (1 << 12);   // D0 OFF
        GPIOD->ODR |= (1 << 15);    // D1 OFF
        GPIOD->ODR |= (1 << 14);    // D2 OFF
        GPIOA->ODR |=  (1 << 7);    // D3 OFF
        salida_display(0xFF);
        salida_display(p2);         // patrón D1
        GPIOD->ODR &= ~(1 << 15);   // D1 ON
        Delay(900);

        // D2
        GPIOF->ODR |=  (1 << 12);   // D0 OFF
        GPIOD->ODR |= (1 << 15);    // D1 OFF
        GPIOD->ODR |= (1 << 14);    // D2 OFF
        GPIOA->ODR |=  (1 << 7);    // D3 OFF
        salida_display(0xFF);
        salida_display(p3);         // patrón D2
        GPIOD->ODR &= ~(1 << 14);   // D2 ON
        Delay(900);

        // D3
        GPIOF->ODR |=  (1 << 12);   // D0 OFF
        GPIOD->ODR |= (1 << 15);    // D1 OFF
        GPIOD->ODR |= (1 << 14);    // D2 OFF
        GPIOA->ODR |=  (1 << 7);    // D3 OFF
        salida_display(0xFF);
        salida_display(p4);         // patrón D3
        GPIOA->ODR &= ~(1 << 7);    // D3 ON
        Delay(900);
    }
}

uint32_t deco_display(uint32_t simbolo)
{
    // 0..9 (ánodo común: invertidos)
    if(simbolo == 0)       return (uint32_t)(~0xBF) & 0xFF;
    else if(simbolo == 1)  return (uint32_t)(~0x86) & 0xFF;
    else if(simbolo == 2)  return (uint32_t)(~0xDB) & 0xFF;
    else if(simbolo == 3)  return (uint32_t)(~0xCF) & 0xFF;
    else if(simbolo == 4)  return (uint32_t)(~0xE6) & 0xFF;
    else if(simbolo == 5)  return (uint32_t)(~0xED) & 0xFF;
    else if(simbolo == 6)  return (uint32_t)(~0xFD) & 0xFF;
    else if(simbolo == 7)  return (uint32_t)(~0x87) & 0xFF;
    else if(simbolo == 8)  return (uint32_t)(~0xFF) & 0xFF;
    else if(simbolo == 9)  return (uint32_t)(~0xEF) & 0xFF;

    // especiales
    else if(simbolo == 10) return (uint32_t)(~0x39) & 0xFF; // [
    else if(simbolo == 11) return (uint32_t)(~0xF7) & 0xFF; // A
    else if(simbolo == 12) return (uint32_t)(~0xFC) & 0xFF; // b
    else if(simbolo == 13) return (uint32_t)(~0x0F) & 0xFF; // ]
    else if(simbolo == 14) return (uint32_t)(~0xB9) & 0xFF; // C
    else if(simbolo == 15) return (uint32_t)(~0xDE) & 0xFF; // D aprox
    else if(simbolo == 16) return (uint32_t)(~0xF9) & 0xFF; // E
    else return 0xFF;
}

void salida_display(uint32_t display)
{
    uint32_t SA = 0, SB = 0, SC = 0, SD = 0;
    uint32_t SE = 0, SF = 0, SG = 0, DP = 0;

    DP = (display & 0x80) >> 0x06; // DP7 - PB1
    SG = (display & 0x40) >> 0x04; // SG6 - PC2
    SF = (display & 0x20) << 0x07; // SF5 - PD12
    SE = (display & 0x10) >> 0x02; // SE4 - PB2
    SD = (display & 0x08) << 0x03; // SD3 - PB6
    SC = (display & 0x04) << 0x02; // SC2 - PF4
    SB = (display & 0x02) << 0x0C; // SB1 - PD13
    SA = (display & 0x01) << 0x0B; // SA0 - PD11

    GPIOB->ODR &= ~((1 << 1) + (1 << 2) + (1 << 6));
    GPIOB->ODR |= DP + SE + SD;

    GPIOC->ODR &= ~((1 << 2));
    GPIOC->ODR |= SG;

    GPIOD->ODR &= ~((1 << 11) + (1 << 12) + (1 << 13));
    GPIOD->ODR |= SA + SB + SF;

    GPIOF->ODR &= ~((1 << 4));
    GPIOF->ODR |= SC;
}

uint32_t entrada_sw(void)
{
    uint32_t entrada = 0;

    if(GPIOF->IDR & (1 << 13)) entrada |= (1 << 0); // SW0
    if(GPIOE->IDR & (1 <<  9)) entrada |= (1 << 1); // SW1
    if(GPIOE->IDR & (1 << 11)) entrada |= (1 << 2); // SW2
    if(GPIOF->IDR & (1 << 14)) entrada |= (1 << 3); // SW3
    if(GPIOE->IDR & (1 << 13)) entrada |= (1 << 4); // SW4
    if(GPIOF->IDR & (1 << 15)) entrada |= (1 << 5); // SW5
    if(GPIOG->IDR & (1 << 14)) entrada |= (1 << 6); // SW6
    if(GPIOG->IDR & (1 <<  9)) entrada |= (1 << 7); // SW7

    return entrada;
}

void GPIO_Init(void)
{
    // Habilitar relojes
    RCC->AHB1ENR |= (1 << 0); // GPIOA
    RCC->AHB1ENR |= (1 << 1); // GPIOB
    RCC->AHB1ENR |= (1 << 2); // GPIOC
    RCC->AHB1ENR |= (1 << 3); // GPIOD
    RCC->AHB1ENR |= (1 << 4); // GPIOE
    RCC->AHB1ENR |= (1 << 5); // GPIOF
    RCC->AHB1ENR |= (1 << 6); // GPIOG

    //========================
    // SWITCHES entradas
    //========================
    GPIOF->MODER &= ~(3 << (13 * 2));
    GPIOF->MODER &= ~(3 << (14 * 2));
    GPIOF->MODER &= ~(3 << (15 * 2));

    GPIOE->MODER &= ~(3 << (9 * 2));
    GPIOE->MODER &= ~(3 << (11 * 2));
    GPIOE->MODER &= ~(3 << (13 * 2));

    GPIOG->MODER &= ~(3 << (9 * 2));
    GPIOG->MODER &= ~(3 << (14 * 2));

    //========================
    // Selección displays (salida)
    //========================
    GPIOA->MODER &= ~(3 << (7 * 2));
    GPIOA->MODER |=  (1 << (7 * 2));   // PA7 D3

    GPIOD->MODER &= ~(3 << (14 * 2));
    GPIOD->MODER |=  (1 << (14 * 2));  // PD14 D2

    GPIOD->MODER &= ~(3 << (15 * 2));
    GPIOD->MODER |=  (1 << (15 * 2));  // PD15 D1

    GPIOF->MODER &= ~(3 << (12 * 2));
    GPIOF->MODER |=  (1 << (12 * 2));  // PF12 D0

    //========================
    // Segmentos (salida)
    //========================
    GPIOB->MODER &= ~(3 << (1 * 2));   GPIOB->MODER |= (1 << (1 * 2)); // PB1 DP
    GPIOB->MODER &= ~(3 << (2 * 2));   GPIOB->MODER |= (1 << (2 * 2)); // PB2 E
    GPIOB->MODER &= ~(3 << (6 * 2));   GPIOB->MODER |= (1 << (6 * 2)); // PB6 D

    GPIOC->MODER &= ~(3 << (2 * 2));   GPIOC->MODER |= (1 << (2 * 2)); // PC2 G

    GPIOD->MODER &= ~(3 << (11 * 2));  GPIOD->MODER |= (1 << (11 * 2)); // PD11 A
    GPIOD->MODER &= ~(3 << (12 * 2));  GPIOD->MODER |= (1 << (12 * 2)); // PD12 F
    GPIOD->MODER &= ~(3 << (13 * 2));  GPIOD->MODER |= (1 << (13 * 2)); // PD13 B

    GPIOF->MODER &= ~(3 << (4 * 2));   GPIOF->MODER |= (1 << (4 * 2)); // PF4 C

    // Estado inicial
    salida_display(0xFF);

    GPIOF->ODR &= ~(1 << 12);
    GPIOD->ODR &= ~(1 << 15);
    GPIOD->ODR &= ~(1 << 14);
    GPIOA->ODR &= ~(1 << 7);
}

void Delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0; i < count; i++) { __NOP(); }
}

void SystemClock_Config(void)
{
    // Manteniendo tu base
}
