#include "uart.h"
#include "SEGGER_RTT.h"
#include <stdio.h>

/* DMA1_Channel7 = USART2_TX */
static volatile uint8_t s_tx_busy = 0;

/* ================================================================
 * uart_init: USART2 + TX DMA
 *   PA2(TX)=复用推挽, PA3(RX)=浮空输入
 *   APB1 = PCLK1 = 36MHz (72MHz 时)
 * ================================================================ */
void uart_init(uint32_t baud)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef  nvic;

    /* 时钟: GPIOA + AFIO(APB2), USART2(APB1), DMA1(AHB) */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* PA2 (TX): 复用推挽 */
    gpio.GPIO_Pin   = GPIO_Pin_2;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* PA3 (RX): 浮空输入 */
    gpio.GPIO_Pin   = GPIO_Pin_3;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    /* USART2: 8-N-1, 只发不收也保持 RX 使能, 方便后面做命令通道 */
    USART_StructInit(&usart);
    usart.USART_BaudRate            = baud;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART2, &usart);

    /* 打开发送 DMA 请求 */
    USART_DMACmd(USART2, USART_DMAReq_Tx, ENABLE);
    USART_Cmd(USART2, ENABLE);

    /* DMA1_Ch7 传输完成中断: 只用来清 busy 标志 */
    nvic.NVIC_IRQChannel                   = DMA1_Channel7_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 3;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);
}

/* ================================================================
 * uart_dma_send: 非阻塞发送
 *   返回 0=已启动, -1=上一次还没发完(调用方应丢弃或重试)
 * ================================================================ */
int uart_dma_send(const void *buf, uint16_t len)
{
    DMA_InitTypeDef dma;

    if (len == 0) return 0;
    if (s_tx_busy) return -1;

    s_tx_busy = 1;

    DMA_DeInit(DMA1_Channel7);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&USART2->DR;
    dma.DMA_MemoryBaseAddr     = (uint32_t)buf;
    dma.DMA_DIR                = DMA_DIR_PeripheralDST;
    dma.DMA_BufferSize         = len;
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_Byte;
    dma.DMA_Mode               = DMA_Mode_Normal;
    dma.DMA_Priority           = DMA_Priority_High;
    dma.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel7, &dma);

    DMA_ITConfig(DMA1_Channel7, DMA_IT_TC, ENABLE);
    DMA_Cmd(DMA1_Channel7, ENABLE);
    return 0;
}

uint8_t uart_tx_busy(void) { return s_tx_busy; }

void uart_wait_idle(void)
{
    while (s_tx_busy) { }
    /* 等移位寄存器也发完, 避免最后一个字节被下一次发送覆盖 */
    while (USART_GetFlagStatus(USART2, USART_FLAG_TC) == RESET) { }
}

/* ================================================================
 * 阻塞发送: 仅用于没有起 DMA 时的裸调试
 * ================================================================ */
void uart_putc(char c)
{
    while (USART_GetFlagStatus(USART2, USART_FLAG_TXE) == RESET);
    USART_SendData(USART2, (uint16_t)c);
}

void uart_puts(const char *s)
{
    while (*s) uart_putc(*s++);
}

/* ================================================================
 * uart_poll_rx: 非中断轮询 RX, 用于 'l' 回环自检
 *   不开 RXNE 中断, 避免和 scope 的 DMA 抢 NVIC; 主循环轮询就行
 * ================================================================ */
int uart_poll_rx(char *out, int max)
{
    int n = 0;
    while (n < max && USART_GetFlagStatus(USART2, USART_FLAG_RXNE) == SET) {
        out[n++] = (char)USART_ReceiveData(USART2);
    }
    return n;
}

/* ================================================================
 * _write: printf 重定向到 RTT, 不再占用串口
 *
 *   串口是波形专用通道, printf 的 ASCII 混进去会把 JustFloat 帧搞乱。
 *   日志一律走 RTT (见 log.h), 两者互不干扰。
 * ================================================================ */
int _write(int fd, const char *buf, int len)
{
    (void)fd;
    if (len > 0) {
        SEGGER_RTT_Write(0, buf, (unsigned)len);
    }
    return len;
}

/* ================================================================
 * DMA1_Channel7_IRQHandler: 发送完成
 * ================================================================ */
void DMA1_Channel7_IRQHandler(void)
{
    if (DMA_GetITStatus(DMA1_IT_TC7)) {
        DMA_ClearITPendingBit(DMA1_IT_TC7);
        DMA_Cmd(DMA1_Channel7, DISABLE);
        s_tx_busy = 0;
    }
}
