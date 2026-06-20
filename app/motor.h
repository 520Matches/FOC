#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

/* 三轮换相电机控制 (DRV8313 + MT6701 + INA199)
 *   三步开环/闭环换相, 支持加速斜坡和角度反馈  */

/* ---- 返回状态 ---- */
typedef enum {
    MOTOR_OK = 0,
    MOTOR_ERR_ANGLE,        /* 编码器读角失败            */
    MOTOR_ERR_OVERCURRENT,  /* 过流保护触发              */
} motor_status_t;

/* ---- 初始化 + 控制 ---- */
void motor_init(void);                    /* 初始化所有外设 + SysTick  */
void motor_start(void);                   /* 开始换相 (开环, 自动加速) */
void motor_stop(void);                    /* 停止换相, 关闭 PWM 输出   */
void motor_set_duty(uint16_t duty);       /* 设置占空比 pulse (0~1440) */
uint16_t motor_get_duty(void);            /* 读取当前占空比            */

/* ---- 传感器读取 ---- */
float motor_read_angle(void);             /* 编码器角度 0~360°         */
float motor_read_current_u(void);         /* U 相电流 (A)              */
float motor_read_current_v(void);         /* V 相电流 (A)              */

#endif
