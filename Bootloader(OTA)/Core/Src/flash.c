#include "flash.h"

static void flash_unlock(void)
{
    FLASH_Unlock();
    while (FLASH_BUSY == FLASH_GetStatus())
        ;
}

static void flash_lock(void)
{
    FLASH_Lock();
}

FLASH_Status erase_app_sector(uint32_t flash_sector)
{
    flash_unlock();
    FLASH_Status status = FLASH_EraseSector(flash_sector, VoltageRange_3);
    flash_lock();
    return status;
}

void flash_write(uint32_t address, uint32_t data)
{
    flash_unlock();
    FLASH_Status status = FLASH_ProgramWord(address, data);
    if (FLASH_COMPLETE == status)
    {
    }
    else
    {
        // Handle error
    }
    flash_lock();
}
