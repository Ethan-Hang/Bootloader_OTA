#ifndef __UART_H__
#define __UART_H__

#include <stdio.h>

#include "stm32f4xx.h"

void usart1_init(void);
void uart_sendchar(USART_TypeDef *USARTx, uint8_t data);
uint8_t uart_receivechar(USART_TypeDef *USARTx);


#endif /* __UART_H__ */
