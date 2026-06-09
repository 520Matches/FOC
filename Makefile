# 工具链配置
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size
OPENOCD = openocd

# 目标芯片配置
TARGET = stm32f103c8t6
CPU = cortex-m3

# 编译选项
CFLAGS = -mcpu=$(CPU) -mthumb -nostartfiles -Wall -g -O2
CFLAGS += -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER -Iinclude -Istartup -Icore -Idrivers/inc 
LDFLAGS = -Tlinker.ld -Wl,-Map=output/$(TARGET).map
LIBS = -lc -lm -lnosys

# 源文件和目标文件
SRCS = $(wildcard drivers/src/*.c) \
	   $(wildcard peripherals/systick/*.c) \
	   $(wildcard peripherals/timer/*.c) \
		startup/system_stm32f10x.c \
		startup/stm32f10x_it.c \
		core/core_cm3.c	\
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
	@mkdir $(OUTPUT)
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
#	windows use
#	del /f /q $(subst /,\,$(OBJS) $(ELF) $(BIN) $(HEX) $(OUTPUT)/$(TARGET).map)
#	linux use
	rm -f $(OBJS) $(STARTUP:.s=.o) $(ELF) $(BIN) $(HEX) $(OUTPUT)/$(TARGET).map
	rmdir $(OUTPUT)

# 烧录到芯片
flash: $(BIN)
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program ./output/stm32f103c8t6.hex verify reset exit"

# 调试
debug:
	$(OPENOCD) -f interface/stlink.cfg -f target/stm32f1x.cfg

gdb:
	arm-none-eabi-gdb ./output/stm32f103c8t6.elf -ex "target remote localhost:3333"
