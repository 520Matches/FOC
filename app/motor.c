#include "motor.h"

void motor_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_BDTRInitTypeDef TIM_BDTRInitStructure;

    // 1. 使能时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_TIM1, ENABLE);

    // 2. 配置 PA8(TIM1_CH1), PA9(TIM1_CH2), PA10(TIM1_CH3)
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP; // 复用推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 注意：FOC 还需要底管的互补输出（PB13, PB14, PB15），此处以 PA8/9/10 为主，互补通道会自动映射到 PB13/14/15
    // 建议也把 PB13, PB14, PB15 配置为复用推挽输出
    // RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14 | GPIO_Pin_15;
    // GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 3. 配置 TIM1 时基单元 (中心对齐模式)
    TIM_TimeBaseStructure.TIM_Prescaler = 0;                       // 不分频
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_CenterAligned1; // 中心对齐模式1
    TIM_TimeBaseStructure.TIM_Period = FOC_PWM_PERIOD;             // ARR 值
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;               // 重复计数器，FOC 中通常设为0
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    // 4. 配置 PWM 通道 (CH1, CH2, CH3)
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;              // PWM模式1
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;  // 使能主输出
    // TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Enable;// 使能互补输出
    TIM_OCInitStructure.TIM_Pulse = 0;                             // 初始占空比为0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;      // 主通道高电平有效
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;    // 互补通道高电平有效
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;   // 空闲状态主通道低电平
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;// 空闲状态互补通道低电平

    TIM_OC1Init(TIM1, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

    TIM_OC2Init(TIM1, &TIM_OCInitStructure);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);

    TIM_OC3Init(TIM1, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);

    // 5. 配置死区时间与刹车 (刹车功能视硬件需求，此处主要配置死区)
    TIM_BDTRInitStructure.TIM_OSSRState = TIM_OSSRState_Enable;
    TIM_BDTRInitStructure.TIM_OSSIState = TIM_OSSIState_Enable;
    TIM_BDTRInitStructure.TIM_LOCKLevel = TIM_LOCKLevel_OFF;       // 不锁定
    TIM_BDTRInitStructure.TIM_DeadTime = FOC_DEAD_TIME;            // 死区时间
    TIM_BDTRInitStructure.TIM_Break = TIM_Break_Disable;           // 禁用刹车
    TIM_BDTRInitStructure.TIM_BreakPolarity = TIM_BreakPolarity_High;
    TIM_BDTRInitStructure.TIM_AutomaticOutput = TIM_AutomaticOutput_Enable;
    TIM_BDTRConfig(TIM1, &TIM_BDTRInitStructure);

    // 6. 使能预装载寄存器 (ARR, CCR)
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    // 7. 启动 TIM1
    TIM_Cmd(TIM1, ENABLE);

    // 8. 关键步骤：使能主输出 (MOE)，否则互补[118;1:3u通道和主通道都不会有波形输出！
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
}

// 假设 duty 范围 0.0f ~ 1.0f，对应 0% ~ 100% 占空比
void motor_set_duty(float duty_a, float duty_b, float duty_c)
{
    // 限制占空比范围，防止 100% 或 0% 导致的自举电容无法充电等问题
    if(duty_a > 0.95f) duty_a = 0.95f;
    if(duty_a < 0.05f) duty_a = 0.05f;
    if(duty_b > 0.95f) duty_b = 0.95f;
    if(duty_b < 0.05f) duty_b = 0.05f;
    if(duty_c > 0.95f) duty_c = 0.95f;
    if(duty_c < 0.05f) duty_c = 0.05f;
    // 同理限制 duty_b, duty_c...

    // 中心对齐模式下，CCR 值直接对应占空比
    // 公式：CCR = duty * ARR
    TIM1->CCR1 = (uint16_t)(duty_a * FOC_PWM_PERIOD);
    TIM1->CCR2 = (uint16_t)(duty_b * FOC_PWM_PERIOD);
    TIM1->CCR3 = (uint16_t)(duty_c * FOC_PWM_PERIOD);
}
