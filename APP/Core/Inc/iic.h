#ifndef _IIC_H
#define _IIC_H

#include "main.h"

/*EEPROM写方向*/
#define EEPROM_I2C_WR        0
/*EEPROM读方向*/
#define EEPROM_I2C_RD        1

#define EEPROM_I2C_GPIO_PORT GPIOB
#define EEPROM_I2C_SCL_PIN   EE_SCL_Pin
#define EEPROM_I2C_SDA_PIN   EE_SDA_Pin

#define EEPROM_I2C_SCL_1()                                                     \
    HAL_GPIO_WritePin(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SCL_PIN, GPIO_PIN_SET);
#define EEPROM_I2C_SCL_0()                                                     \
    HAL_GPIO_WritePin(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SCL_PIN, GPIO_PIN_RESET);

#define EEPROM_I2C_SDA_1()                                                     \
    HAL_GPIO_WritePin(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN, GPIO_PIN_SET);
#define EEPROM_I2C_SDA_0()                                                     \
    HAL_GPIO_WritePin(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN, GPIO_PIN_RESET);

/*读取SDA引脚的电平状态*/
#define EEPROM_I2C_SDA_READ()                                                  \
    HAL_GPIO_ReadPin(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN)

void    i2c_Start(void);
void    i2c_Stop(void);
void    i2c_SendByte(uint8_t _ucByte);
uint8_t i2c_ReadByte(void);
uint8_t i2c_WaitAck(void);
void    i2c_Ack(void);
void    i2c_NAck(void);
uint8_t i2c_CheckDevice(uint8_t _Address);

#endif
