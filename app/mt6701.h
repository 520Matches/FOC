#ifndef __MT6701_H
#define __MT6701_H

#include "stm32f10x.h"

/* MT6701 磁编码器 SSI 接口: PA4=CSN, PA5=SCLK, PA6=DO(MISO)
 * 24-bit 帧: D[13:0] 角度 + Mg[3:0] 磁场状态 + CRC[5:0]
 * 角度 = raw * 360.0 / 16384.0 */

void mt6701_init(void);               /* 初始化 GPIO                  */
uint16_t mt6701_read_angle_raw(void); /* 14-bit 原始角度 0~16383      */
float mt6701_read_angle_deg(void);    /* 角度 0.0~360.0°              */
uint8_t mt6701_read_status(void);     /* 4-bit 磁场状态               */

#endif
