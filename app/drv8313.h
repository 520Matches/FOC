#ifndef __DRV8313_H
#define __DRV8313_H

#include "stm32f10x.h"

/* DRV8313 三相电机驱动: TIM1 PWM, 中心对齐 25kHz
 *   PA8=IN1(U相), PA9=IN2(V相), PA10=IN3(W相)
 *   EN1/2/3 硬件接 3.3V (常使能, 无浮空)
 *   占空比范围: 0(关断) ~ 1440(全开)   */

#define PWM_ARR   1440        /* TIM1 ARR: f_pwm = 72M/(2*1440)=25kHz */
#define PWM_MAX   PWM_ARR     /* CCR = ARR → 100% 导通                */
#define PWM_OFF   0           /* CCR = 0   → GND                      */

void drv8313_init(void);                     /* 初始化 TIM1 三路 PWM     */
void drv8313_set_phase(uint16_t u,           /* 设 U/V/W 相 CCR 值       */
                       uint16_t v,
                       uint16_t w);
void drv8313_set_duty(float duty);           /* 设三相等占空比 0.0~1.0   */
void drv8313_enable(void);                   /* 使能 PWM 输出 (MOE=1)    */
void drv8313_disable(void);                  /* 关断 PWM 输出 (MOE=0)    */

#endif
