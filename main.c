#include "stm32f10x.h"
#include "motor.h"     /* motor_init/start/stop    */
#include "drv8313.h"   /* drv8313_set_phase 等     */
#include "mt6701.h"    /* mt6701_read_angle_raw    */
#include "ina199.h"    /* ina199_read_u_raw        */
#include "log.h"       /* RTT 日志                 */
#include "uart.h"      /* uart_poll_rx, 回环自检    */
#include "pid.h"

#include "math.h"
#define PI_F    3.14159265f

typedef struct
{
    float u;
    float v;
    float w;
}duty_t;

typedef struct
{
    float Ia;
    float Ib;
}clark_t;

typedef struct
{
    float Id;
    float Iq;
}park_t;

typedef struct
{
	float Iu;
	float Iv;
	float Iw;
}I_t;

duty_t duty;
clark_t clark;
park_t park;
I_t ampere;

//angle=(0~360)
void svpwm(int angle, float s)
{
    int v[6][3] = {{0,1,1},{0,0,1},{1,0,1},{1,0,0},{1,1,0},{0,1,0}};
    int selector = angle / 60;

    float tm = s * sinf(((selector + 1) * 60 - angle) * PI_F / 180);
    float tn = s * sinf((angle - selector * 60) * PI_F / 180);
    float t0 = 1 - tm - tn;

    duty.u = tm * v[selector][0] + tn*v[(selector + 1) % 6][0] + t0 / 2;
    duty.v = tm * v[selector][1] + tn*v[(selector + 1) % 6][1] + t0 / 2;
    duty.w = tm * v[selector][2] + tn*v[(selector + 1) % 6][2] + t0 / 2;
}

void clark_transform(I_t i)
{
    clark.Ia = (i.Iu - i.Iv / 2 - i.Iw / 2) * 2 / 3;
    clark.Ib = (i.Iv * cosf(PI_F / 6) - i.Iw * cosf(PI_F / 6)) * 2 / 3;
}

void park_transform(clark_t clark, int angle)
{
    park.Id = clark.Ia * cosf(angle * PI_F / 180) + clark.Ib * sinf(angle * PI_F / 180);
    park.Iq = clark.Ib * cosf(angle * PI_F / 180) - clark.Ia * sinf(angle * PI_F / 180);
}

void delay_ms(uint32_t ms)
{
    uint32_t reload = SystemCoreClock / 1000u;       /* 1ms 折算成 SysTick 计数 */

    if (reload == 0u)         reload = 1u;
    if (reload > 0x00FFFFFFu) reload = 0x00FFFFFFu;  /* LOAD 只有 24 位 */

    SysTick->LOAD = reload - 1u;
    SysTick->VAL  = 0u;
    /* 时钟源 = 内核时钟(72MHz); 不置 TICKINT -> 纯轮询, 不产生中断 */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (ms--) {
        /* COUNTFLAG 由硬件在计数归零时置 1(读一次自动清零), 置位即过了 1ms */
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0u) {
        }
    }

    SysTick->CTRL = 0u;      /* 用完关掉, 把 SysTick 让回给 motor 那边 */
    SysTick->VAL  = 0u;
}

#define POLE_PAIRS	7
#define MACH_ANGLE (360 / 7)

float elec_angle_offset = 0;

int main(void)
{
    log_init();
    SystemCoreClockUpdate();

    ina199_init();               /* 初始化 ADC1+DMA, PA0/PA1    */
    mt6701_init();               /* 初始化 GPIO                  */
    motor_init();
	
    // 初始化 d轴 PID (通常 Id_ref = 0, 所以误差较小)
    // Kp=2.0, Ki=0.05, Kd=0, 输出限幅 ±MAX_VOLTAGE, 积分限幅 ±MAX_VOLTAGE
    PID_Init(&pid_id, 2.0f, 0.0f, 0.5f, MAX_VOLTAGE, -MAX_VOLTAGE, MAX_VOLTAGE, -MAX_VOLTAGE);
    
    // 初始化 q轴 PID (Iq_ref 由速度环输出，负责产生转矩)
    PID_Init(&pid_iq, 2.0f, 0.0f, 0.5f, MAX_VOLTAGE, -MAX_VOLTAGE, MAX_VOLTAGE, -MAX_VOLTAGE);

    /* ---- 电零点标定：固定磁场让转子对齐，多次采样取平均 ---- */
    svpwm(0, 0.3f);
    motor_set_duty(duty.u, duty.v, duty.w);
    delay_ms(1000);                              /* 等转子尽量停稳 */

    float align_sum = 0.0f;
    for (int i = 0; i < 10; i++) {
        align_sum += mt6701_read_angle_deg();
        delay_ms(10);
    }
    float elec_angle_offset = align_sum / 10.0f;

	for(int i = 0; i < POLE_PAIRS; i++)
	{
		if(MACH_ANGLE > elec_angle_offset)
		{
			break;
		}
		else
		{
			elec_angle_offset -= MACH_ANGLE;
		}
	}


    uint32_t cnt = 0;
	float tag_id = 0.3;
	float tag_iq = 0;
	float now_angle = 0;
	float Vd,Vq;
    while (1)
    {
		ampere.Iu = ina199_read_u_amps();
		ampere.Iv = ina199_read_v_amps();
		ampere.Iw = 0 - ampere.Iu - ampere.Iv;

		now_angle = mt6701_read_angle_deg();

		clark_transform(ampere);
		park_transform(clark, (now_angle - elec_angle_offset) * 7);

		Vq = PID_Calc(&pid_iq, 0.5, park.Iq);
		Vd = PID_Calc(&pid_id, 0, park.Id);
    }

    return 0;
}
