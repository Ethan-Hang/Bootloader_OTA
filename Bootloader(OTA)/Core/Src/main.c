/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "gpio.h"
#include <stddef.h>
// 全局定义 STM32F411xE 或者 STM32F401xx
// 当前定义 STM32F411xE

// STM32F411 外部晶振25Mhz，考虑到USB使用，内部频率设置为96Mhz
// 需要100mhz,自行修改system_stm32f4xx.c

/** @addtogroup Template_Project
 * @{
 */

/* Private typedef -----------------------------------------------------------*/
typedef void (*pFunction)(void);
/* Private define ------------------------------------------------------------*/
#define APP_FLASH_ADDR 0x08008000
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static __IO uint32_t uwTimingDelay;
RCC_ClocksTypeDef    RCC_Clocks;

/* Private function prototypes -----------------------------------------------*/
void disable_all_peripherals(void)
{
    __disable_irq();
    /* Stop SysTick to avoid bootloader tick interrupt after jump. */
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;

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
    __IO uint32_t sp            = *(__IO uint32_t *)(APP_FLASH_ADDR);
    uint32_t      jump_addr     = 0x00;
    pFunction     jump_app_func = NULL;

    if (0x20000000 < sp && sp < 0x20020000)
    {
        disable_all_peripherals();

        // Reset Handler
        jump_addr = *(__IO uint32_t *)(APP_FLASH_ADDR + 4);

        __set_MSP(sp);

        jump_app_func = (pFunction)jump_addr;
        jump_app_func();
    }
}

uint8_t key_scan(void)
{
    if (Bit_RESET == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0))
    {
        delay_ms(10);
        if (Bit_RESET == GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0))
        {
            return 1;
        }
    }
    return 0;
}

/* Private functions ---------------------------------------------------------*/
/*
 *power by WeAct Studio
 *The board with `WeAct` Logo && `version number` is our board, quality
 * guarantee. For more information please visit:
 * https://github.com/WeActTC/MiniF4-STM32F4x1
 *更多信息请访问：https://gitee.com/WeActTC/MiniF4-STM32F4x1
 */
/**
 * @brief  Main program
 * @param  None
 * @retval None
 */
int main(void)
{
    SCB->VTOR = 0x08000000 | 0x00000000;
    /* Enable Clock Security System(CSS): this will generate an NMI exception
     when HSE clock fails *****************************************************/
    RCC_ClockSecuritySystemCmd(ENABLE);

    /*!< At this stage the microcontroller clock setting is already configured,
          this is done through SystemInit() function which is called from
       startup files before to branch to application main. To reconfigure the
       default setting of SystemInit() function, refer to system_stm32f4xx.c
       file */

    /* SysTick end of count event each 1ms */
    SystemCoreClockUpdate();
    RCC_GetClocksFreq(&RCC_Clocks);
    SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);


    /* Add your application code here */
    /* Insert 50 ms delay */
    delay_ms(50);

    GPIO_Config();

    // jump_to_app();
    // TIM_Config();
    /* Infinite loop */
    while (1)
    {
        
    }
}

/**
 * @brief  Inserts a delay time.
 * @param  nTime: specifies the delay time length, in milliseconds.
 * @retval None
 */
void delay_ms(__IO uint32_t nTime)
{
    uwTimingDelay = nTime;

    while (uwTimingDelay != 0)
        ;
}

/**
 * @brief  Decrements the TimingDelay variable.
 * @param  None
 * @retval None
 */
void TimingDelay_Decrement(void)
{
    if (uwTimingDelay != 0x00)
    {
        uwTimingDelay--;
    }
}

#ifdef USE_FULL_ASSERT

/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
       file, line) */

    /* Infinite loop */
    while (1)
    {
    }
}
#endif

/**
 * @}
 */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
