#include "pid.h"

PID_TypeDef pid_id; // d轴电流环
PID_TypeDef pid_iq; // q轴电流环

/**
  * @brief  PID 初始化
  * @param  pid: PID结构体指针
  * @param  kp, ki, kd: PID参数
  * @param  out_max, out_min: 输出限幅
  * @param  int_max, int_min: 积分限幅
  */
void PID_Init(PID_TypeDef *pid, float kp, float ki, float kd, 
              float out_max, float out_min, float int_max, float int_min) 
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    
    pid->out_max = out_max;
    pid->out_min = out_min;
    pid->integral_max = int_max;
    pid->integral_min = int_min;
    
    pid->target = 0.0f;
    pid->actual = 0.0f;
    pid->error = 0.0f;
    pid->last_error = 0.0f;
    pid->integral = 0.0f;
    pid->output = 0.0f;
}

/**
  * @brief  PID 计算函数 (带抗积分饱和)
  * @param  pid: PID结构体指针
  * @param  target: 目标电流
  * @param  actual: 实际电流
  * @retval PID 输出 (Vd 或 Vq)
  */
float PID_Calc(PID_TypeDef *pid, float target, float actual) 
{
    float p_out, i_out, d_out;
    
    // 1. 更新目标与反馈
    pid->target = target;
    pid->actual = actual;
    
    // 2. 计算误差
    pid->error = pid->target - pid->actual;
    
    // 3. 比例项
    p_out = pid->Kp * pid->error;
    
    // 4. 积分项 (带积分限幅)
    pid->integral += pid->Ki * pid->error;
    
    // 积分抗饱和 (Clamping)
    if (pid->integral > pid->integral_max) {
        pid->integral = pid->integral_max;
    } else if (pid->integral < pid->integral_min) {
        pid->integral = pid->integral_min;
    }
    i_out = pid->integral;
    
    // 5. 微分项 (电流环通常设为0，或做低通滤波)
    // 如果 Kd = 0，这一项直接为 0，不产生计算开销
    if (pid->Kd > 0.0f) {
        d_out = pid->Kd * (pid->error - pid->last_error);
    } else {
        d_out = 0.0f;
    }
    pid->last_error = pid->error;
    
    // 6. 计算总输出
    pid->output = p_out + i_out + d_out;
    
    // 7. 输出限幅 (防止超出 SVPWM 线性调制区)
    if (pid->output > pid->out_max) {
        pid->output = pid->out_max;
    } else if (pid->output < pid->out_min) {
        pid->output = pid->out_min;
    }
    
    return pid->output;
}
