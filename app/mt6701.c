#include "mt6701.h"

/* ---- 引脚: PA4=CSN, PA5=SCLK, PA6=DO ---- */
#define CSN_PORT  GPIOA
#define CSN_PIN   GPIO_Pin_4
#define CLK_PORT  GPIOA
#define CLK_PIN   GPIO_Pin_5
#define DO_PORT   GPIOA
#define DO_PIN    GPIO_Pin_6

#define CSN_LOW()   GPIO_ResetBits(CSN_PORT, CSN_PIN)
#define CSN_HIGH()  GPIO_SetBits(CSN_PORT, CSN_PIN)
#define CLK_LOW()   GPIO_ResetBits(CLK_PORT, CLK_PIN)
#define CLK_HIGH()  GPIO_SetBits(CLK_PORT, CLK_PIN)
#define READ_DO()   GPIO_ReadInputDataBit(DO_PORT, DO_PIN)

/* ================================================================
 * mt6701_init: 软件 GPIO 模拟 SSI (避开硬件 SPI 24-bit 帧长限制)
 *   MT6701 SSI 时序: CSN↓ → 24 CLK → CSN↑
 *   DO 在 CLK 上升沿更新, 主机在 CLK 下降沿采样
 * ================================================================ */
void mt6701_init(void)
{
    GPIO_InitTypeDef g;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    /* CSN (PA4): 推挽输出, 初始高 (空闲) */
    g.GPIO_Pin  = CSN_PIN;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(CSN_PORT, &g);
    CSN_HIGH();

    /* SCLK (PA5): 推挽输出, 初始低 */
    g.GPIO_Pin  = CLK_PIN;
    GPIO_Init(CLK_PORT, &g);
    CLK_LOW();

    /* DO (PA6): 浮空输入 */
    g.GPIO_Pin  = DO_PIN;
    g.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(DO_PORT, &g);
}

/* ================================================================
 * ssix_read24: 读 24-bit SSI 帧, 返回完整 32-bit (低 24 位有效)
 *   帧格式: D13..D0 (bit23..10) | Mg3..Mg0 (bit9..6) | CRC (bit5..0)
 * ================================================================ */
static uint32_t ssix_read24(void)
{
    uint32_t data = 0;

    CSN_LOW();                          /* SSI 起始                   */
    for (int i = 0; i < 24; i++) {
        CLK_HIGH();                     /* 上升沿: MT6701 更新 DO     */
        __NOP(); __NOP(); __NOP();      /* 延时 >30ns (CLK 低电平)    */
        CLK_LOW();                      /* 下降沿: MCU 采样           */
        data <<= 1;
        if (READ_DO()) data |= 1;
    }
    CSN_HIGH();                         /* SSI 结束                   */
    return data;
}

/* ================================================================
 * mt6701_read_angle_raw: 读 14-bit 角度值 (0~16383)
 *   角度 = raw * 360.0 / 16384.0
 * ================================================================ */
uint16_t mt6701_read_angle_raw(void)
{
    uint32_t data = ssix_read24();
    return (uint16_t)((data >> 10) & 0x3FFF);  /* D[13:0] 在 bit23..10 */
}

/* ================================================================
 * mt6701_read_angle_deg: 返回角度 0.0 ~ 360.0°
 * ================================================================ */
float mt6701_read_angle_deg(void)
{
    return (float)mt6701_read_angle_raw() * 360.0f / 16384.0f;
}

/* ================================================================
 * mt6701_read_status: 读 4-bit 磁场强度 Mg[3:0]
 *   正常值在中间范围, 过大/过小 → 磁铁距离异常
 * ================================================================ */
uint8_t mt6701_read_status(void)
{
    uint32_t data = ssix_read24();
    return (uint8_t)((data >> 6) & 0x0F);  /* Mg[3:0] 在 bit9..6  */
}
