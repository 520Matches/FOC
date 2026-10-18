/*********************************************************************
*                   (c) SEGGER Microcontroller GmbH                  *
*                        The Embedded Experts                        *
*                           www.segger.com                           *
**********************************************************************
*                                                                    *
*        SEGGER RTT * Real Time Transfer for embedded targets        *
*                  https://github.com/SEGGERMicro/RTT                *
*                                                                    *
**********************************************************************

---------------------------END-OF-HEADER------------------------------
Purpose : User configuration file for RTT.
          For available configuration,
          refer to SEGGER_RTT_ConfDefaults.h.

----------------------------------------------------------------------
*/

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H


/*********************************************************************
*
*       Defines, configurable
*
*  目标: STM32F103C8T6 (Cortex-M3, 20KB SRAM, 64KB Flash, 无 Cache)
*
**********************************************************************
*/

//
// F1 没有 D-Cache, 必须为 0。
// 如果留成非 0, RTT 会额外做 cache-line 对齐和 cache 维护, 白白吃掉几百字节 RAM。
//
#define SEGGER_RTT_CPU_CACHE_LINE_SIZE      (0)

//
// 通道数量。默认各 3 个, 每个通道描述符 24 字节。
// 目前只用到 Terminal(通道 0), 留 1 个备用通道给后面的 FOC 波形数据流。
//
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS       (2)
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS     (2)

//
// 上行(Target -> Host)缓冲, 即终端打印缓冲。1KB 足够。
//
#define BUFFER_SIZE_UP                      (1024)

//
// 下行(Host -> Target)缓冲, 即终端输入缓冲。
// 默认是 16 字节, 从主机敲命令过来很容易被截断, 放大到 64。
//
#define BUFFER_SIZE_DOWN                    (64)

//
// 通道默认模式 —— 这一条非常关键。
// NO_BLOCK_SKIP: 缓冲区满时直接丢弃本次数据, 绝不阻塞。
// 如果没有主机(OpenOCD/RTT Viewer)连接, 上行缓冲会一直满,
// 一旦配成 BLOCK_IF_FIFO_FULL, MCU 会永远卡死在 SEGGER_RTT_Write() 里,
// 表现就是"程序像没跑一样"。学习阶段必须保持非阻塞。
//
#define SEGGER_RTT_MODE_DEFAULT             SEGGER_RTT_MODE_NO_BLOCK_SKIP

//
// SEGGER_RTT_printf() 的临时格式化缓冲(放在栈上)。
// 默认 64 字节, 日志行稍长就会被截断, 放到 128。
//
#define SEGGER_RTT_PRINTF_BUFFER_SIZE       (128u)

//
// 打开 %f —— 由本工程自己在 SEGGER_RTT_printf.c 里实现, **不走 newlib**。
//
//   为什么不用 snprintf / newlib 的 %f:
//     1) newlib-nano 默认不编浮点 printf, 要 -u _printf_float 才生效;
//     2) 一旦生效, _dtoa_r + _vfprintf_r 会把 malloc/free 一族拉进来,
//        实测 +10KB 左右 Flash —— 这块 F103C8 只有 64KB, 不划算;
//     3) 自带实现也不开 TICKINT/中断, 不阻塞, 和现有 NO_BLOCK_SKIP 一致。
//   代价: 约 +1KB Flash (double 的软浮点四则 + 比较, 全在 libgcc 里)。
//
//   关掉它: make EXTRA_CFLAGS=-DSEGGER_RTT_PRINTF_FLOAT_ENABLE=0
//   (直接改这里的 0/1 也可以; 但别写 `make CFLAGS+=...`, 见 Makefile 里的注释)
//   打开后 %f 的行为见 SEGGER_RTT_printf.c 顶部注释(有明确的量程上限)。
//
#ifndef SEGGER_RTT_PRINTF_FLOAT_ENABLE
  //
  // 兼容旧写法: 早期 log.h 注释里提到的是 SEGGER_RTT_PRINT_FLOAT_ENABLE,
  // 两个宏都认, 免得 -D 写哪个都心里没底。
  //
  #ifdef SEGGER_RTT_PRINT_FLOAT_ENABLE
    #define SEGGER_RTT_PRINTF_FLOAT_ENABLE  (SEGGER_RTT_PRINT_FLOAT_ENABLE)
  #else
    #define SEGGER_RTT_PRINTF_FLOAT_ENABLE  (1)
  #endif
#endif

//
// RTT 临界区实现(Cortex-M3 分支 __ARM_ARCH_7M__):
// 默认用 BASEPRI 屏蔽优先级号 >= 0x20 的中断, 而不是粗暴关总中断,
// 这样 FOC 的高优先级 PWM/换相中断不会被日志打印拖住。
// 0x20 本身就是默认值, 这里显式写出来方便以后调。
//
#define SEGGER_RTT_MAX_INTERRUPT_PRIORITY   (0x20)

#endif
/*************************** End of file ****************************/
