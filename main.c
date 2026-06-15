#include "stm32f10x.h"

#define PWM_PERIOD  1000   // ARR
#define PWM_PULSE   500    // 50% 占空比（示例）

/* DMA 缓冲区 */
#define ADC_CH_NUM  2
uint16_t adc_buf[ADC_CH_NUM];

void pwm_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCStructure;

    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_TIM1 |
        RCC_APB2Periph_GPIOA,
        ENABLE
    );

    /* PA8 / PA9 / PA10 : AF_PP */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 时基 */
    TIM_TimeBaseStructure.TIM_Period = PWM_PERIOD;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode =
        TIM_CounterMode_CenterAligned1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /* PWM 通道 */
    TIM_OCStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCStructure.TIM_Pulse = PWM_PERIOD / 2;
    TIM_OCStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;

    TIM_OC1Init(TIM1, &TIM_OCStructure);
    TIM_OC2Init(TIM1, &TIM_OCStructure);
    TIM_OC3Init(TIM1, &TIM_OCStructure);

    /* CC1 用作 ADC 触发 */
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

    TIM_Cmd(TIM1, ENABLE);
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

void adc_init(void)
{
	ADC_InitTypeDef ADC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;
    DMA_InitTypeDef DMA_InitStructure;

    /* 1. 开时钟 */
    RCC_APB2PeriphClockCmd(
        RCC_APB2Periph_ADC1 |
        RCC_APB2Periph_GPIOA,
        ENABLE
    );
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    /* 2. PA0 / PA1：模拟输入 */
    GPIO_InitStructure.GPIO_Pin =
        GPIO_Pin_0 | GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 3. DMA 配置 */
    DMA_DeInit(DMA1_Channel1);

    DMA_InitStructure.DMA_PeripheralBaseAddr =
        (uint32_t)&ADC1->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr =
        (uint32_t)adc_buf;
    DMA_InitStructure.DMA_DIR =
        DMA_DIR_PeripheralSRC;
    DMA_InitStructure.DMA_BufferSize =
        ADC_CH_NUM;
    DMA_InitStructure.DMA_PeripheralInc =
        DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc =
        DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize =
        DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize =
        DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode =
        DMA_Mode_Circular;      // ✅ 循环模式
    DMA_InitStructure.DMA_Priority =
        DMA_Priority_High;
    DMA_InitStructure.DMA_M2M =
        DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &DMA_InitStructure);

    DMA_Cmd(DMA1_Channel1, ENABLE);

    /* 4. ADC 配置 */
    ADC_DeInit(ADC1);

    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = ENABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv =
        ADC_ExternalTrigConv_Ext_IT11_TIM8_TRGO;   // ✅ TIM1 Update
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = ADC_CH_NUM;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 5. 通道顺序 */
    ADC_RegularChannelConfig(
        ADC1, ADC_Channel_0, 1, ADC_SampleTime_28Cycles5
    );
    ADC_RegularChannelConfig(
        ADC1, ADC_Channel_1, 2, ADC_SampleTime_28Cycles5
    );

    /* 6. ADC → DMA */
    ADC_DMACmd(ADC1, ENABLE);

    /* 7. 外部触发使能 */
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);

    /* 8. 启动 ADC */
    ADC_Cmd(ADC1, ENABLE);

    /* 9. 校准 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));

    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));

	ADC_ExternalTrigInjectedConvConfig(ADC1, ADC_ExternalTrigInjecConv_T1_TRGO);
	/* Enable ADC1 injected external trigger conversion */
	ADC_ExternalTrigInjectedConvCmd(ADC1, ENABLE);
}

int main(void)
{
	pwm_init();
	adc_init();
	while(1);
	return 0;
}
