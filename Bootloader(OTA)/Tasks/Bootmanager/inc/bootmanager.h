/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BOOT_MANAGER_H
#define __BOOT_MANAGER_H

/* Includes ------------------------------------------------------------------*/
#include "flash.h"

/* Exported types ------------------------------------------------------------*/
typedef  void (*pFunction)(void);
/* Exported constants --------------------------------------------------------*/
/* Exported macro ------------------------------------------------------------*/
#define ApplicationAddress          BackAppAddress  /* Use Flash defined address */
#define NVIC_VectTab_FLASH          ((uint32_t)0x08000000)
/* Exported functions ------------------------------------------------------- */
void jump_to_app(void);
#endif /* __BOOT_MANAGER_H */

