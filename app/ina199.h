#ifndef __INA199_H
#define __INA199_H

#include "stm32f10x.h"

/* INA199A1 电流采样: 50V/V 增益, 20mΩ 采样电阻, VREF=1.65V
 * I = (Vout - 1.65) / (0.02 * 50) = Vout - 1.65  (单位: A)
 * ADC 12-bit @ 3.3V: Vout = adc_val * 3.3 / 4096   */

void ina199_init(void);               /* 初始化 ADC1+DMA, PA0/PA1    */
uint16_t ina199_read_u_raw(void);     /* U 相 ADC 原始值 (DMA 缓冲)  */
uint16_t ina199_read_v_raw(void);     /* V 相 ADC 原始值              */
float ina199_read_u_amps(void);       /* U 相电流 (A)                */
float ina199_read_v_amps(void);       /* V 相电流 (A)                */

#endif
