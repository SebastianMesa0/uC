#include "stm32f429xx.h"

void SystemClock_Config(void);
void GPIO_Init(void);
void Delay(uint32_t count);

uint32_t entrada_sw(void);
uint32_t deco_display(uint32_t simbolo);
void salida_display(uint32_t display);

// Actividad 1, Punto 2, uControladores, Sebastian Mesa

int main(void)
{
    uint32_t entrada = 0;

    // Datos ordenados del punto 2
    uint32_t A = 2;
    uint32_t B = 4;
    uint32_t C = 10;
    uint32_t D = 45;
    uint32_t E = 46;

    // Apagamos todos los displays Anodo
    uint32_t p1 = 0xFF, p2 = 0xFF, p3 = 0xFF, p4 = 0xFF;

    SystemClock_Config();
    GPIO_Init();

    while (1)
    {
        entrada = entrada_sw();
        p1 = 0xFF;
        p2 = 0xFF;
        p3 = 0xFF;
        p4 = 0xFF;

        // Caso A <= entrada <= B : mostrar A y B -> 00AB
        if ((entrada >= A) && (entrada <= B))
        {
            p1 = deco_display(0);
            p2 = deco_display(0);
            p3 = deco_display(A);   // 2
            p4 = deco_display(B);   // 4
        }

        // Caso Entrada == C : mostrar C -> 0010
        else if (entrada == C)
        {
            p1 = deco_display(0);
            p2 = deco_display(0);
            p3 = deco_display(1);
            p4 = deco_display(0);
        }

        // Caso D <= Entrada <= E : mostrar D y E 4546
        else if ((entrada >= D) && (entrada <= E))
        {
            p1 = deco_display(4);
            p2 = deco_display(5);
            p3 = deco_display(4);
            p4 = deco_display(6);
        }


        // Multiplexado para anodo comun (ON=LOW / OFF=HIGH)

        // D0
        GPIOF->ODR |=  (1 << 12);   
        GPIOD->ODR |= (1 << 15);  
        GPIOD->ODR |= (1 << 14);   
        GPIOA->ODR |=  (1 << 7);    
        salida_display(0xFF);      
        salida_display(p1);       
        GPIOF->ODR &= ~(1 << 12);   
        Delay(900);

        // D1
        GPIOF->ODR |=  (1 << 12);   
        GPIOD->ODR |= (1 << 15);   
        GPIOD->ODR |= (1 << 14);   
        GPIOA->ODR |=  (1 << 7);   
        salida_display(0xFF);
        salida_display(p2);         
        GPIOD->ODR &= ~(1 << 15);   
        Delay(900);

        // D2
        GPIOF->ODR |=  (1 << 12);   
        GPIOD->ODR |= (1 << 15);    
        GPIOD->ODR |= (1 << 14);    
        GPIOA->ODR |=  (1 << 7);    
        salida_display(0xFF);
        salida_display(p3);         
        GPIOD->ODR &= ~(1 << 14);   
        Delay(900);

        // D3
        GPIOF->ODR |=  (1 << 12);  
        GPIOD->ODR |= (1 << 15);   
        GPIOD->ODR |= (1 << 14);   
        GPIOA->ODR |=  (1 << 7); 
        salida_display(0xFF);
        salida_display(p4);     
        GPIOA->ODR &= ~(1 << 7);
        Delay(900);
    }
}

// Decodificador para Anodo Comun

uint32_t deco_display(uint32_t simbolo)
{
    if(simbolo == 0)      return (uint32_t)(~0xBF) & 0xFF; // 0
    else if(simbolo == 1) return (uint32_t)(~0x86) & 0xFF; // 1
    else if(simbolo == 2) return (uint32_t)(~0xDB) & 0xFF; // 2
    else if(simbolo == 3) return (uint32_t)(~0xCF) & 0xFF; // 3
    else if(simbolo == 4) return (uint32_t)(~0xE6) & 0xFF; // 4
    else if(simbolo == 5) return (uint32_t)(~0xED) & 0xFF; // 5
    else if(simbolo == 6) return (uint32_t)(~0xFD) & 0xFF; // 6
    else if(simbolo == 7) return (uint32_t)(~0x87) & 0xFF; // 7
    else if(simbolo == 8) return (uint32_t)(~0xFF) & 0xFF; // 8
    else if(simbolo == 9) return (uint32_t)(~0xEF) & 0xFF; // 9
    else return 0xFF;
}

// Salida Displays
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


// Lectura Dipswitch
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

// Inicializacion de los puertos GPIO
void GPIO_Init(void)
{
    // Clock
    RCC->AHB1ENR |= (1 << 0); // GPIOA
    RCC->AHB1ENR |= (1 << 1); // GPIOB
    RCC->AHB1ENR |= (1 << 2); // GPIOC
    RCC->AHB1ENR |= (1 << 3); // GPIOD
    RCC->AHB1ENR |= (1 << 4); // GPIOE
    RCC->AHB1ENR |= (1 << 5); // GPIOF
    RCC->AHB1ENR |= (1 << 6); // GPIOG

    // Displays Apagados
    GPIOF->ODR |=  (1 << 12);
    GPIOD->ODR |= (1 << 15);
    GPIOD->ODR |= (1 << 14);
    GPIOA->ODR |=  (1 << 7);

    // Entrada SW
    GPIOF->MODER &= ~(3 << (13 * 2));
    GPIOF->MODER &= ~(3 << (14 * 2));
    GPIOF->MODER &= ~(3 << (15 * 2));

    GPIOE->MODER &= ~(3 << (9 * 2));
    GPIOE->MODER &= ~(3 << (11 * 2));
    GPIOE->MODER &= ~(3 << (13 * 2));

    GPIOG->MODER &= ~(3 << (9 * 2));
    GPIOG->MODER &= ~(3 << (14 * 2));

    // Seccion de displays como salida
    GPIOA->MODER &= ~(3 << (7 * 2));   GPIOA->MODER |= (1 << (7 * 2));   // PA7 D3
    GPIOD->MODER &= ~(3 << (14 * 2));  GPIOD->MODER |= (1 << (14 * 2));  // PD14 D2
    GPIOD->MODER &= ~(3 << (15 * 2));  GPIOD->MODER |= (1 << (15 * 2));  // PD15 D1
    GPIOF->MODER &= ~(3 << (12 * 2));  GPIOF->MODER |= (1 << (12 * 2));  // PF12 D0

    // Seccion de segmentos como salida
    GPIOB->MODER &= ~(3 << (1 * 2));   GPIOB->MODER |= (1 << (1 * 2));   // PB1 DP
    GPIOB->MODER &= ~(3 << (2 * 2));   GPIOB->MODER |= (1 << (2 * 2));   // PB2 E
    GPIOB->MODER &= ~(3 << (6 * 2));   GPIOB->MODER |= (1 << (6 * 2));   // PB6 D

    GPIOC->MODER &= ~(3 << (2 * 2));   GPIOC->MODER |= (1 << (2 * 2));   // PC2 G

    GPIOD->MODER &= ~(3 << (11 * 2));  GPIOD->MODER |= (1 << (11 * 2));  // PD11 A
    GPIOD->MODER &= ~(3 << (12 * 2));  GPIOD->MODER |= (1 << (12 * 2));  // PD12 F
    GPIOD->MODER &= ~(3 << (13 * 2));  GPIOD->MODER |= (1 << (13 * 2));  // PD13 B

    GPIOF->MODER &= ~(3 << (4 * 2));   GPIOF->MODER |= (1 << (4 * 2));   // PF4 C

    // Estado inicial
    salida_display(0xFF); // segmentos apagados (ánodo común)

}


// Delay
void Delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0; i < count; i++) 
    { __NOP();
     }
}

// Clock
void SystemClock_Config(void)
{
 
}
