#ifndef __UART_H
#define __UART_H

#include "stm32f10x.h"

/* ================================================================
 * USART2: PA2=TX, PA3=RX, 8-N-1
 *
 * 职责划分 (重要):
 *   - 串口 = 波形数据专用通道 (DMA, JustFloat 二进制)
 *   - printf / 日志 = 走 RTT (_write 已重定向到 RTT)
 *   两者混用会互相打断, 波形里会莫名其妙出现 ASCII 字符。
 *
 * 波特率上限: USART2 挂 APB1, 波特率 = PCLK1 / (16 * BRR)。
 *   PCLK1 = 36MHz → BRR 最小 1 → 理论上限 2.25Mbps。
 *   常用: 921600 (最稳), 2000000 (BRR=0x12, 零误差, 需 USB-TTL 支持)
 * ================================================================ */

#define UART_BAUD_115200   115200UL
#define UART_BAUD_460800   460800UL
#define UART_BAUD_921600   921600UL
#define UART_BAUD_2000000  2000000UL

void     uart_init(uint32_t baud);    /* 初始化 USART2 + TX DMA (DMA1_Ch7) */
void     uart_putc(char c);           /* 阻塞发单字符 (仅用于裸调试)      */
void     uart_puts(const char *s);    /* 阻塞发字符串                      */

int      uart_dma_send(const void *buf, uint16_t len);  /* 非阻塞 DMA 发送 */
uint8_t  uart_tx_busy(void);                            /* 1=正在发送     */
void     uart_wait_idle(void);                          /* 阻塞等待发完   */
int      uart_poll_rx(char *out, int max);              /* 轮询 RX, 非 DMA, 用于回环自检 */

#endif
