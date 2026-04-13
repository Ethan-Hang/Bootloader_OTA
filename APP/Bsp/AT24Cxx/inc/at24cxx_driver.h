#ifndef __ATC24CXX_DRIVER_H
#define __ATC24CXX_DRIVER_H

#include <stdint.h>

/*
 * AT24C02 2kb = 2048bit = 2048/8 B = 256 B
 * 32 pages of 8 bytes each
 *
 * Device Address
 * 1 0 1 0 A2 A1 A0 R/W
 * 1 0 1 0 0  0  0  0 = 0XA0
 * 1 0 1 0 0  0  0  1 = 0XA1
 */
/* AT24C01/02每页有8个字节
 * AT24C04/08A/16A每页有16个字节
 */
#define EEPROM_DEV_ADDR  0xA0 /* 24xx02的设备地址 */
#define EEPROM_PAGE_SIZE 8    /* 24xx02的页面大小 */
#define EEPROM_SIZE      256  /* 24xx02总容量 */

/* Debug output uses DEBUG_OUT(...) in Debug.h */

/**
 * @brief
 * Probe EEPROM device presence on I2C bus.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 1 when EEPROM responds, 0 otherwise.
 * */
uint8_t ee_CheckOk(void);

/**
 * @brief
 * Read bytes from EEPROM starting at given address.
 *
 * @param[in]  _usAddress : EEPROM start address.
 *
 * @param[in]  _usSize    : Byte count to read.
 *
 * @param[out] _pReadBuf  : Destination buffer.
 *
 * @return
 * 1 on success, 0 on failure.
 * */
uint8_t ee_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize);

/**
 * @brief
 * Write bytes to EEPROM from given address.
 *
 * @param[in] _pWriteBuf  : Source buffer.
 *
 * @param[in] _usAddress  : EEPROM start address.
 *
 * @param[in] _usSize     : Byte count to write.
 *
 * @param[out] : None.
 *
 * @return
 * 1 on success, 0 on failure.
 * */
uint8_t ee_WriteBytes(uint8_t *_pWriteBuf, uint16_t _usAddress,
                      uint16_t _usSize);

/**
 * @brief
 * Erase EEPROM content to 0xFF.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * None.
 * */
void    ee_Erase(void);

/**
 * @brief
 * Run EEPROM read/write self-test.
 *
 * @param[in] : None.
 *
 * @param[out] : None.
 *
 * @return
 * 1 when test passes, 0 on failure.
 * */
uint8_t ee_Test(void);

#endif /* __I2C_EE_H */
