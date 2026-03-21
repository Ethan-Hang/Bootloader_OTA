/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOOT_MANAGER_H
#define __BOOT_MANAGER_H

/* Includes ------------------------------------------------------------------*/
#include "flash.h"
#include "aes.h"

/* Exported types ------------------------------------------------------------*/
typedef void (*pFunction)(void);
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define ApplicationAddress AppAddress /* Use Flash defined address */
#define NVIC_VectTab_FLASH ((uint32_t)0x08000000)
#define AppAddress           0x08008000UL /* Application/main flash address (Sector 2) */
#define BackAppAddress       0x08020000UL /* Backup/OTA write address (Sector 5) */

/* Exported functions ------------------------------------------------------- */
void   jump_to_app(void);
int8_t back_to_app(int32_t buf_size);
int8_t aes_decrypt_data(uint8_t *input, uint32_t input_len, uint8_t *output,
                        uint32_t *output_len);

#endif /* __BOOT_MANAGER_H */
