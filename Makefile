# 工具链配置
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size
OPENOCD = openocd

# 目标芯片配置
TARGET = stm32f103c8t6
CPU = cortex-m3

# 编译选项
CFLAGS = -mcpu=$(CPU) -mthumb -nostartfiles -Wall -g -O0
CFLAGS += -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER
CFLAGS += -Iinclude -Istartup -Icore -Idrivers/inc -Iapp
CFLAGS += -Os
LDFLAGS = -Tlinker.ld -Wl,-Map=output/$(TARGET).map
LIBS = -lc -lm -lnosys

# 源文件 (仅编译用到的外设驱动, 省 Flash)
DRV_SRCS = drivers/src/misc.c \
           drivers/src/stm32f10x_adc.c \
           drivers/src/stm32f10x_dma.c \
           drivers/src/stm32f10x_flash.c \
           drivers/src/stm32f10x_gpio.c \
           drivers/src/stm32f10x_rcc.c \
           drivers/src/stm32f10x_tim.c \
           drivers/src/stm32f10x_usart.c

APP_SRCS = $(wildcard app/*.c)

SRCS = $(DRV_SRCS) \
       $(APP_SRCS) \
       startup/system_stm32f10x.c \
       startup/stm32f10x_it.c \
       core/core_cm3.c \
       main.c

OBJS = $(SRCS:.c=.o)
STARTUP = startup/startup_stm32f10x_md.s

# 输出目录
OUTPUT = output
ELF = $(OUTPUT)/$(TARGET).elf
BIN = $(OUTPUT)/$(TARGET).bin
HEX = $(OUTPUT)/$(TARGET).hex

# 默认目标
all: $(ELF) $(BIN) $(HEX)

# 生成 .elf 文件
$(ELF): $(OBJS) $(STARTUP)
	@mkdir -p $(OUTPUT)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(SIZE) $@

# 生成 .bin 文件
$(BIN): $(ELF)
	$(OBJCOPY) -O binary $< $@

# 生成 .hex 文件
$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

# 编译 .c 文件
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# 编译启动文件
$(STARTUP:.s=.o):
	$(CC) $(CFLAGS) -c -o $@ $<

# 清理
clean:
	rm -f $(OBJS) $(STARTUP:.s=.o) $(ELF) $(BIN) $(HEX) $(OUTPUT)/$(TARGET).map
	rm -rf $(OUTPUT) $(STARTUP:.s=.o)

# 烧录
flash: $(BIN)
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program ./output/stm32f103c8t6.hex verify reset exit"

erase:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg -c "init" -c "halt" -c "stm32f1x mass_erase 0" -c "exit"

readbin:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg -c "init" -c "halt" -c "flash read_bank 0 read_flash.bin 0 0x10000" -c "exit"

debug:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg

gdb:
	arm-none-eabi-gdb ./output/stm32f103c8t6.elf -ex "target remote localhost:3333" -ex "monitor reset halt"
