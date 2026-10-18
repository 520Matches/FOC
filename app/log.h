#ifndef __LOG_H
#define __LOG_H

/* ================================================================
 * 极简日志层: 底层走 SEGGER RTT 通道 0
 *
 *  - 不占用任何 UART 引脚, 不需要额外接线, 只靠 SWD 的 SWDIO/SWCLK
 *  - 非阻塞 (SEGGER_RTT_MODE_NO_BLOCK_SKIP), 没接主机时静默丢弃
 *  - 编译期可裁剪: 把 LOG_LEVEL 调小即可整级消掉 Flash 占用
 *
 * 浮点: RTT 的 %f **可用**(默认打开), 但是**本工程自己在
 *       rtt/SEGGER_RTT_printf.c 的 _PrintFloat() 里实现的**, 不走 newlib:
 *         LOG_INFO("v = %.3f V  i = %+.2f A", 3.3, -1.25);   ← 直接就能用
 *       · 支持 宽度/精度/'-'左对齐/'0'补零/'+'号, 也支持 nan/inf;
 *         不支持 %e %g %a 和 long double(%Lf);
 *       · 传参必须是 double(可变参数里 float 会自动提升, 写 double 就对了);
 *       · 代价实测约 +2.6KB Flash(double 的软浮点库是大头), 关掉的办法:
 *             make EXTRA_CFLAGS=-DSEGGER_RTT_PRINTF_FLOAT_ENABLE=0
 *       · 返回值契约也有回归测试: make rtt-fmt-test
 * ================================================================ */

#include "SEGGER_RTT.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 日志级别: 0=关闭 1=ERROR 2=WARN 3=INFO 4=DEBUG */
#ifndef LOG_LEVEL
#define LOG_LEVEL       (4)
#endif

/* 终端颜色 (RTT 终端支持 ANSI 转义, 不识别时会原样显示, 无害) */
#define LOG_CLR_ERR     "\x1b[31m"
#define LOG_CLR_WRN     "\x1b[33m"
#define LOG_CLR_INF     "\x1b[32m"
#define LOG_CLR_DBG     "\x1b[90m"
#define LOG_CLR_RST     "\x1b[0m"

/* 裸输出, 不带前缀和换行, 自己拼一行时用 */
#define LOG_RAW(fmt, ...)   SEGGER_RTT_printf(0, (fmt), ##__VA_ARGS__)

#if LOG_LEVEL >= 1
  #define LOG_ERROR(fmt, ...) LOG_RAW(LOG_CLR_ERR "[E] " fmt LOG_CLR_RST "\n", ##__VA_ARGS__)
#else
  #define LOG_ERROR(fmt, ...) ((void)0)
#endif

#if LOG_LEVEL >= 2
  #define LOG_WARN(fmt, ...)  LOG_RAW(LOG_CLR_WRN "[W] " fmt LOG_CLR_RST "\n", ##__VA_ARGS__)
#else
  #define LOG_WARN(fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= 3
  #define LOG_INFO(fmt, ...)  LOG_RAW(LOG_CLR_INF "[I] " fmt LOG_CLR_RST "\n", ##__VA_ARGS__)
#else
  #define LOG_INFO(fmt, ...)  ((void)0)
#endif

#if LOG_LEVEL >= 4
  #define LOG_DEBUG(fmt, ...) LOG_RAW(LOG_CLR_DBG "[D] " fmt LOG_CLR_RST "\n", ##__VA_ARGS__)
#else
  #define LOG_DEBUG(fmt, ...) ((void)0)
#endif

/* 必须在 main() 最开头调用: 建立 RTT 控制块, 让主机一上电就能找到它 */
void log_init(void);

#ifdef __cplusplus
}
#endif

#endif /* __LOG_H */
