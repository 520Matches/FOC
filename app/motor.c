#include "motor.h"
#include "drv8313.h"
#include "mt6701.h"
#include "ina199.h"

/* ---- 三步换相模式: 每步仅一相 PWM, 另两相 GND ---- */
static const uint8_t comm_pattern[3][3] = {
    {1, 0, 0},  /* 步0: U→V+W  磁场 0°   */
    {0, 1, 0},  /* 步1: V→U+W  磁场 120° */
    {0, 0, 1},  /* 步2: W→U+V  磁场 240° */
};

/* ---- 加速斜坡参数 ---- */
#define STEP_MS_START  100   /* 起步换相间隔 ms */
#define STEP_MS_END     25   /* 目标换相间隔 ms */
#define STEP_MS_DEC      3   /* 每档递减 ms     */
#define RAMP_REVS         5   /* 每 N 电周期加速 */

/* ---- 保护 ---- */
#define OC_THRESHOLD_ADC  2800  /* 过流阈值 (≈1.2A @ ADC) */

/* ---- 全局状态 ---- */
static volatile uint32_t comm_tick   = 0;             /* SysTick 累加器       */
static volatile uint32_t step_ms     = STEP_MS_START; /* 换相间隔 ms          */
static volatile uint8_t  comm_step   = 0;             /* 当前步 0~2           */
static volatile uint8_t  rev_count   = 0;             /* 电周期计数           */
static volatile uint16_t motor_duty  = 80;            /* 当前占空比           */
static volatile uint8_t  motor_on    = 0;             /* 0=停转, 1=运行      */

/* ---- 内部函数 ---- */
static void motor_commutate(uint8_t step);
static void systick_init(void);

/* ================================================================
 * motor_init: 一次性初始化全部外设
 *   drv8313(TIM1 PWM) + mt6701(SSI) + ina199(ADC+DMA) + SysTick
 * ================================================================ */
void motor_init(void)
{
    drv8313_init();       /* TIM1 PWM 配置 (输出关闭) */
    mt6701_init();        /* 编码器 SSI GPIO          */
    ina199_init();        /* ADC + DMA 电流采样       */
    systick_init();       /* 1ms 中断定时器           */
}

/* ================================================================
 * motor_start: 使能 PWM 输出, 从步0开始换相, 自动加速
 * ================================================================ */
void motor_start(void)
{
    comm_tick  = 0;
    step_ms    = STEP_MS_START;
    comm_step  = 0;
    rev_count  = 0;
    motor_on   = 1;

    drv8313_enable();           /* MOE=1, PWM 输出使能 */
    motor_commutate(0);         /* 首次换相 (步0)      */
}

/* ================================================================
 * motor_stop: 关 PWM 输出, 停止换相
 * ================================================================ */
void motor_stop(void)
{
    motor_on = 0;
    drv8313_disable();          /* MOE=0, 全部关断 */
    drv8313_set_phase(0, 0, 0);
}

/* ================================================================
 * motor_commutate: 按 comm_pattern 设置三相 CCR
 *   导通相 = motor_duty, 关断相 = 0 (GND)
 * ================================================================ */
static void motor_commutate(uint8_t step)
{
    uint16_t d = motor_duty;  /* 快照, 防止中断修改 */

    drv8313_set_phase(
        comm_pattern[step][0] ? d : 0,  /* U */
        comm_pattern[step][1] ? d : 0,  /* V */
        comm_pattern[step][2] ? d : 0   /* W */
    );
}

/* ================================================================
 * systick_init: SysTick 1ms
 * ================================================================ */
static void systick_init(void)
{
    if (SysTick_Config(SystemCoreClock / 1000)) {
        while (1);      /* 配置失败: 死循环, 方便调试 */
    }
}

/* ================================================================
 * SysTick_Handler: 1ms 中断 — 换相定时 + 加速斜坡 + 过流保护
 * ================================================================ */
void SysTick_Handler(void)
{
    if (!motor_on) return;

    comm_tick++;

    /* 过流保护: 任一相超过阈值则立即停机 */
    if (ina199_read_u_raw() > OC_THRESHOLD_ADC ||
        ina199_read_v_raw() > OC_THRESHOLD_ADC) {
        motor_stop();
        return;
    }

    /* 换相定时 */
    if (comm_tick >= step_ms) {
        comm_tick = 0;
        comm_step++;
        if (comm_step >= 3) {
            comm_step = 0;                  /* 3步 = 1 电周期       */
            rev_count++;

            /* 加速斜坡 */
            if (rev_count >= RAMP_REVS) {
                rev_count = 0;
                if (step_ms > STEP_MS_END + STEP_MS_DEC) {
                    step_ms -= STEP_MS_DEC;
                }
            }
        }
        motor_commutate(comm_step);
    }
}

/* ================================================================
 * motor_set_duty / motor_get_duty: 运行时调整占空比
 * ================================================================ */
void motor_set_duty(uint16_t duty)
{
    if (duty > PWM_MAX) duty = PWM_MAX;
    motor_duty = duty;
}

uint16_t motor_get_duty(void)
{
    return motor_duty;
}

/* ================================================================
 * 传感器接口: 封装 MT6701 / INA199 驱动
 * ================================================================ */
float motor_read_angle(void)
{
    return mt6701_read_angle_deg();
}

float motor_read_current_u(void)
{
    return ina199_read_u_amps();
}

float motor_read_current_v(void)
{
    return ina199_read_v_amps();
}
