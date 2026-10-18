#include "ina199.h"

/* ---- ADC DMA 缓冲: [0]=U相, [1]=V相 ---- */
#define ADC_BUF_SIZE 2
static volatile uint16_t adc_buf[ADC_BUF_SIZE];

/* ================================================================
 * ina199_init: ADC1 双通道 + DMA 循环采集
 *   PA0=CH0(U相), PA1=CH1(V相), ADC_CLK=12MHz (PCLK2/6)
 * ================================================================ */
void ina199_init(void)
{
    ADC_InitTypeDef adc;
    GPIO_InitTypeDef gpio;
    DMA_InitTypeDef dma;

    /* 1. 时钟: ADC1 + GPIOA + DMA1 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 |
                           RCC_APB2Periph_GPIOA, ENABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* 2. ADC 时钟 ≤14MHz: PCLK2/6 = 12MHz */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 3. PA0/PA1: 模拟输入 */
    gpio.GPIO_Pin  = GPIO_Pin_0 | GPIO_Pin_1;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio);

    /* 4. DMA1_Ch1: 外设(ADC_DR)→内存(adc_buf), 循环, 半字 */
    DMA_DeInit(DMA1_Channel1);
    DMA_StructInit(&dma);
    dma.DMA_PeripheralBaseAddr = (uint32_t)&ADC1->DR;
    dma.DMA_MemoryBaseAddr     = (uint32_t)adc_buf;
    dma.DMA_DIR                = DMA_DIR_PeripheralSRC;
    dma.DMA_BufferSize         = ADC_BUF_SIZE;
    dma.DMA_PeripheralInc      = DMA_PeripheralInc_Disable;
    dma.DMA_MemoryInc          = DMA_MemoryInc_Enable;
    dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma.DMA_MemoryDataSize     = DMA_MemoryDataSize_HalfWord;
    dma.DMA_Mode               = DMA_Mode_Circular;
    dma.DMA_Priority           = DMA_Priority_High;
    dma.DMA_M2M                = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &dma);
    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* 5. ADC1: 扫描+连续, 软件启动 */
    ADC_DeInit(ADC1);
    ADC_StructInit(&adc);
    adc.ADC_Mode               = ADC_Mode_Independent;
    adc.ADC_ScanConvMode       = ENABLE;
    adc.ADC_ContinuousConvMode = ENABLE;
    adc.ADC_ExternalTrigConv   = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign          = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel       = ADC_BUF_SIZE;
    ADC_Init(ADC1, &adc);

    /* 6. 通道顺序 + 采样时间 (55.5周期 @12MHz ≈ 4.6µs) */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_0, 1,
                             ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2,
                             ADC_SampleTime_55Cycles5);

    /* 7. DMA 使能 + 上电 + 校准 */
    ADC_DMACmd(ADC1, ENABLE);
    ADC_Cmd(ADC1, ENABLE);

    /* ADC 稳定延时 (tSTAB ≈ 1µs) */
    for (volatile int i = 0; i < 100; i++) __NOP();

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

    /* 8. 启动连续转换 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
}

/* ================================================================
 * ina199_read_u_raw / ina199_read_v_raw: 读 ADC 原始值
 *   DMA 自动刷新 adc_buf, 读取时关中断防数据撕裂
 * ================================================================ */
uint16_t ina199_read_u_raw(void)
{
    uint16_t val;
    __disable_irq();
    val = adc_buf[0];
    __enable_irq();
    return val;
}

uint16_t ina199_read_v_raw(void)
{
    uint16_t val;
    __disable_irq();
    val = adc_buf[1];
    __enable_irq();
    return val;
}

/* ================================================================
 * ina199_read_uv_raw: 一次读走两相
 *
 * 为什么要有这个函数: 调用方 (电流监视) 要的是"同一时刻的 U 伏和 V",
 * 分两次调用除了多一倍临界区开销, 两个值之间还可能夹进一次 ADC 转换。
 *   DMA 一轮扫描 CH0→CH1 约 11µs, 而两条 LDRH 只要几十 ns,
 *   撞上的概率约 0.5%, 真撞上也只差一个采样点 (0.8mA / 11µs)。
 *
 * 顺便纠正一个常见误解: __disable_irq() 挡不住 DMA —— DMA 不受 PRIMASK
 * 影响, 前后做两次读本来就无法靠关中断做到"快照"。
 * 真正保证单个值不被读撕裂的是: 对齐的 16 位读在 Cortex-M3 上是
 * 一条 LDRH, 本身就是原子的, 内存里也不会出现"半个"值 (DMA 写的是半字)。
 * 这里保留临界区只是顺带避免别的中断上下文插进来读走中间状态。
 * ================================================================ */
void ina199_read_uv_raw(uint16_t *u, uint16_t *v)
{
    __disable_irq();
    *u = adc_buf[0];
    *v = adc_buf[1];
    __enable_irq();
}

/* ================================================================
 * ina199_read_u_amps / ina199_read_v_amps: 电流 (A)
 *   I = (ADC / 4096 * 3.3 - 1.65) / (0.02 * 50)
 *     = ADC * 3.3 / 4096 - 1.65
 *   零电流 ≈ 2048 ADC → 0.0A
 *   满量程 ≈ ±1.65A
 * ================================================================ */
float ina199_read_u_amps(void)
{
    return (float)ina199_read_u_raw() * 3.3f / 4096.0f - 1.65f;
}

float ina199_read_v_amps(void)
{
    return (float)ina199_read_v_raw() * 3.3f / 4096.0f - 1.65f;
}
