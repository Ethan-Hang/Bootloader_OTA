#ifndef __UART_H__
#define __UART_H__

#include "stm32f4xx.h"
#include "main.h"

void usart1_init(void);
void uart_sendchar(USART_TypeDef *USARTx, uint8_t data);
void uart_receivechar(USART_TypeDef *USARTx, uint8_t *data);

#endif /* __UART_H__ */
