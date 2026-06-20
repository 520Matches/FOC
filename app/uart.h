#ifndef __UART_H
#define __UART_H

#include "stm32f10x.h"

/* USART2: PA2=TX, PA3=RX, 115200-8-N-1
 * 接 ST-Link V2 的 TX/RX/GND 或 USB-TTL 模块      */

void uart_init(void);                  /* 初始化 USART2, 重定向 printf */
void uart_putc(char c);               /* 单字符发送                   */
void uart_puts(const char *s);        /* 字符串发送                   */

#endif
