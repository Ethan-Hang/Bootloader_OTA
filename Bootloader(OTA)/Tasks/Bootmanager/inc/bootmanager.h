/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOOT_MANAGER_H
#define __BOOT_MANAGER_H

/* Includes ------------------------------------------------------------------*/
#include <stdbool.h>

#include "flash.h"

#include "aes.h"

/* Exported types ------------------------------------------------------------*/
typedef void (*pFunction)(void);
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define ApplicationAddress AppAddress /* Use Flash defined address */
#define NVIC_VectTab_FLASH ((uint32_t)0x08000000)
#define AppAddress                                                             \
    0x08008000UL /* Application/main flash address (Sector 2)                  \
                  */
#define BackAppAddress 0x08020000UL /* Backup/OTA write address (Sector 5) */

/* Exported functions ------------------------------------------------------- */
void   jump_to_app(void);
int8_t back_to_app(int32_t buf_size);
int8_t aes_decrypt_data(uint8_t *input, uint32_t input_len, uint8_t *output,
                        uint32_t *output_len);
int8_t exA_to_exB_AES(int32_t fl_size);
int8_t exB_to_app(void);
int8_t app_to_exA(uint32_t fl_size);
int8_t exA_to_app(void);
void   OTA_StateManager(void);
void ota_apply_update(int32_t file_size, bool first_boot);


#define EE_OTA_NO_APP_UPDATE     0x00
#define EE_OTA_DOWNLOADING       0x11
#define EE_OTA_DOWNLOAD_FINISHED 0x22
#define EE_OTA_APP_CHECK_START   0x33
#define EE_OTA_APP_CHECK_SUCCESS 0x44
#define EE_INIT_NO_APP           0xFF

#endif /* __BOOT_MANAGER_H */
