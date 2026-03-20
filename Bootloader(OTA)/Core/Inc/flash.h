#ifndef __FLASH_H__
#define __FLASH_H__

#include "stm32f4xx.h"
#include "main.h"

FLASH_Status erase_app_sector(uint32_t flash_sector);
void         flash_write(uint32_t address, uint32_t data);

#endif /* __FLASH_H__ */
