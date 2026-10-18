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
Purpose : Replacement for printf to write formatted data via RTT

----------------------------------------------------------------------
*/
#include "SEGGER_RTT.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#ifndef SEGGER_RTT_PRINTF_BUFFER_SIZE
  #define SEGGER_RTT_PRINTF_BUFFER_SIZE (64)
#endif

#include <stdlib.h>
#include <stdarg.h>


#define FORMAT_FLAG_LEFT_JUSTIFY   (1u << 0)
#define FORMAT_FLAG_PAD_ZERO       (1u << 1)
#define FORMAT_FLAG_PRINT_SIGN     (1u << 2)
#define FORMAT_FLAG_ALTERNATE      (1u << 3)

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

typedef struct {
  char*     pBuffer;
  unsigned  BufferSize;
  unsigned  Cnt;

  int   ReturnValue;

  unsigned RTTBufferIndex;
} SEGGER_RTT_PRINTF_DESC;

/*********************************************************************
*
*       Function prototypes
*
**********************************************************************
*/

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/
/*********************************************************************
*
*       _StoreChar
*/
static void _StoreChar(SEGGER_RTT_PRINTF_DESC * p, char c) {
  unsigned Cnt;

  Cnt = p->Cnt;
  if ((Cnt + 1u) <= p->BufferSize) {
    *(p->pBuffer + Cnt) = c;
    p->Cnt = Cnt + 1u;
    p->ReturnValue++;
  }
  //
  // Write part of string, when the buffer is full
  //
  if (p->Cnt == p->BufferSize) {
    if (SEGGER_RTT_Write(p->RTTBufferIndex, p->pBuffer, p->Cnt) != p->Cnt) {
      p->ReturnValue = -1;
    } else {
      p->Cnt = 0u;
    }
  }
}

/*********************************************************************
*
*       _PrintUnsigned
*/
static void _PrintUnsigned(SEGGER_RTT_PRINTF_DESC * pBufferDesc, unsigned v, unsigned Base, unsigned NumDigits, unsigned FieldWidth, unsigned FormatFlags) {
  static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  unsigned Div;
  unsigned Digit;
  unsigned Number;
  unsigned Width;
  char c;

  Number = v;
  Digit = 1u;
  //
  // Get actual field width
  //
  Width = 1u;
  while (Number >= Base) {
    Number = (Number / Base);
    Width++;
  }
  if (NumDigits > Width) {
    Width = NumDigits;
  }
  //
  // Print leading chars if necessary
  //
  if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) {
    if (FieldWidth != 0u) {
      if (((FormatFlags & FORMAT_FLAG_PAD_ZERO) == FORMAT_FLAG_PAD_ZERO) && (NumDigits == 0u)) {
        c = '0';
      } else {
        c = ' ';
      }
      while ((FieldWidth != 0u) && (Width < FieldWidth)) {
        FieldWidth--;
        _StoreChar(pBufferDesc, c);
        if (pBufferDesc->ReturnValue < 0) {
          break;
        }
      }
    }
  }
  if (pBufferDesc->ReturnValue >= 0) {
    //
    // Compute Digit.
    // Loop until Digit has the value of the highest digit required.
    // Example: If the output is 345 (Base 10), loop 2 times until Digit is 100.
    //
    while (1) {
      if (NumDigits > 1u) {       // User specified a min number of digits to print? => Make sure we loop at least that often, before checking anything else (> 1 check avoids problems with NumDigits being signed / unsigned)
        NumDigits--;
      } else {
        Div = v / Digit;
        if (Div < Base) {        // Is our divider big enough to extract the highest digit from value? => Done
          break;
        }
      }
      Digit *= Base;
    }
    //
    // Output digits
    //
    do {
      Div = v / Digit;
      v -= Div * Digit;
      _StoreChar(pBufferDesc, _aV2C[Div]);
      if (pBufferDesc->ReturnValue < 0) {
        break;
      }
      Digit /= Base;
    } while (Digit);
    //
    // Print trailing spaces if necessary
    //
    if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == FORMAT_FLAG_LEFT_JUSTIFY) {
      if (FieldWidth != 0u) {
        while ((FieldWidth != 0u) && (Width < FieldWidth)) {
          FieldWidth--;
          _StoreChar(pBufferDesc, ' ');
          if (pBufferDesc->ReturnValue < 0) {
            break;
          }
        }
      }
    }
  }
}

/*********************************************************************
*
*       _PrintInt
*/
static void _PrintInt(SEGGER_RTT_PRINTF_DESC * pBufferDesc, int v, unsigned Base, unsigned NumDigits, unsigned FieldWidth, unsigned FormatFlags) {
  unsigned Width;
  int Number;

  Number = (v < 0) ? -v : v;

  //
  // Get actual field width
  //
  Width = 1u;
  while (Number >= (int)Base) {
    Number = (Number / (int)Base);
    Width++;
  }
  if (NumDigits > Width) {
    Width = NumDigits;
  }
  if ((FieldWidth > 0u) && ((v < 0) || ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) == FORMAT_FLAG_PRINT_SIGN))) {
    FieldWidth--;
  }

  //
  // Print leading spaces if necessary
  //
  if ((((FormatFlags & FORMAT_FLAG_PAD_ZERO) == 0u) || (NumDigits != 0u)) && ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u)) {
    if (FieldWidth != 0u) {
      while ((FieldWidth != 0u) && (Width < FieldWidth)) {
        FieldWidth--;
        _StoreChar(pBufferDesc, ' ');
        if (pBufferDesc->ReturnValue < 0) {
          break;
        }
      }
    }
  }
  //
  // Print sign if necessary
  //
  if (pBufferDesc->ReturnValue >= 0) {
    if (v < 0) {
      v = -v;
      _StoreChar(pBufferDesc, '-');
    } else if ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) == FORMAT_FLAG_PRINT_SIGN) {
      _StoreChar(pBufferDesc, '+');
    } else {

    }
    if (pBufferDesc->ReturnValue >= 0) {
      //
      // Print leading zeros if necessary
      //
      if (((FormatFlags & FORMAT_FLAG_PAD_ZERO) == FORMAT_FLAG_PAD_ZERO) && ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) && (NumDigits == 0u)) {
        if (FieldWidth != 0u) {
          while ((FieldWidth != 0u) && (Width < FieldWidth)) {
            FieldWidth--;
            _StoreChar(pBufferDesc, '0');
            if (pBufferDesc->ReturnValue < 0) {
              break;
            }
          }
        }
      }
      if (pBufferDesc->ReturnValue >= 0) {
        //
        // Print number without sign
        //
        _PrintUnsigned(pBufferDesc, (unsigned)v, Base, NumDigits, FieldWidth, FormatFlags);
      }
    }
  }
}

#if SEGGER_RTT_PRINTF_FLOAT_ENABLE

//
// 小数点后最多打几位。
//   double 的有效位就 15~16 位, 再往后全是垃圾; 9 位足够覆盖
//   "电流 0.001mA / 角度 0.001° / 电压 0.001V" 这类实际需求。
//
#define SEGGER_RTT_PRINTF_FLOAT_PREC_MAX   (9u)

//
// 整数部分的上限。
//   %f 的整数部分用 32 位无符号数打(复用 _PrintUnsigned), 所以不能超过
//   UINT32_MAX。留一点余量给四舍五入的进位。超过就按 inf 打。
//   实际用途(72MHz 时钟 = 7.2e7, 电流/角度/占空比都 < 1e4)远够用。
//
#define SEGGER_RTT_PRINTF_FLOAT_INT_MAX    (4000000000.0)

/*********************************************************************
*
*       _PrintFloatText
*
*  Function description
*    用字段宽度排版一个纯文本(给 nan / inf 用), '-' 左对齐, 其余右对齐。
*    不进这里的路径只有 3 个字符串, 所以不做补零(对 inf 补零没意义)。
*/
static void _PrintFloatText(SEGGER_RTT_PRINTF_DESC * p, const char * s, unsigned FieldWidth, unsigned FormatFlags) {
  unsigned Len;
  unsigned Pad;

  Len = 0u;
  while (s[Len] != '\0') {
    Len++;
  }
  Pad = (FieldWidth > Len) ? (FieldWidth - Len) : 0u;
  //
  // 右对齐: 先补空格
  //
  if (((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) && (Pad != 0u)) {
    while (Pad != 0u) {
      Pad--;
      _StoreChar(p, ' ');
      if (p->ReturnValue < 0) {
        return;
      }
    }
  }
  while (*s != '\0') {
    _StoreChar(p, *s);
    s++;
    if (p->ReturnValue < 0) {
      return;
    }
  }
  //
  // 左对齐: 空格补在后面
  //
  if (((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) != 0u) && (Pad != 0u)) {
    while (Pad != 0u) {
      Pad--;
      _StoreChar(p, ' ');
      if (p->ReturnValue < 0) {
        return;
      }
    }
  }
}

/*********************************************************************
*
*       _PrintFloat
*
*  Function description
*    按 printf 的规则排版一个 double (%f / %F)。
*
*  实现要点(为什么这么写):
*    1) 不调 newlib 的 printf/snprintf -> 不引入 malloc, 不引入 _dtoa_r。
*    2) 整数部分直接强转成 unsigned 再复用 _PrintUnsigned —— 除法走的是
*       Cortex-M3 的硬件 UDIV 指令, 不用 libgcc 的 64 位除法(那要 +400B)。
*    3) 小数部分不做 10^n 放大(那样大数会丢精度), 而是"逐位乘 10 取整":
*         0.375 -> *10 = 3.75 取 3 -> 0.75 -> *10 = 7.5 取 7 -> ...
*       先加半个最低位再取位, 就得到了四舍五入; 加完之后如果 >= 1.0,
*       说明进位到了整数部分, 直接在 IntPart 上加 1 再继续 —— 这样
*       "9.999 保留 2 位" 能正确输出 10.00 而不是 9.99。
*    4) 支持 '-' 左对齐 / '0' 补零(补在符号之后) / '+' / 字段宽度 / 精度。
*       不支持的: %e %g %a、long double(即 %Lf)。
*
*  参数:
*    pBufferDesc  输出描述符
*    v            要打印的值(带符号)
*    Precision    小数点后位数(调用者已填好默认值)
*    FieldWidth   最小字段宽度
*    FormatFlags  FORMAT_FLAG_* 位
*/
static void _PrintFloat(SEGGER_RTT_PRINTF_DESC * pBufferDesc, double v, unsigned Precision, unsigned FieldWidth, unsigned FormatFlags) {
  char     acFrac[SEGGER_RTT_PRINTF_FLOAT_PREC_MAX];
  double   Frac;
  double   Half;
  unsigned NumFrac;
  unsigned NumInt;
  unsigned Content;
  unsigned Fill;
  unsigned i;
  int      Sign;      /* 0=不打印符号  1='-'  2='+' */
  int      PadZero;

  //
  // 特殊值先处理掉, 免得后面强转出垃圾数字。
  // 顺序不能反: NaN 跟任何数比较都是 false, 只有 v != v 能抓出来。
  // 这里用**带符号**的 v 判断, 所以 "-inf" 不会被 %+f 打成 "+inf"。
  //
  if (v != v) {
    _PrintFloatText(pBufferDesc, "nan", FieldWidth, FormatFlags);
    return;
  }
  if (v > SEGGER_RTT_PRINTF_FLOAT_INT_MAX) {   /* 含 +Inf */
    _PrintFloatText(pBufferDesc, ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) != 0u) ? "+inf" : "inf", FieldWidth, FormatFlags);
    return;
  }
  if (v < -SEGGER_RTT_PRINTF_FLOAT_INT_MAX) {  /* 含 -Inf */
    _PrintFloatText(pBufferDesc, "-inf", FieldWidth, FormatFlags);
    return;
  }

  //
  // 定符号, 然后只在绝对值上做排版
  //
  Sign = 0;
  if (v < 0.0) {
    v = -v;
    Sign = 1;
  } else if ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) != 0u) {
    Sign = 2;
  }

  if (Precision > SEGGER_RTT_PRINTF_FLOAT_PREC_MAX) {
    Precision = SEGGER_RTT_PRINTF_FLOAT_PREC_MAX;
  }
  NumFrac = Precision;

  //
  // 拆成 "整数部分 + 小数部分"
  //
  {
    unsigned IntPart = (unsigned)v;            /* 向零截断; v < 4e9 所以安全 */
    Frac = v - (double)IntPart;

    //
    // 四舍五入: 加半个最低位, 满了就进位到整数部分
    //
    Half = 0.5;
    for (i = 0u; i < NumFrac; i++) {
      Half *= 0.1;
    }
    Frac += Half;
    if (Frac >= 1.0) {
      Frac -= 1.0;
      IntPart++;
    }

    //
    // 逐位取小数(乘 10 取整数位)。d > 9 是浮点误差兜底, 例如 0.999999999
    // 在最后一位会算出 9.999999..., 强转仍是 9, 但保险起见夹一下。
    //
    for (i = 0u; i < NumFrac; i++) {
      unsigned d;
      Frac *= 10.0;
      d = (unsigned)Frac;
      if (d > 9u) {
        d = 9u;
      }
      Frac -= (double)d;
      acFrac[i] = (char)('0' + d);
    }

    //
    // 整数部分位数(用于算填充); 用硬件 UDIV, 代价可忽略。
    //
    NumInt = 1u;
    {
      unsigned t = IntPart;
      while (t >= 10u) {
        t /= 10u;
        NumInt++;
      }
    }

    Content = NumInt;
    if (NumFrac != 0u) {
      Content += NumFrac + 1u;                 /* '.' + 小数位 */
    }
    if (Sign != 0) {
      Content += 1u;
    }
    Fill = (FieldWidth > Content) ? (FieldWidth - Content) : 0u;

    //
    // 先铺空格(仅在"右对齐 + 不补零"时; 补零要留到符号之后)
    //
    PadZero = (((FormatFlags & FORMAT_FLAG_PAD_ZERO) != 0u) && ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u));
    if (((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) && !PadZero && (Fill != 0u)) {
      while (Fill != 0u) {
        Fill--;
        _StoreChar(pBufferDesc, ' ');
        if (pBufferDesc->ReturnValue < 0) {
          return;
        }
      }
    }
    //
    // 符号: 负数 '-' , '+'-flag 时正数也打 '+'
    //
    if (Sign != 0) {
      _StoreChar(pBufferDesc, (Sign == 1) ? '-' : '+');
      if (pBufferDesc->ReturnValue < 0) {
        return;
      }
    }
    //
    // 补零必须排在符号后面("%08.2f" 的 -1.5 -> -0001.50)
    //
    if (PadZero && (Fill != 0u)) {
      while (Fill != 0u) {
        Fill--;
        _StoreChar(pBufferDesc, '0');
        if (pBufferDesc->ReturnValue < 0) {
          return;
        }
      }
    }
    //
    // 整数部分: 复用整数打印(字段宽度/精度都传 0, 填充已经自己做完了)
    //
    _PrintUnsigned(pBufferDesc, IntPart, 10u, 0u, 0u, 0u);
    if (pBufferDesc->ReturnValue < 0) {
      return;
    }
    //
    // 小数点 + 小数位
    //
    if (NumFrac != 0u) {
      _StoreChar(pBufferDesc, '.');
      for (i = 0u; i < NumFrac; i++) {
        _StoreChar(pBufferDesc, acFrac[i]);
        if (pBufferDesc->ReturnValue < 0) {
          return;
        }
      }
    }
    //
    // 左对齐: 尾巴补空格
    //
    if (((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) != 0u) && (Fill != 0u)) {
      while (Fill != 0u) {
        Fill--;
        _StoreChar(pBufferDesc, ' ');
        if (pBufferDesc->ReturnValue < 0) {
          return;
        }
      }
    }
  }
}

#endif  /* SEGGER_RTT_PRINTF_FLOAT_ENABLE */

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       SEGGER_RTT_vprintf
*
*  Function description
*    Stores a formatted string in SEGGER RTT control block.
*    This data is read by the host.
*
*  Parameters
*    BufferIndex  Index of "Up"-buffer to be used. (e.g. 0 for "Terminal")
*    sFormat      Pointer to format string
*    pParamList   Pointer to the list of arguments for the format string
*
*  Return values
*    >= 0:  Number of bytes which have been stored in the "Up"-buffer.
*     < 0:  Error
*/
int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList) {
  char c;
  SEGGER_RTT_PRINTF_DESC BufferDesc;
  int v;
  unsigned char PrecisionSet;
  unsigned Precision;
  unsigned FormatFlags;
  unsigned FieldWidth;
  char acBuffer[SEGGER_RTT_PRINTF_BUFFER_SIZE];

  BufferDesc.pBuffer        = acBuffer;
  BufferDesc.BufferSize     = SEGGER_RTT_PRINTF_BUFFER_SIZE;
  BufferDesc.Cnt            = 0u;
  BufferDesc.RTTBufferIndex = BufferIndex;
  BufferDesc.ReturnValue    = 0;

  do {
    c = *sFormat;
    sFormat++;
    if (c == 0u) {
      break;
    }
    if (c == '%') {
      //
      // Filter out flags
      //
      FormatFlags = 0u;
      v = 1;
      do {
        c = *sFormat;
        switch (c) {
        case '-': FormatFlags |= FORMAT_FLAG_LEFT_JUSTIFY; sFormat++; break;
        case '0': FormatFlags |= FORMAT_FLAG_PAD_ZERO;     sFormat++; break;
        case '+': FormatFlags |= FORMAT_FLAG_PRINT_SIGN;   sFormat++; break;
        case '#': FormatFlags |= FORMAT_FLAG_ALTERNATE;    sFormat++; break;
        default:  v = 0; break;
        }
      } while (v);
      //
      // filter out field with
      //
      FieldWidth = 0u;
      do {
        c = *sFormat;
        if ((c < '0') || (c > '9')) {
          break;
        }
        sFormat++;
        FieldWidth = (FieldWidth * 10u) + ((unsigned)c - '0');
      } while (1);

      //
      // Filter out precision (number of digits to display)
      //
      PrecisionSet = 0;
      Precision = 0u;
      c = *sFormat;
      if (c == '.') {
        sFormat++;
        if (*sFormat == '*') {
          sFormat++;
          PrecisionSet = 1;
          Precision = va_arg(*pParamList, int);
        } else {
          do {
            c = *sFormat;
            if ((c < '0') || (c > '9')) {
              break;
            }
            PrecisionSet = 1;
            sFormat++;
            Precision = Precision * 10u + ((unsigned)c - '0');
          } while (1);
        }
      }
      //
      // Filter out length modifier
      //
      c = *sFormat;
      do {
        if ((c == 'l') || (c == 'h')) {
          sFormat++;
          c = *sFormat;
        } else {
          break;
        }
      } while (1);
      //
      // Handle specifiers
      //
      switch (c) {
      case 'c': {
        char c0;
        v = va_arg(*pParamList, int);
        c0 = (char)v;
        _StoreChar(&BufferDesc, c0);
        break;
      }
      case 'd':
        v = va_arg(*pParamList, int);
        _PrintInt(&BufferDesc, v, 10u, Precision, FieldWidth, FormatFlags);
        break;
      case 'u':
        v = va_arg(*pParamList, int);
        _PrintUnsigned(&BufferDesc, (unsigned)v, 10u, Precision, FieldWidth, FormatFlags);
        break;
      case 'x':
      case 'X':
        v = va_arg(*pParamList, int);
        _PrintUnsigned(&BufferDesc, (unsigned)v, 16u, Precision, FieldWidth, FormatFlags);
        break;
      case 's':
        {
          const char * s = va_arg(*pParamList, const char *);
          if (s == NULL) {
            s = "(NULL)";     // Print (NULL) instead of crashing or breaking, as it is more informative to the user.
            PrecisionSet = 0; // Make sure (NULL) is printed, even when precision was set.
          }
          do {
            c = *s;
            s++;
            if (c == '\0') {
              break;
            }
            if ((PrecisionSet != 0) && (Precision == 0)) {
              break;
            }
            _StoreChar(&BufferDesc, c);
            Precision--;
          } while (BufferDesc.ReturnValue >= 0);
        }
        break;
      case 'p':
        v = va_arg(*pParamList, int);
        _PrintUnsigned(&BufferDesc, (unsigned)v, 16u, 8u, 8u, 0u);
        break;
#if SEGGER_RTT_PRINTF_FLOAT_ENABLE
      case 'f':
      case 'F':
        //
        // 取参必须用 double: 可变参数里 float 会被默认实参提升成 double,
        // 这里写 float 会读到垃圾(而且 ARM 上 double 是 8 字节、栈 8 字节对齐)。
        // 精度未显式指定时默认 6 位 —— 这点和整数不同(整数 0 表示不补零,
        // 浮点必须区分 "%.0f" 和 "%f", 所以靠 PrecisionSet 判断)。
        //
        if (PrecisionSet == 0) {
          Precision = 6u;
        }
        _PrintFloat(&BufferDesc, va_arg(*pParamList, double), Precision, FieldWidth, FormatFlags);
        break;
#endif
      case '%':
        _StoreChar(&BufferDesc, '%');
        break;
      default:
        break;
      }
      sFormat++;
    } else {
      _StoreChar(&BufferDesc, c);
    }
  } while (BufferDesc.ReturnValue >= 0);

  if (BufferDesc.ReturnValue > 0) {
    //
    // Write remaining data, if any
    //
    if (BufferDesc.Cnt != 0u) {
      SEGGER_RTT_Write(BufferIndex, acBuffer, BufferDesc.Cnt);
    }
    //
    // 这里**不能**再 `ReturnValue += Cnt`。
    // _StoreChar() 每收一个字符就已经 ReturnValue++ 了(包括还留在 acBuffer
    // 里、没触发整块 flush 的那些), 再加一次等于把尾部不足一缓冲的字节数
    // 记了两遍 —— 返回值会变成实际字节数的 2 倍。
    // 由 tools/test_rtt_printf.c 抓出来的(所有 printf 都中招, 不止 %f)。
    //
  }
  return BufferDesc.ReturnValue;
}

/*********************************************************************
*
*       SEGGER_RTT_printf
*
*  Function description
*    Stores a formatted string in SEGGER RTT control block.
*    This data is read by the host.
*
*  Parameters
*    BufferIndex  Index of "Up"-buffer to be used. (e.g. 0 for "Terminal")
*    sFormat      Pointer to format string, followed by the arguments for conversion
*
*  Return values
*    >= 0:  Number of bytes which have been stored in the "Up"-buffer.
*     < 0:  Error
*
*  Notes
*    (1) Conversion specifications have following syntax:
*          %[flags][FieldWidth][.Precision]ConversionSpecifier
*    (2) Supported flags:
*          -: Left justify within the field width
*          +: Always print sign extension for signed conversions
*          0: Pad with 0 instead of spaces. Ignored when using '-'-flag or precision
*        Supported conversion specifiers:
*          c: Print the argument as one char
*          d: Print the argument as a signed integer
*          u: Print the argument as an unsigned integer
*          x: Print the argument as an hexadecimal integer
*          s: Print the string pointed to by the argument
*          p: Print the argument as an 8-digit hexadecimal integer. (Argument shall be a pointer to void.)
*          f / F: Print the argument as a floating point number.
*                 Own implementation, no newlib needed (see _PrintFloat).
*                 Default precision is 6. Range limitation: the integral part
*                 must be < 4e9, otherwise "inf" is printed. NaN/Inf are
*                 handled. %e, %g and long double (%Lf) are NOT supported.
*
*    (3) %f is compiled in only when SEGGER_RTT_PRINTF_FLOAT_ENABLE is non-0
*        (see SEGGER_RTT_Conf.h). It costs roughly 1 KB of code and pulls in
*        the soft-float double routines from libgcc, but never malloc.
*/
int SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...) {
  int r;
  va_list ParamList;

  va_start(ParamList, sFormat);
  r = SEGGER_RTT_vprintf(BufferIndex, sFormat, &ParamList);
  va_end(ParamList);
  return r;
}
/*************************** End of file ****************************/
