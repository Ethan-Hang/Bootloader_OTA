/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stddef.h>

#include "main.h"
#include "tim.h"
#include "gpio.h"
#include "uart.h"

#include "bootmanager.h"
#include "elog.h"
#include "ymodem.h"
#include "flash.h"

/* Private typedef -----------------------------------------------------------*/
typedef void (*pFunction)(void);
/* Private define ------------------------------------------------------------*/
#define APP_FLASH_ADDR 0x08008000
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static __IO uint32_t uwTimingDelay;
RCC_ClocksTypeDef    RCC_Clocks;
/* Note: g_buf is no longer needed - OTA now writes directly to Flash */
/* Private function prototypes -----------------------------------------------*/


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

    /* SysTick end of count event each 1ms */
    SystemCoreClockUpdate();
    RCC_GetClocksFreq(&RCC_Clocks);
    SysTick_Config(RCC_Clocks.HCLK_Frequency / 1000);


    /* Add your application code here */
    /* Insert 50 ms delay */
    delay_ms(50);

    GPIO_Config();
    usart1_init();

    elog_init();
    elog_start();

    int32_t buf_size = 0;
    buf_size = Ymodem_Receive(NULL);  /* buf parameter is no longer used, data goes directly to Flash */
    log_i("Ymodem receive complete, file size: %d bytes", buf_size);

    /* Check if OTA was successful (positive size) */
    if (buf_size > 0)
    {
        log_i("OTA success! Jumping to application at 0x%08x", BackAppAddress);
        delay_ms(100);  /* Brief delay before jump */
        jump_to_app();
        /* Never return here if jump is successful */
    }
    else
    {
        log_e("OTA failed or no data received (size: %d)", buf_size);
        log_i("Remaining in bootloader...");
    }
    // TIM_Config();
    /* Infinite loop */
    while (1)
    {
        log_i("This is a info log.");
        delay_ms(500);   
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
