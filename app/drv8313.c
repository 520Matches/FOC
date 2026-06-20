#include "drv8313.h"

/* ================================================================
 * drv8313_init: TIM1 三路 PWM, PA8/PA9/PA10, 25kHz 中心对齐
 *   初始占空比 = 0 (安全关断), 需调用 drv8313_enable() 开启输出
 * ================================================================ */
void drv8313_init(void)
{
    GPIO_InitTypeDef gpio;
    TIM_TimeBaseInitTypeDef tim;
    TIM_OCInitTypeDef oc;
    TIM_BDTRInitTypeDef bdtr;

    /* 开启外设时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1 |
                           RCC_APB2Periph_GPIOA, ENABLE);

    /* PA8/9/10 → 推挽复用 → DRV8313 IN1/IN2/IN3 */
    gpio.GPIO_Pin   = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    /* 时基: 中心对齐模式1 (CMS=01, DIR=00) */
    TIM_TimeBaseStructInit(&tim);
    tim.TIM_Period            = PWM_ARR;
    tim.TIM_Prescaler         = 0;
    tim.TIM_ClockDivision     = TIM_CKD_DIV1;
    tim.TIM_CounterMode       = TIM_CounterMode_CenterAligned1;
    tim.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &tim);

    /* OC1/OC2/OC3: PWM1 模式, 初始 pulse=0 */
    TIM_OCStructInit(&oc);
    oc.TIM_OCMode        = TIM_OCMode_PWM1;
    oc.TIM_OutputState   = TIM_OutputState_Enable;
    oc.TIM_OutputNState  = TIM_OutputNState_Disable;
    oc.TIM_Pulse         = 0;
    oc.TIM_OCPolarity    = TIM_OCPolarity_High;
    oc.TIM_OCNPolarity   = TIM_OCNPolarity_High;
    oc.TIM_OCIdleState   = TIM_OCIdleState_Reset;
    oc.TIM_OCNIdleState  = TIM_OCNIdleState_Reset;

    TIM_OC1Init(TIM1, &oc);
    TIM_OC2Init(TIM1, &oc);
    TIM_OC3Init(TIM1, &oc);

    /* 预装载使能: CCR 写入后在下个更新事件生效 */
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM1, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM1, ENABLE);

    /* BDTR: 高级定时器必需 (死区/刹车) */
    TIM_BDTRStructInit(&bdtr);
    TIM_BDTRConfig(TIM1, &bdtr);

    /* 启动计数器, 但不使能输出 (MOE 由 enable/disable 控制) */
    TIM_Cmd(TIM1, ENABLE);
}

/* ================================================================
 * drv8313_set_phase: 设置三相 CCR 值 (0 ~ PWM_ARR)
 * ================================================================ */
void drv8313_set_phase(uint16_t u, uint16_t v, uint16_t w)
{
    TIM_SetCompare1(TIM1, u);
    TIM_SetCompare2(TIM1, v);
    TIM_SetCompare3(TIM1, w);
}

/* ================================================================
 * drv8313_set_duty: 三相等占空比 (0.0 ~ 1.0)
 * ================================================================ */
void drv8313_set_duty(float duty)
{
    uint16_t pulse = (uint16_t)((float)PWM_ARR * duty);
    if (pulse > PWM_ARR) pulse = PWM_ARR;
    drv8313_set_phase(pulse, pulse, pulse);
}

/* ================================================================
 * drv8313_enable / drv8313_disable: MOE (Main Output Enable)
 * ================================================================ */
void drv8313_enable(void)
{
    TIM_CtrlPWMOutputs(TIM1, ENABLE);   /* BDTR bit15 MOE=1 */
}

void drv8313_disable(void)
{
    TIM_CtrlPWMOutputs(TIM1, DISABLE);  /* MOE=0, 输出全关 */
}
