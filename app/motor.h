#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

// PWM 频率与周期配置
#define FOC_PWM_FREQ        16000       // 16kHz FOC 载波频率
#define FOC_PWM_PERIOD      2250        // ARR 值 (72MHz / 2 / 16kHz)
#define FOC_DEAD_TIME       0xAC        // 死区时间约 3us (需根据实际硬件微调)


void motor_init(void);
void motor_set_duty(float duty_a, float duty_b, float duty_c);

#endif
