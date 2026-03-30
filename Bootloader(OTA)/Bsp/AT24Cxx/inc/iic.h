#ifndef _IIC_H
#define _IIC_H

#include "stm32f4xx.h"
/*EEPROM写方向*/
#define EEPROM_I2C_WR        0
/*EEPROM读方向*/
#define EEPROM_I2C_RD        1

#define EEPROM_I2C_GPIO_PORT GPIOB
#define EEPROM_I2C_GPIO_CLK  RCC_AHB1Periph_GPIOB
#define EEPROM_I2C_SCL_PIN   GPIO_Pin_8
#define EEPROM_I2C_SDA_PIN   GPIO_Pin_9

#define EEPROM_I2C_SCL_1()                                                     \
    GPIO_SetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SCL_PIN);
#define EEPROM_I2C_SCL_0()                                                     \
    GPIO_ResetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SCL_PIN);

#define EEPROM_I2C_SDA_1()                                                     \
    GPIO_SetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN);
#define EEPROM_I2C_SDA_0()                                                     \
    GPIO_ResetBits(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN);

/*读取SDA引脚的电平状态*/
#define EEPROM_I2C_SDA_READ()                                                  \
    GPIO_ReadInputDataBit(EEPROM_I2C_GPIO_PORT, EEPROM_I2C_SDA_PIN)

void    i2c_Start(void);
void    i2c_Stop(void);
void    i2c_SendByte(uint8_t _ucByte);
uint8_t i2c_ReadByte(void);
uint8_t i2c_WaitAck(void);
void    i2c_Ack(void);
void    i2c_NAck(void);
uint8_t i2c_CheckDevice(uint8_t _Address);

#endif
