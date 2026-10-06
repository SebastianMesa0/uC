# act_4_7seg_secuencia

Contenido del ejercicio para STM32 con teclado matricial y módulo 4x7 segmentos:

- `Inc/main.h`: definición de pines para segmentos y dígitos.
- `Inc/teclado.h`: pines del teclado matricial.
- `Src/main.c`: lógica principal con multiplexado y 8 pasos.
- `Src/teclado.c`: lectura del teclado con antirrebote.

Dependencias HAL requeridas en tu proyecto STM32CubeIDE:

- `stm32f4xx_hal.h`
- `stm32f4xx_hal_gpio.h`
- `stm32f4xx_hal_rcc.h`
- `stm32f4xx_hal_cortex.h`
- `stm32f4xx_hal_pwr.h`
- `system_stm32f4xx.c`
- startup del micro STM32F4 que estés usando.

> Si tu módulo 4x7 segmentos es de ánodo común, invierte niveles lógicos cambiando `DISPLAY_ACTIVE_HIGH` a `0`.
