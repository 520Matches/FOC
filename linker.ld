/* 定义内存区域 */
MEMORY
{
    FLASH (rx)      : ORIGIN = 0x08000000, LENGTH = 64K
    RAM (xrw)       : ORIGIN = 0x20000000, LENGTH = 20K
}

/* 定义堆栈大小 */
_stack_size = 1024; /* 1KB 堆栈 */
_heap_size = 512;   /* 512B 堆 */

/* 定义入口点 */
ENTRY(Reset_Handler)

/* 定义输出段 */
SECTIONS
{
    /* 代码段（.text） */
    .text :
    {
        *(.isr_vector)    /* 中断向量表 */
        *(.text)          /* 代码 */
        *(.rodata)        /* 只读数据 */
        *(.glue_7)        /* 用于 ARM/Thumb 交互的代码 */
        *(.glue_7t)       /* 同上 */
        *(.eh_frame)      /* 异常处理框架 */
        KEEP(*(.init))    /* 初始化代码 */
        KEEP(*(.fini))    /* 终止代码 */
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