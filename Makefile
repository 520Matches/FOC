# 工具链配置
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size
NM = arm-none-eabi-nm
OPENOCD = openocd

# 目标芯片配置
TARGET = stm32f103c8t6
CPU = cortex-m3

# ================================================================
# 调试器选择 —— 烧录 / 调试 / RTT 全部共用这一个变量
#
#   make DEBUGGER=stlink      ST-Link V2 / V2-1 / V3   (默认)
#   make DEBUGGER=cmsis-dap   CMSIS-DAP / DAPLink
#   make DEBUGGER=jlink       J-Link
#   make DEBUGGER=cmsis-dap DBG_SERIAL=E47529FB51F487BD   # 多个探针时指定
#
# 为什么不能一律写 "transport select swd":
#   ST-Link 走 OpenOCD 的 hla 驱动, transport 由 hla_layout 内部接管;
#   再显式 select 会直接报 "Debug adapter doesn't support 'swd' transport"。
#   CMSIS-DAP / J-Link 是通用适配器, 显式选 SWD 可以避免落到 JTAG。
# ================================================================
# DEBUGGER ?= stlink
DEBUGGER ?= cmsis-dap

ifeq ($(DEBUGGER),stlink)
  IFACE_CFG = interface/stlink.cfg
  NEED_SWD  =
else ifeq ($(DEBUGGER),cmsis-dap)
  IFACE_CFG = interface/cmsis-dap.cfg
  NEED_SWD  = 1
else ifeq ($(DEBUGGER),jlink)
  IFACE_CFG = interface/jlink.cfg
  NEED_SWD  = 1
else
  $(error DEBUGGER="$(DEBUGGER)" 不认识; 可选: stlink / cmsis-dap / jlink)
endif

DBG_SERIAL ?=
SERIAL_OPT = $(if $(DBG_SERIAL),-c "adapter serial $(DBG_SERIAL)",)

OPENOCD_CFG = -f $(IFACE_CFG) \
              $(if $(NEED_SWD),-c "transport select swd",) \
              $(SERIAL_OPT) \
              -f target/stm32f1x.cfg

# 传给 tools/rtt.sh 的参数 (脚本里不再重复维护一份映射, 避免两处不一致)
RTT_TOOL_ARGS = $(IFACE_CFG) $(if $(NEED_SWD),--swd,) $(if $(DBG_SERIAL),--serial $(DBG_SERIAL),)

# RTT 配置: STM32F103C8T6 的 SRAM 是 20KB (0x5000), 从 0x20000000 开始
#   OpenOCD 会在这段区间里扫描 "SEGGER RTT" 这个 16 字节 ID 来定位控制块
#   之前写的是 0x10000 (64KB), 已经超出真实 SRAM 了
RTT_ADDR   = 0x20000000
RTT_SIZE   = 0x5000
RTT_PORT   = 9090
RTT_ID     = SEGGER RTT

# RTT 通道 1 = 三相电流波形, 单独开一个 TCP 端口给 Serial Studio 用。
#   通道 0 → 9090 → make rtt 日志控制台
#   通道 1 → 9091 → Serial Studio (Quick Plot, TCP 客户端)
# 两个 server 各自独立, OpenOCD 的对应实现是每调一次
# "rtt server start <port> <channel>" 就 add_service 一次, 互不干扰。
RTT_WAVE_PORT = 9091
RTT_WAVE_CH   = 1

# 默认按 ID 扫描整段 SRAM 找控制块。
# RTT_ADDR_USE / RTT_SIZE_USE 的实际取值在下面 ELF 定义之后再定 —— 因为
# "RTT_CB=1" 模式要用 nm 读 ELF 符号, 而 $(shell) 是在解析到这一行时就展开的,
# 那时 ELF 变量还没赋值, 会拿到空文件名。这是踩过的坑, 别把那段挪回来。
RTT_ADDR_USE = $(RTT_ADDR)
RTT_SIZE_USE = $(RTT_SIZE)

# RTT_SETUP 里的 \" 不能省: 它会被拼进 recipe 的 -c "..." 里,
# 少了转义, shell 会把内层引号吃掉, OpenOCD 收到的是 3 个独立单词
# ("SEGGER" "RTT"), TCL 就报 wrong # args。
RTT_SETUP = rtt setup $(RTT_ADDR_USE) $(RTT_SIZE_USE) \"$(RTT_ID)\"

# 串口波形波特率 (USART2 @ APB1 36MHz, 上限约 2.25Mbps)
#   921600 最通用; 2000000 时 BRR=0x12 零误差, 但要求 USB-TTL 支持
SCOPE_BAUD = 921600

# 编译选项
#   注意: 原来同时写了 -O0 和 -Os, 后面的覆盖前面的, 实际生效的是 -Os。
#   学习/调试阶段用 -Og: 保留调试信息又不至于让代码膨胀太多。
CFLAGS = -mcpu=$(CPU) -mthumb -nostartfiles -Wall -g2 -Og
CFLAGS += -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER
CFLAGS += -ffunction-sections -fdata-sections
CFLAGS += -Iinclude -Istartup -Icore -Idrivers/inc -Iapp -Irtt

# 追加开关的**唯一正确入口**:  make EXTRA_CFLAGS=-DXXX
#
#   ⚠️ 不要写 `make CFLAGS+=-DXXX`: 命令行上给 CFLAGS 赋值会让 makefile 里
#      所有 CFLAGS 赋值**全部失效**(GNU make 的规则: 命令行变量优先, 除非
#      加 override)。结果是 -mcpu/-mthumb/-Og/-I... 全丢, 编出来的 .o 是
#      默认架构(armv4t 的 ARM 模式, 不是 Thumb), 体积和性能都对不上, 还会
#      给出"改了开关反而更小"这种看不懂的数据。(本轮踩过)
EXTRA_CFLAGS ?=
CFLAGS += $(EXTRA_CFLAGS)

# 链接选项
#   --gc-sections   : 配合上面的 -ffunction-sections, 丢掉没用到的函数
#   --print-memory-usage : 直接打印 FLASH / RAM 占用, 省得每次去算
LDFLAGS = -Tlinker.ld -Wl,-Map=$(MAP)
LDFLAGS += -Wl,--gc-sections -Wl,--print-memory-usage
LIBS = --specs=nano.specs -lc -lm -lnosys

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

RTT_SRCS = $(wildcard rtt/*.c)

SRCS = $(DRV_SRCS) \
       $(APP_SRCS) \
       $(RTT_SRCS) \
       startup/system_stm32f10x.c \
       startup/stm32f10x_it.c \
       core/core_cm3.c \
       main.c

OBJS = $(SRCS:.c=.o)
STARTUP = startup/startup_stm32f10x_md.s

# 输出目录
#   MAP 被 LDFLAGS 的 -Wl,-Map= 引用, 所以链接产物路径只有这一处定义。
MAP  = $(OUTPUT)/$(TARGET).map
OUTPUT = output
ELF = $(OUTPUT)/$(TARGET).elf
BIN = $(OUTPUT)/$(TARGET).bin
HEX = $(OUTPUT)/$(TARGET).hex
IMON_TEST_BIN = $(OUTPUT)/imon_fmt_test
WAVE_TEST_BIN = $(OUTPUT)/wave_fmt_test
RTT_FMT_TEST_BIN = $(OUTPUT)/rtt_printf_test

# ================================================================
# RTT 控制块地址 —— 从**编译产物**里取 `_SEGGER_RTT` 的地址
#
#   默认(不设 RTT_CB): `rtt setup 0x20000000 0x5000 "SEGGER RTT"` —— 在整段
#     20KB SRAM 里找控制块。
#   RTT_CB=1 / map / nm: 把 `_SEGGER_RTT` 的确切地址喂给 OpenOCD。
#
#   ⚠️ 先搞清 OpenOCD 的语义, 别想当然: `rtt setup <addr> <size> <id>`
#      **永远是"在 [addr, addr+size) 这个窗口里搜索 ID"**, 没有"直接用这个
#      地址"的模式。0.12.0 源码 (src/target/rtt.c):
#          LOG_INFO("rtt: Searching for control block '%s'", id);
#          for (addr = *address; addr < address_end; addr += sizeof(buf)) { ... }
#      窗口内还要逐字节滑窗比对 ID。
#      所以 RTT_CB 的收益**不是"不扫描"**, 而是:
#        · 窗口从 20KB(0x5000) 缩到 1KB(0x400)。控制块正好在窗口首址,
#          所以第一块 1KB 就命中 —— 原来要读 20 个 1KB 块, 启动明显更快;
#        · 窗口越小, "在别处撞上同样字节序列"的概率越低。
#
#   来源二选一:
#     map  —— 从 $(MAP) 里抓。**纯文本, 不依赖工具链在不在 PATH**, 最稳。
#     nm   —— 从 ELF 符号表抓 (arm-none-eabi-nm)。
#   取值:
#     RTT_CB=1     先试 map, 读不到再退回 nm (推荐)
#     RTT_CB=map   强制只用 map      (排查用)
#     RTT_CB=nm    强制只用 ELF 符号表 (排查用)
#   看一眼当前取到什么:  make rtt-addr
#
#   ⚠️ 两个必须知道的前提:
#     1) 地址对应的是**这份产物**。改了代码没重新 flash, 板子上的控制块
#        就不在这个地址 —— 窗口里搜不到 ID 时 OpenOCD 只会打一行提示,
#        很容易被忽略, 表现是"连上了但一行日志都没有"。所以顺序永远是:
#        make flash → 再 make rtt RTT_CB=1。
#     2) 这必须放在 ELF/MAP 定义之后 —— $(shell) 在解析到这一行时就展开,
#        那时变量还没赋值会拿到空文件名 (踩过, 别把这段挪到前面去)。
#
#   map 里的符号定义行长这样 (GNU ld):
#        .bss._SEGGER_RTT
#                       0x20002940       0x78 rtt/SEGGER_RTT.o
#                       0x20002940                _SEGGER_RTT   <-- 取这行的 $1
#   匹配靠 "$2 == _SEGGER_RTT"(字段 2 恰好是符号名)。
#   ⚠️ 别写成 /^[[:space:]]+_SEGGER_RTT$/: 行首空白后面紧跟的是**地址**,
#      不是符号名, 那样写永远匹配不到 (踩过这个坑)。
# ================================================================
MAP_RTT_ADDR = $(shell awk '$$2=="_SEGGER_RTT"{print $$1; exit}' $(MAP) 2>/dev/null)
NM_RTT_ADDR  = $(shell $(NM) $(ELF) 2>/dev/null | awk '$$3=="_SEGGER_RTT"{print "0x"$$1}')

RTT_CB_ADDR =
RTT_CB_SRC  =
ifeq ($(RTT_CB),map)
  RTT_CB_ADDR = $(MAP_RTT_ADDR)
  RTT_CB_SRC  = map
else ifeq ($(RTT_CB),nm)
  RTT_CB_ADDR = $(NM_RTT_ADDR)
  RTT_CB_SRC  = ELF(nm)
else ifeq ($(RTT_CB),1)
  # 自动: map 优先 (不依赖工具链), 读不到再退回 nm
  ifneq ($(MAP_RTT_ADDR),)
    RTT_CB_ADDR = $(MAP_RTT_ADDR)
    RTT_CB_SRC  = map
  else
    RTT_CB_ADDR = $(NM_RTT_ADDR)
    RTT_CB_SRC  = ELF(nm)
  endif
endif

ifneq ($(strip $(RTT_CB_ADDR)),)
  RTT_ADDR_USE = $(RTT_CB_ADDR)
  # 0x400 = OpenOCD 的**搜索窗口**长度(控制块本身只有 0x78 字节),
  # 不是"控制块大小"。窗口首址就是控制块, 所以第一块就读到了。
  RTT_SIZE_USE = 0x400
else ifneq ($(filter 1 map nm,$(RTT_CB)),)
  $(warning RTT_CB=$(RTT_CB) 但产物里读不到 _SEGGER_RTT (map=$(MAP)); 回退到扫描模式。先 make 一次)
endif

# 默认目标
all: $(ELF) $(BIN) $(HEX)

# ================================================================
# 串口波形 (VOFA+ JustFloat, 921600)
#   依赖装在独立虚拟环境里: make deps   (或手动 pip install ...)
#   为什么不用系统 python: macOS 上 Homebrew 的 python 是 PEP 668
#   "externally-managed" 环境, 直接 pip install 会被拒绝。
#   也可以直接用 VOFA+ 上位机, 协议选 JustFloat, 波特率 921600。
# ================================================================
PY_VENV ?= $(HOME)/.workbuddy/binaries/python/envs/default
PY      ?= $(shell test -x $(PY_VENV)/bin/python && echo $(PY_VENV)/bin/python || echo python3)

deps:
	@echo ">>> 创建虚拟环境: $(PY_VENV)"
	@test -x $(PY_VENV)/bin/python || python3 -m venv $(PY_VENV)
	$(PY_VENV)/bin/pip install -q --upgrade pip
	$(PY_VENV)/bin/pip install pyserial pyqtgraph PyQt5
	@echo ">>> 装好了, 用 $(PY_VENV)/bin/python 跑脚本"

plot:
	@$(PY) tools/scope_plot.py --baud $(SCOPE_BAUD) --names IU,IV,angle,duty

# ================================================================
# 电流文字输出 (RTT)
#   imon      : 实时画电流曲线, 数据来自 RTT 的 IMON 行 (不是串口)
#               OpenOCD 没在跑的话会自己起一个, 所以单独一条命令就能用;
#               已经在跑 make rtt 的话就直接接上去 (两者可以同时开)
#   fmt-test  : 主机侧单测 —— app/imon.c 刻意不依赖任何 STM32 头文件,
#               所以能用系统 cc 直接在电脑上编译运行, 不需要硬件。
#               验证的是排版规则和 raw→mA 换算, 也就是最容易写错的部分。
#   imon-test : 上位机解析器测试 + 两端契约测试。
#               会运行 imon_fmt_test --dump, 把 C 端真正写进 RTT 的字节
#               喂给 tools/imon_plot.py 的解析器 —— 防止"固件打 TABLE、
#               上位机只认 CSV"这类两端各写各的静默失配再次发生。
# ================================================================
imon:
	-@RTT_PORT=$(RTT_PORT) RTT_ADDR=$(RTT_ADDR_USE) RTT_SIZE=$(RTT_SIZE_USE) \
	 RTT_ID="$(RTT_ID)" PY="$(PY)" RTT_ATTACH=1 \
	 RTT_WAVE_PORT=$(RTT_WAVE_PORT) RTT_WAVE_CH=$(RTT_WAVE_CH) \
	 RTT_CLIENT="$(PY) $(CURDIR)/tools/imon_plot.py --port $(RTT_PORT)" \
	 bash tools/rtt.sh $(RTT_TOOL_ARGS)

$(IMON_TEST_BIN): tools/test_imon_fmt.c app/imon.c app/imon.h
	@mkdir -p $(OUTPUT)
	cc -Wall -Wextra -O1 -Iapp -o $@ tools/test_imon_fmt.c app/imon.c

fmt-test: $(IMON_TEST_BIN)
	@$(IMON_TEST_BIN)

imon-test: $(IMON_TEST_BIN)
	@$(PY) tools/test_imon_plot.py --bin $(IMON_TEST_BIN)

# ================================================================
# RTT 波形 (给 Serial Studio)
#   app/wave_fmt.c 跟 imon.c 一样刻意不依赖任何 STM32 头文件, 所以能在
#   电脑上直接用系统 cc 跑单测。这里除了逐字节核对输出, 还会用
#   Serial Studio Quick Plot 的拆分规则(只按逗号拆、按行分帧)
#   复检一遍 —— 确保固件吐出去的每一行, 上位机都真的能画成 3 条曲线。
# ================================================================
$(WAVE_TEST_BIN): tools/test_wave_fmt.c app/wave_fmt.c app/wave_fmt.h
	@mkdir -p $(OUTPUT)
	cc -Wall -Wextra -O1 -Iapp -o $@ tools/test_wave_fmt.c app/wave_fmt.c

# wave-test   : C 端排版单测 + 契约测试(C 端真实字节 → Python 的 Quick Plot 解析器)
# wave-live   : 对正在跑的 9091 端口做实时体检(列数/非法行/帧率/各列范围)
wave-test: $(WAVE_TEST_BIN)
	@$(WAVE_TEST_BIN)
	@echo ""
	@$(PY) tools/ss_wave_check.py --bin $(WAVE_TEST_BIN) --expect-cols 3

wave-live:
	@$(PY) tools/ss_wave_check.py --host 127.0.0.1 --port $(RTT_WAVE_PORT) --expect-cols 3

# ================================================================
# SEGGER_RTT_printf 浮点 (%) 单测 —— 主机侧, 不需要硬件
#
#   本工程的 %f 是**自己在 SEGGER_RTT_printf.c 里实现**的(不走 newlib),
#   所以必须有自己的回归测试:
#     · 大小: rtt/SEGGER_RTT_printf.c 只依赖 SEGGER_RTT_Write(), 给个假实现
#             就能在电脑上跑, 把固件真正要发出去的字节抓下来逐字节比对;
#     · 期望值全部手写死, 不用宿主机的 vsnprintf 当基准 —— 宿主的 %f 是
#       banker's rounding, 语义和我们的不一样, 拿它做参考只会误判。
# ================================================================
$(RTT_FMT_TEST_BIN): tools/test_rtt_printf.c rtt/SEGGER_RTT_printf.c \
                     rtt/SEGGER_RTT.h rtt/SEGGER_RTT_Conf.h rtt/SEGGER_RTT_ConfDefaults.h
	@mkdir -p $(OUTPUT)
	cc -Wall -Wextra -O1 -Irtt -o $@ tools/test_rtt_printf.c rtt/SEGGER_RTT_printf.c

rtt-fmt-test: $(RTT_FMT_TEST_BIN)
	@$(RTT_FMT_TEST_BIN)

.PHONY: all clean flash erase readbin probe diag reset stop rtt-addr debug log rtt gdb plot deps imon fmt-test imon-test wave-test wave-live rtt-fmt-test

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

# ================================================================
# 探针互斥 —— 为什么需要这个
#
#   DAPLink / ST-Link 这类 USB 调试器是**独占设备**: 同一时刻只能被一个
#   进程打开。如果上一次 `make rtt` / `make debug` 的 OpenOCD 没退干净
#   (最常见的是直接叉掉终端窗口, 而不是 Ctrl-C), 它还在后台攥着探针,
#   新的 openocd 连 USB 句柄都拿不到, 于是报出非常误导人的错误:
#
#       Error: CMSIS-DAP command CMD_INFO failed.
#       Error: error reading data: Entity not found
#       Error: error writing data: Entity not found
#
#   看起来像"固件把板子写坏了 / 你的程序有问题", 其实跟代码**毫无关系**,
#   只是探针被占着。这里在真正碰硬件之前先把残留 OpenOCD 清掉,
#   让 make flash / probe / reset 永远一把过。
#
#   仍然建议正常用 Ctrl-C 退出 make rtt; 实在忘了也没关系, 这里兜底。
#   手动清理:  make stop
# ================================================================
PROBE_PAT = openocd .*$(IFACE_CFG)

# 探针是否被占: (a) 有匹配本工程配置的 openocd 进程, 或 (b) RTT 端口在监听
define PROBE_BUSY_TEST
pgrep -f "$(PROBE_PAT)" >/dev/null 2>&1 || lsof -nP -iTCP:$(RTT_PORT) -sTCP:LISTEN >/dev/null 2>&1
endef

define PROBE_FREE
	@if $(PROBE_BUSY_TEST); then \
	  echo ">>> 检测到残留 OpenOCD 仍占用调试探针, 自动释放 (想手动清: make stop)"; \
	  pkill -f "$(PROBE_PAT)" 2>/dev/null || true; \
	  pids=`lsof -nP -iTCP:$(RTT_PORT) -sTCP:LISTEN -t 2>/dev/null`; \
	  [ -n "$$pids" ] && kill $$pids 2>/dev/null; \
	  i=0; \
	  while [ $$i -lt 5 ]; do \
	    if ! ( $(PROBE_BUSY_TEST) ); then break; fi; \
	    sleep 1; i=$$((i+1)); \
	  done; \
	  echo ">>> 探针已释放"; \
	fi
endef

# 手动停止残留 OpenOCD (释放探针)
stop:
	@pkill -f "$(PROBE_PAT)" 2>/dev/null && echo ">>> 已停止 OpenOCD, 探针已释放" \
	  || echo ">>> 没有正在运行的 OpenOCD"

# 烧录 (依赖 .hex 就烧 .hex, 别依赖 .bin 却烧 .hex)
flash: $(HEX)
	$(PROBE_FREE)
	$(OPENOCD) $(OPENOCD_CFG) -c "init" -c "program $(HEX) verify reset" -c "exit"

erase:
	$(PROBE_FREE)
	$(OPENOCD) $(OPENOCD_CFG) -c "init" -c "halt" -c "stm32f1x mass_erase 0" -c "exit"

readbin:
	$(PROBE_FREE)
	$(OPENOCD) $(OPENOCD_CFG) -c "init" -c "halt" -c "flash read_bank 0 read_flash.bin 0 0x10000" -c "exit"

# ================================================================
# 调试探针自检
#   probe : 打印当前选的调试器、能否连上芯片、Flash 大小
#           换调试器后第一个该跑的命令 —— 报错信息能直接定位问题
#   diag  : probe 的加强版。逐级降速(1MHz→100kHz→100kHz+复位连接)重试,
#           全失败时直接打印这块板子的接线表 + 供电排查清单。
#           "cannot read IDR" 是 SWD 的万能错误码, 光看它猜不出原因,
#           而这块板子恰好两个坑都容易踩: 板载没有 5V 来源,
#           且调试口 H8 不是标准 ARM 10 针定义。
#   reset : 复位并全速运行
# ================================================================
probe:
	$(PROBE_FREE)
	@echo ">>> DEBUGGER=$(DEBUGGER)   IFACE=$(IFACE_CFG)"
	@echo ">>> 命令: $(OPENOCD) $(OPENOCD_CFG) -c init"
	$(OPENOCD) $(OPENOCD_CFG) -c "init" -c "targets" -c "flash probe 0" -c "exit"

diag:
	$(PROBE_FREE)
	@bash tools/probe.sh $(RTT_TOOL_ARGS)

reset:
	$(PROBE_FREE)
	$(OPENOCD) $(OPENOCD_CFG) -c "init" -c "reset run" -c "exit"

# ================================================================
# RTT 调试 (日志打印走这条通道)
#
#   RTT 的原理: 目标板在内存里放一个控制块 + 两个环形缓冲(上行/下行),
#   调试探针在 CPU 全速运行时用 SWD 直接读这块内存 —— 不占用任何 UART 引脚,
#   也不需要停下来, 所以它跟串口波形可以同时用, 互不干扰。
#   和 SPY 一样是"读内存", 所以 ST-Link / CMSIS-DAP / J-Link 都支持。
#
#   make debug   : 只起 OpenOCD + RTT server (占一个终端), 适合配合 make log
#   make rtt     : 一条命令搞定 —— 起 OpenOCD + 直接进 RTT 控制台, Ctrl-C 全退出
#   make log     : 配合 make debug 用的裸连接 (nc)
#
#   原命令有两个致命问题(已修):
#     1) 没有 -c "init"      -> target 没被 examine, rtt 命令不生效
#     2) 没有 -c "rtt start" -> 只起了 TCP server, 没有任何后台轮询去
#                              扫描 SRAM 里的 RTT 控制块, 所以永远连不上
#
#   注意 RTT_SETUP 定义在文件上面 (RTT 配置那一段), 别在这里重复定义 ——
#   重复定义时后一个会静默覆盖前一个, 而且很容易丢掉引号转义, 很难查。
# ================================================================
# 看一眼从编译产物里取到的 RTT 控制块地址 (map 和 nm 两个来源都列出来对照)
#   排查"RTT_CB=1 到底有没有生效 / 地址是不是这份固件的"用这个。
rtt-addr:
	@echo ">>> _SEGGER_RTT 地址来源对照"
	@printf "    map %s\n        -> %s\n" "$(MAP)" "$(if $(MAP_RTT_ADDR),$(MAP_RTT_ADDR),✗ 没读到)"
	@printf "    nm  %s\n        -> %s\n" "$(ELF)" "$(if $(NM_RTT_ADDR),$(NM_RTT_ADDR),✗ 没读到)"
	@if [ -n "$(strip $(RTT_CB_ADDR))" ]; then \
	  echo ">>> RTT_CB=$(RTT_CB) 生效: $(RTT_CB_ADDR)  [来源: $(RTT_CB_SRC)], 搜索窗口缩到 $(RTT_SIZE_USE)"; \
	else \
	  echo ">>> 未启用 RTT_CB → 用扫描模式 $(RTT_ADDR) + $(RTT_SIZE)"; \
	  echo "    改用编译产物里的精确地址: make rtt RTT_CB=1"; \
	fi

debug:
	@if [ -n "$(strip $(RTT_CB_ADDR))" ]; then \
	  echo ">>> RTT 控制块地址 $(RTT_ADDR_USE)  (RTT_CB=$(RTT_CB) 来源 $(RTT_CB_SRC); 搜索窗口 $(RTT_SIZE_USE), 不是 $(RTT_SIZE) 盲扫)"; \
	else \
	  echo ">>> RTT 扫描窗口 $(RTT_ADDR_USE) + $(RTT_SIZE_USE)  (RTT_CB=1 可用 map 里的精确地址把窗口缩到 1KB)"; \
	fi
	@echo ">>> RTT 端口: $(RTT_PORT)=日志(ch0)  $(RTT_WAVE_PORT)=波形(ch$(RTT_WAVE_CH))"
	$(OPENOCD) $(OPENOCD_CFG) \
		-c "init" \
		-c "$(RTT_SETUP)" \
		-c "rtt polling_interval 10" \
		-c "rtt start" \
		-c "rtt server start $(RTT_PORT) 0" \
		-c "rtt server start $(RTT_WAVE_PORT) $(RTT_WAVE_CH)" \
		-c "reset run"

# 一条命令: 起 OpenOCD + 自动连上 RTT 控制台; Ctrl-C 会把 OpenOCD 一起收掉
#   开头那个 '-' 是给 make 的: 脚本 Ctrl-C 退出码是 130, 不加 '-' 会打印
#   "make: *** [rtt] Error 130", 看着像出错, 其实是你自己按的 Ctrl-C。
rtt: $(ELF)
	-@RTT_PORT=$(RTT_PORT) RTT_ADDR=$(RTT_ADDR_USE) RTT_SIZE=$(RTT_SIZE_USE) \
	 RTT_ID="$(RTT_ID)" PY="$(PY)" RTT_SRC="$(RTT_CB_SRC)" \
	 RTT_WAVE_PORT=$(RTT_WAVE_PORT) RTT_WAVE_CH=$(RTT_WAVE_CH) \
	 bash tools/rtt.sh $(RTT_TOOL_ARGS)

# 起完 make debug 后, 另开一个终端执行这个看日志
log:
	@echo ">>> connect to RTT 127.0.0.1:$(RTT_PORT)  (Ctrl-C 退出)"
	nc 127.0.0.1 $(RTT_PORT)

gdb:
	arm-none-eabi-gdb ./output/stm32f103c8t6.elf -ex "target remote localhost:3333" -ex "monitor reset halt"
