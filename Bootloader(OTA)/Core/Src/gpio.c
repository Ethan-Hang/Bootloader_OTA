/**
  ******************************************************************************
    WeAct 微行创新
    >> 标准库 GPIO
  ******************************************************************************
  */

#include "gpio.h"
#include <string.h>

void GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* Output HSE clock on MCO1 pin(PA8)
     * ****************************************/
    /* Enable the GPIOA peripheral */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);

    /* Configure MCO1 pin(PA8) in alternate function */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* HSE clock selected to output on MCO1 pin(PA8)*/
    RCC_MCO1Config(RCC_MCO1Source_HSE, RCC_MCO1Div_1);

    /* Key on A0 pin(PA0) ***********************************/
    memset(&GPIO_InitStructure, 0, sizeof(GPIO_InitStructure));
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_0;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* LED on C13 pin(PC13) ***********************************/
    /* Enable the GPIOCperipheral */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);

    /* Configure C13 pin(PC13) in output function */
    GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
    GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
}
