#ifndef __PID_H
#define __PID_H


// 假设 SVPWM 的最大线性调制电压为 Vdc/sqrt(3)，这里以 24V 母线电压为例
#define VDC_BUS 12.0f
#define MAX_VOLTAGE (VDC_BUS * 0.577f) // 约 13.8V
									   
					
					
typedef struct {
    // 参数设置
    float Kp;           // 比例系数
    float Ki;           // 积分系数
    float Kd;           // 微分系数（电流环通常设为0）
    
    // 限幅设置
    float out_max;      // 输出最大限幅 (例如 SVPWM 的最大电压)
    float out_min;      // 输出最小限幅
    float integral_max; // 积分项最大限幅 (防止积分饱和)
    float integral_min; // 积分项最小限幅
    
    // 状态变量
    float target;       // 目标值 (Id_ref / Iq_ref)
    float actual;       // 实际反馈值 (Id_fdb / Iq_fdb)
    float error;        // 当前误差
    float last_error;   // 上次误差 (用于D项)
    float integral;     // 积分累加项
    float output;       // PID 输出
} PID_TypeDef;

extern PID_TypeDef pid_id; // d轴电流环
extern PID_TypeDef pid_iq; // q轴电流环
					
// 函数声明
void PID_Init(PID_TypeDef *pid, float kp, float ki, float kd, float out_max, float out_min, float int_max, float int_min);
float PID_Calc(PID_TypeDef *pid, float target, float actual);

#endif

