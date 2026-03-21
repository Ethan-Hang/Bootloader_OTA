#include <stddef.h>

#include "stm32f4xx.h"

#include "bootmanager.h"
#include "elog.h"

void disable_all_peripherals(void)
{
    __disable_irq();

    elog_stop();
    elog_deinit();
    USART_DeInit(USART1);
    GPIO_DeInit(GPIOA);

    /* Stop SysTick to avoid bootloader tick interrupt after jump. */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    /* Disable and clear all NVIC interrupt lines. */
    for (uint32_t i = 0U; i < 8U; i++)
    {
        NVIC->ICER[i] = 0xFFFFFFFFU;
        NVIC->ICPR[i] = 0xFFFFFFFFU;
    }
    RCC_DeInit();
}

void jump_to_app(void)
{
    __IO uint32_t sp            = *(__IO uint32_t *)(ApplicationAddress);
    uint32_t      jump_addr     = 0x00;
    pFunction     jump_app_func = NULL;

    if (0x20000000 < sp && sp < 0x20020000)
    {
        disable_all_peripherals();

        // Reset Handler
        jump_addr = *(__IO uint32_t *)(ApplicationAddress + 4);

        __set_MSP(sp);

        jump_app_func = (pFunction)jump_addr;
        jump_app_func();
    }
}
