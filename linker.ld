/* 定义内存区域 */
MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 64K
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 定义堆栈大小
 *   栈从 1KB 加到 2KB:
 *     RTT 打印本身要在栈上开 SEGGER_RTT_PRINTF_BUFFER_SIZE(128B) 的临时缓冲,
 *     printf 内部还有一层开销; 后面 FOC 的浮点运算 + 中断嵌套更吃栈。
 *     1KB 在"只打个 Hello"时够, 一旦多打几个参数就可能踩到 .bss 上去,
 *     表现为日志莫名乱码 / 程序跑飞, 非常难排查, 这里一次留足。
 *   堆从 512B 加到 1KB: 目前没用 malloc, 先留着备用。 */
_stack_size = 2048; /* 2KB 栈 */
_heap_size = 1024;  /* 1KB 堆 */

/* 定义入口点 */
ENTRY(Reset_Handler)

/* 定义输出段 */
SECTIONS
{
    /* 代码段（.text） */
    .text :
    {
        /* 必须 KEEP: 开了 --gc-sections 后, 没有任何代码"引用"中断向量表,
           不 KEEP 的话它会被当成死代码整个删掉, 结果就是 Flash 开头直接是
           函数体而不是栈指针 + 复位向量, 芯片上电即跑飞。 */
        KEEP(*(.isr_vector))   /* 中断向量表, 必须排在最前面 */
        *(.text)               /* 代码 */
        *(.text*)              /* 配合 -ffunction-sections 的 .text.xxx */
        *(.rodata)             /* 只读数据 */
        *(.rodata*)            /* 配合 -fdata-sections 的 .rodata.xxx */
        *(.glue_7)             /* 用于 ARM/Thumb 交互的代码 */
        *(.glue_7t)            /* 同上 */
        *(.eh_frame)           /* 异常处理框架 */
        KEEP(*(.init))         /* 初始化代码 */
        KEEP(*(.fini))         /* 终止代码 */
        _etext = .;            /* 代码段结束, 供下面的容量断言使用 */
    } > FLASH

    /* ARM 特定段 */
    .ARM.extab :
    {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > FLASH

    .ARM :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* 初始化数据段（.data） */
    .data :
    {
        _sidata = LOADADDR(.data); /* 数据加载地址（Flash） */
        _sdata = .;                /* 数据起始地址（RAM） */
        *(.data)                   /* 数据段 */
        *(.data*)                  /* 数据段 */
        _edata = .;                /* 数据结束地址（RAM） */
    } > RAM AT > FLASH

    /* 未初始化数据段（.bss） */
    .bss :
    {
        _sbss = .; /* BSS 起始地址 */
        *(.bss)    /* BSS 段 */
        *(.bss*)   /* BSS 段 */
        *(COMMON)  /* 公共段 */
        _ebss = .; /* BSS 结束地址 */
    } > RAM

    /* 堆和栈 */
    ._user_heap_stack :
    {
        . = ALIGN(8);
        PROVIDE ( end = . );
        PROVIDE ( _end = . );
        . = . + _heap_size;
        _sstack = .;
        . = . + _stack_size;
        _estack = .;
        . = ALIGN(8);
    } > RAM

    /* 移除未使用的段 */
    /DISCARD/ :
    {
        *(.init)
        *(.fini)
    }
}

/* 容量断言: 让"超了"在链接阶段直接报错, 而不是等到运行期莫名 HardFault */
ASSERT(_etext + SIZEOF(.data) <= ORIGIN(FLASH) + LENGTH(FLASH), "ERROR: FLASH 溢出")
ASSERT(_estack <= ORIGIN(RAM) + LENGTH(RAM), "ERROR: RAM 溢出 (.bss + heap + stack 超过 20KB)")