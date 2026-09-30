#include "stm32f429xx.h"
void SystemClock_Config(void);
void GPIO_Init(void);
void Delay(uint32_t count);
void salida_leds(uint32_t leds);
void board_leds(uint32_t leds);
uint32_t entrada_sw(void);

// Actividad 1, Punto 1, uControladores, Sebastian Mesa

int main(void)
{
    // Se crean 4 secuencias para diferenciar cada caso
    uint32_t entrada = 0;

    // A de izquierda a derecha
    const uint8_t seqA[8] = {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80};

    // B de derecha a izquierda
    const uint8_t seqB[8] = {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01};

    // C alternada
    const uint8_t seqC[8] = {0x01,0x04,0x02,0x08,0x10,0x40,0x20,0x80};

    // D llenado progresivo
    const uint8_t seqD[8] = {0x01,0x03,0x07,0x0F,0x1F,0x3F,0x7F,0xFF};


    // Auxiliares de secuencia
    uint32_t iA = 0, iB = 0, iC = 0, iD = 0;

    SystemClock_Config();
    GPIO_Init();

    while (1)
    {
        entrada = entrada_sw();
        
        // A
        if (entrada == 10)
        {
            salida_leds(seqA[iA]);
            iA++;
            if(iA >= 8) iA = 0;
            Delay(150000);
        }
        
        // B
        else if (entrada == 2)
        {
            salida_leds(seqB[iB]);
            iB++;
            if(iB >= 8) iB = 0;
            Delay(300000);
        }
        
        // C
        else if (entrada == 46)
        {
            salida_leds(seqC[iC]);
            iC++;
            if(iC >= 8) iC = 0;
            Delay(450000);
        }
        
        // D
        else if (entrada == 4)
        {
            salida_leds(seqD[iD]);
            iD++;
            if(iD >= 8) iD = 0;
            Delay(600000);
        }
        
        // Aoagado
        else
        {
            salida_leds(0x00);
        }
    }
}

uint32_t entrada_sw(void)
{
    uint32_t entrada=0;
    uint32_t PF13 = 0, PE9=0, PE11=0, PF14 = 0;
    uint32_t PE13 = 0, PF15=0, PG14=0, PG9 = 0;

    PF13 = (GPIOF->IDR & GPIO_IDR_ID13) >> 13;  // PF13 - SW0
    PE9  = (GPIOE->IDR & GPIO_IDR_ID9)  >>  8;  // PE9 - SW1
    PE11 = (GPIOE->IDR & GPIO_IDR_ID11) >>  9;  // PE11 - SW2
    PF14 = (GPIOF->IDR & GPIO_IDR_ID14) >> 11;  // PF14 - SW3
    PE13 = (GPIOE->IDR & GPIO_IDR_ID13) >>  9;  // PE13 - SW4
    PF15 = (GPIOF->IDR & GPIO_IDR_ID15) >> 10;  // PF15 - SW5
    PG14 = (GPIOG->IDR & GPIO_IDR_ID14) >>  8;  // PG14 - SW6
    PG9  = (GPIOG->IDR & GPIO_IDR_ID9)  >>  2;  // PG9 - SW7

    entrada = PF13 + PE9 + PE11 + PF14 + PE13 + PF15 + PG14 + PG9;
    return entrada;
}

void salida_leds(uint32_t leds)
{
    uint32_t PB11 = 0, PB10=0;
    uint32_t PE7 = 0, PE8=0, PE10=0, PE12=0, PE14=0, PE15=0;

    PB11 = (leds & 0x80) << 0x04;
    PB10 = (leds & 0x40) << 0x04;
    PE15 = (leds & 0x20) << 0x0A;
    PE14 = (leds & 0x10) << 0x0A;
    PE12 = (leds & 0x08) << 0x09;
    PE10 = (leds & 0x04) << 0x08;
    PE7  = (leds & 0x02) << 0x06;
    PE8  = (leds & 0x01) << 0x08;

    GPIOB->ODR &= ~((1 << 10) + (1 << 11));
    GPIOB->ODR |= PB11 + PB10;

    GPIOE->ODR &= ~((1 << 7) + (1 << 8) + (1 << 10) + (1 << 12) + (1 << 14) + (1 << 15));
    GPIOE->ODR |= PE15 + PE14 + PE12 + PE10 + PE7 + PE8;
}

void GPIO_Init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    GPIOA->MODER &= ~(3 << (14));
    GPIOA->MODER |=  (1 << (14));

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    GPIOB->MODER &= ~(3);
    GPIOB->MODER |= (1);
    GPIOB->MODER &= ~(3 << (2));
    GPIOB->MODER |= (1 << (2));
    GPIOB->MODER &= ~(3 << (4));
    GPIOB->MODER |= (1 << (4));
    GPIOB->MODER &= ~(3 << (12));
    GPIOB->MODER |= (1 << (12));
    GPIOB->MODER &= ~(0x0000C000);
    GPIOB->MODER |= (0x00004000);
    GPIOB->MODER &= ~(0x30000000);
    GPIOB->MODER |= (0x10000000);
    GPIOB->MODER &= ~(3 << (22));
    GPIOB->MODER |= (1 << (22));
    GPIOB->MODER &= ~(3 << (20));
    GPIOB->MODER |= (1 << (20));

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    GPIOC->MODER &= ~(3 << (4));
    GPIOC->MODER |= (1 << (4));
    GPIOC->MODER &= ~(3 << (26));
    GPIOC->MODER |= (0 << (26));

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;
    GPIOD->MODER &= ~(3 << (22));
    GPIOD->MODER |= (1 << (22));
    GPIOD->MODER &= ~(3 << (24));
    GPIOD->MODER |= (1 << (24));
    GPIOD->MODER &= ~(3 << (26));
    GPIOD->MODER |= (1 << (26));
    GPIOD->MODER &= ~(3 << (28));
    GPIOD->MODER |= (1 << (28));
    GPIOD->MODER &= ~(3 << (30));
    GPIOD->MODER |= (1 << (30));

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    (void)RCC->AHB1ENR;
    GPIOE->MODER &= ~(3 << (30)); GPIOE->MODER |= (1 << (30));
    GPIOE->MODER &= ~(3 << (28)); GPIOE->MODER |= (1 << (28));
    GPIOE->MODER &= ~(3 << (26)); GPIOE->MODER |= (0 << (26));
    GPIOE->MODER &= ~(3 << (24)); GPIOE->MODER |= (1 << (24));
    GPIOE->MODER &= ~(3 << (22)); GPIOE->MODER |= (0 << (22));
    GPIOE->MODER &= ~(3 << (20)); GPIOE->MODER |= (1 << (20));
    GPIOE->MODER &= ~(3 << (18)); GPIOE->MODER |= (0 << (18));
    GPIOE->MODER &= ~(3 << (16)); GPIOE->MODER |= (1 << (16));
    GPIOE->MODER &= ~(3 << (14)); GPIOE->MODER |= (1 << (14));

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;
    (void)RCC->AHB1ENR;
    GPIOF->MODER &= ~(3 << (8));
    GPIOF->MODER |=  (1 << (8));
    GPIOF->MODER &= ~(3 << (24));
    GPIOF->MODER |=  (1 << (24));
    GPIOF->MODER &= ~(3 << (26));
    GPIOF->MODER |= (0 << (26));
    GPIOF->MODER &= ~(3 << (28));
    GPIOF->MODER |= (0 << (28));
    GPIOF->MODER &= ~(3 << (30));
    GPIOF->MODER |= (0 << (30));

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
    (void)RCC->AHB1ENR;
    GPIOG->MODER &= ~(3 << (28));
    GPIOG->MODER |= (0 << (28));
    GPIOG->MODER &= ~(3 << (18));
    GPIOG->MODER |= (0 << (18));
}

void SystemClock_Config(void)
{
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI);
}

void Delay(uint32_t count)
{
    volatile uint32_t i;
    for (i = 0; i < count; i++)
    { __NOP();
     }
}
