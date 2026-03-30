#include "iic.h"


/* GPIO结构体初始化 */
static void i2c_cfgGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    RCC_AHB1PeriphClockCmd(EEPROM_I2C_GPIO_CLK, ENABLE);
    /*Configure GPIO pin : PtPin */
    GPIO_InitStruct.GPIO_Pin   = EEPROM_I2C_SCL_PIN | EEPROM_I2C_SDA_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_OUT;
    GPIO_InitStruct.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_100MHz;
    GPIO_InitStruct.GPIO_OType = GPIO_OType_OD;
    GPIO_Init(EEPROM_I2C_GPIO_PORT, &GPIO_InitStruct);

    /* 给一个停止信号, 复位I2C总线上的所有设备到待机模式 */
    i2c_Stop();
}

/*写一个IIC总线位延时延时函数,根据下图AT24C02脉冲宽度的时间,工作环境168M,IIC速度最快400khz*/
static void i2c_Delay(void)
{
    uint8_t i;
    for (i = 0; i < 40; i++)
        ;
}

/*CPU发起IIC总线的起始信号*/
void i2c_Start(void)
{
    /* 当SCL高电平时，SDA出现一个下跳沿表示I2C总线启动信号 */
    EEPROM_I2C_SCL_1();
    EEPROM_I2C_SDA_1();
    i2c_Delay();
    EEPROM_I2C_SDA_0();
    i2c_Delay();
    EEPROM_I2C_SCL_0();
    i2c_Delay();
}

/*CPU发起IIC总线的停止信号*/
void i2c_Stop(void)
{
    /* 当SCL高电平时，SDA出现一个上跳沿表示I2C总线停止信号 */
    EEPROM_I2C_SDA_0();
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    EEPROM_I2C_SDA_1();
    i2c_Delay();
}

/*CPU产生一个ACK信号*/
void i2c_Ack(void)
{
    EEPROM_I2C_SDA_0();
    i2c_Delay();
    EEPROM_I2C_SCL_1();
    i2c_Delay();

    EEPROM_I2C_SCL_0();
    i2c_Delay();

    EEPROM_I2C_SDA_1(); /* CPU释放SDA总线 */
    i2c_Delay();
}

/*CPU产生1个NACK信号*/
void i2c_NAck(void)
{
    EEPROM_I2C_SDA_1();
    i2c_Delay();
    EEPROM_I2C_SCL_1();
    i2c_Delay();
    EEPROM_I2C_SCL_0();
    i2c_Delay();
}

/*
*********************************************************************************************************
* 函 数 名: i2c_WaitAck
* 功能说明: CPU产生一个时钟，并读取器件的ACK应答信号
* 形    参：无
* 返 回 值: 返回0表示正确应答，1表示无器件响应
*********************************************************************************************************
*/
uint8_t i2c_WaitAck(void)
{
    uint8_t re;
    /* CPU释放SDA总线 */
    EEPROM_I2C_SDA_1();
    i2c_Delay();

    /* CPU驱动SCL = 1, 此时器件会返回ACK应答 */
    EEPROM_I2C_SCL_1();
    i2c_Delay();

    /* CPU读取SDA口线状态 */
    if (EEPROM_I2C_SDA_READ())
    {
        re = 1;
    }
    else
    {
        re = 0;
    }
    EEPROM_I2C_SCL_0();
    i2c_Delay();

    return re;
}

/*
*********************************************************************************************************
* 函 数 名: i2c_SendByte
* 功能说明: CPU向I2C总线设备发送8bit数据
* 形    参：_ucByte ： 等待发送的字节
* 返 回 值: 无
*********************************************************************************************************
*/
void i2c_SendByte(uint8_t _ucByte)
{
    uint8_t i;
    /* 先发送字节的高位bit7 */
    for (i = 0; i < 8; i++)
    {
        if (_ucByte & 0x80)
        {
            EEPROM_I2C_SDA_1();
        }
        else
        {
            EEPROM_I2C_SDA_0();
        }
        i2c_Delay();

        EEPROM_I2C_SCL_1();
        i2c_Delay();
        EEPROM_I2C_SCL_0();
        i2c_Delay();

        if (i == 7)
        {
            EEPROM_I2C_SDA_1();
        }
        _ucByte <<= 1;
        i2c_Delay();
    }
}

/*
*********************************************************************************************************
* 函 数 名: i2c_ReadByte
* 功能说明: CPU从I2C总线设备读取8bit数据
* 形    参：无
* 返 回 值: 读到的数据
*********************************************************************************************************
*/
uint8_t i2c_ReadByte(void)
{
    uint8_t i;
    uint8_t value;

    /* 读到第1个bit为数据的bit7 */
    value = 0;
    for (i = 0; i < 8; i++)
    {
        value <<= 1;
        EEPROM_I2C_SCL_1();
        i2c_Delay();
        if (EEPROM_I2C_SDA_READ())
        {
            value++;
        }
        EEPROM_I2C_SCL_0();
        i2c_Delay();
    }
    return value;
}

/*
*********************************************************************************************************
* 函 数 名: i2c_CheckDevice
* 功能说明:
* 检测I2C总线设备，CPU向发送设备地址，然后读取设备应答来判断该设备是否存在 形
* 参：_Address：设备的I2C总线地址 返 回 值: 返回值 0 表示正确，
* 返回1表示未探测到
*********************************************************************************************************
*/
uint8_t i2c_CheckDevice(uint8_t _Address)
{
    uint8_t ucAck;
    i2c_cfgGpio(); /* 配置GPIO */
    i2c_Start();   /* 发送启动信号 */
                   /* 发送设备地址+读写控制bit（0 = w， 1 = r) bit7 先传 */
    i2c_SendByte(_Address | EEPROM_I2C_WR);
    ucAck = i2c_WaitAck(); /* 检测设备的ACK应答 */
    i2c_Stop();            /* 发送停止信号 */
    return ucAck;
}
