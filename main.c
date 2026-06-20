#include "stm32f10x.h"
#include <stdio.h>
#include "uart.h"      /* uart_init, printf→USART2 */
#include "motor.h"     /* motor_init/start/stop    */
#include "drv8313.h"   /* drv8313_set_phase 等     */
#include "mt6701.h"    /* mt6701_read_angle_raw    */
#include "ina199.h"    /* ina199_read_u_raw        */

/* ==================================================================
 * 驱动测试 + 串口日志 (ST-Link V2 或 USB-TTL)
 *
 * 接线: ST-Link TX→PA3(RX), ST-Link RX→PA2(TX), GND→GND
 * 串口工具: 115200-8-N-1
 *
 * 注: 为避免 Flash 溢出, 浮点数拆为整数+小数打印
 *     例如 271.3° 打印为 "271.3"
 * ================================================================== */

/* ---- 定点数打印: 角度 (1位小数) ---- */
static void print_angle(float deg)
{
    int a = (int)deg;
    int f = (int)((deg - (float)a) * 10.0f + 0.5f);
    printf("%d.%d", a, f);
}

/* ---- 定点数打印: 电流 (2位小数, 单位 A) ---- */
static void print_amps(float a)
{
    int sign = (a < 0) ? -1 : 1;
    int val  = (int)(a * sign * 100.0f + 0.5f);
    printf("%c%d.%02d", (sign < 0) ? '-' : ' ', val / 100, val % 100);
}

/* ---- 软件延时 (~ms @72MHz, 粗略) ---- */
static void delay_approx_ms(uint32_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++) __NOP();
}

/* ================================================================
 * test_mt6701: 每秒打印编码器角度
 *   输出: "[MT6701] raw=12345 angle=271.3 mg=8"
 * ================================================================ */
static void test_mt6701(void)
{
    mt6701_init();
    printf("[MT6701] Test Start\r\n");

    while (1) {
        uint16_t raw   = mt6701_read_angle_raw();
        float    angle = mt6701_read_angle_deg();
        uint8_t  mg    = mt6701_read_status();

        printf("[MT6701] raw=%5u  angle=", raw);
        print_angle(angle);
        printf("  mg=%d\r\n", mg);

        delay_approx_ms(1000);
    }
}

/* ================================================================
 * test_ina199: 每秒打印 U/V 相电流
 *   输出: "[INA199] U=2048(+0.00A) V=2048(+0.00A)"
 * ================================================================ */
static void test_ina199(void)
{
    ina199_init();
    printf("[INA199] Test Start — 零电流≈2048\r\n");

    while (1) {
        uint16_t u_raw  = ina199_read_u_raw();
        uint16_t v_raw  = ina199_read_v_raw();
        float    u_amps = ina199_read_u_amps();
        float    v_amps = ina199_read_v_amps();

        printf("[INA199] U=%4u(", u_raw);
        print_amps(u_amps);
        printf(")  V=%4u(", v_raw);
        print_amps(v_amps);
        printf(")\r\n");

        delay_approx_ms(1000);
    }
}

/* ================================================================
 * test_drv8313: U→V→W 依次 10% PWM 输出, 500ms/相
 * ================================================================ */
static void test_drv8313(void)
{
    drv8313_init();
    drv8313_enable();
    printf("[DRV8313] U→V→W 依次 10%% PWM, 500ms/相\r\n");

    int ph = 0;
    while (1) {
        if      (ph == 0) { drv8313_set_phase(144, 0, 0); printf("[DRV8313] U ON\r\n");  }
        else if (ph == 1) { drv8313_set_phase(0, 144, 0); printf("[DRV8313] V ON\r\n");  }
        else              { drv8313_set_phase(0, 0, 144); printf("[DRV8313] W ON\r\n");  }
        ph = (ph + 1) % 3;
        delay_approx_ms(500);
    }
}

/* ================================================================
 * test_motor: 电机启动, 每秒打印: 角度/电流/占空比/换相间隔
 * ================================================================ */
static void test_motor(void)
{
    motor_init();
    printf("[MOTOR] Init OK, starting...\r\n");
    motor_start();

    uint32_t loop = 0;
    while (1) {
        loop++;
        if (loop % 720000 == 0) {   /* 约 1 秒 */
            float    ang  = motor_read_angle();
            float    iu   = motor_read_current_u();
            float    iv   = motor_read_current_v();
            uint16_t duty = motor_get_duty();

            printf("[MOTOR] ang=");
            print_angle(ang);
            printf("  IU=");
            print_amps(iu);
            printf("  IV=");
            print_amps(iv);
            printf("  duty=%3u\r\n", duty);
        }
        __NOP();
    }
}

/* ================================================================
 * main
 * ================================================================ */
int main(void)
{
    uart_init();
    // printf("\r\n=== FOC Driver Test  %s %s ===\r\n\r\n",
    //        __DATE__, __TIME__);

    /* 取消注释切换测试 */
    // test_motor();           /* 默认: 电机旋转 + 日志  */
    // test_drv8313();      /* PWM 三相测试           */
    test_mt6701();       /* 编码器测试             */
    // test_ina199();       /* 电流采样测试           */

    return 0;
}
