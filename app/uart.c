#include "uart.h"
#include <stdio.h>

/* ================================================================
 * uart_init: USART2 @ 115200, PA2(TX)=AF_PP, PA3(RX)=浮空输入
 *   USART2 挂 APB1=36MHz, 波特率 = 36M/(16*19.5) ≈ 115384bps
 * ================================================================ */
void uart_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;

    /* 开启时钟: USART2(APB1) + GPIOA(APB2) + AFIO(复用) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* PA2 (TX): 复用推挽输出 */
    gpio.GPIO_Pin   = GPIO_Pin_2;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA3 (RX): 浮空输入 */
    gpio.GPIO_Pin   = GPIO_Pin_3;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* USART2: 115200-8-N-1 */
    USART_StructInit(&usart);
    usart.USART_BaudRate            = 115200;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &usart);

    USART_Cmd(USART2, ENABLE);
}

/* ================================================================
 * uart_putc: 阻塞发送单个字符
 * ================================================================ */
void uart_putc(char c)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, (uint16_t)c);
}

/* ================================================================
 * uart_puts: 发送以 '\0' 结尾的字符串
 * ================================================================ */
void uart_puts(const char *s)
{
    while (*s) {
        uart_putc(*s++);
    }
}

/* ================================================================
 * _write: newlib-nano printf 重定向到 USART2
 *   printf / fprintf(stdout, ...) 自动走这里
 * ================================================================ */
int _write(int fd, const char *buf, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        uart_putc(buf[i]);
    }
    return len;
}
