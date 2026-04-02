/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "uart.h"
#include "tim.h"

#include "bootmanager.h"

#include "at24cxx_driver.h"
#include "w25qxx_Handler.h"

#include "elog.h"

#include "Debug.h"


/* Private typedef -----------------------------------------------------------*/
typedef void (*pFunction)(void);
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
static __IO uint32_t uwTimingDelay;
RCC_ClocksTypeDef    RCC_Clocks;
uint8_t              tab_1024[1024];
volatile bool        elog_init_flag = false;

/* Keep jump flag across soft reset by placing it in UNINIT memory. */
uint32_t g_jumpinit __attribute__((section("NO_INIT"), zero_init));

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
    if (0x55AA55AA == g_jumpinit)
    {
        g_jumpinit = 0xFFFFFFFF;
        jump_to_app();
    }


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
    SPI1_Init();
    TIM_Config();

    elog_init();
    elog_set_fmt(ELOG_LVL_ASSERT, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_ERROR, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_WARN, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_INFO, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_set_fmt(ELOG_LVL_DEBUG, ELOG_FMT_LVL | ELOG_FMT_TAG);
    elog_start();
    elog_init_flag = true;

    ee_CheckOk();

//    ee_Erase();
//    W25Q64_EraseChip();
//    DEBUG_OUT(i, EEPROM_LOG_TAG,
//              "EEPROM and external flash erased successfully");

    W25Q64_Init();

    uint8_t ee_read_ota_status = 0;
    ee_ReadBytes(&ee_read_ota_status, 0x00, 1);
    uint32_t ee_read_ota_size = 0;
    ee_ReadBytes((uint8_t *)&ee_read_ota_size, 0x05, sizeof(ee_read_ota_size));
    DEBUG_OUT(d, EEPROM_LOG_TAG, "OTA state: %d, App size: %lu bytes",
              ee_read_ota_status, (unsigned long)ee_read_ota_size);


    DEBUG_OUT(i, MAIN_LOG_TAG, "this is bootloader");

    /* Infinite loop */
    while (1)
    {
        OTA_StateManager();
        delay_ms(500);
    }
}


void delay_ms(__IO uint32_t nTime)
{
    uwTimingDelay = nTime;

    while (uwTimingDelay != 0)
        ;
}

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
