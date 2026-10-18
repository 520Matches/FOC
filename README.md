# STM32F103 FOC 学习工程

纯 Makefile + OpenOCD 的裸机工程，用于学习 FOC（磁场定向控制）。
外设：DRV8313（三相栅极驱动）+ MT6701（磁编码器）+ INA199（电流采样）。

## 三条通道，各管一件事

| 通道 | 引脚 | 用途 | 为什么这么分 |
|---|---|---|---|
| **SWD / RTT ch0** | SWDIO、SWCLK | 日志 + 命令输入 + 电流数值 | 调试探针直接读目标板内存，**不占任何 UART 引脚**，CPU 全速跑也不受影响 |
| **SWD / RTT ch1** | SWDIO、SWCLK | 三相电流波形（`IU,IV,IW`） | 单开一个 RTT 通道给 **Serial Studio** 画曲线 —— 和 ch0 的日志互不污染 |
| **UART（波形）** | PA2(TX) / PA3(RX) | 二进制波形流 | 走 VOFA+ JustFloat + DMA，带宽独立 |
| **PWM / ADC** | PA8/9/10、PA0/1、PA4/5/6 | 电机驱动 / 采样 / 编码器 | |

关键点：**RTT 和串口互不干扰**。日志一律走 RTT，串口只发二进制帧。
如果把 ASCII 日志混进串口，JustFloat 帧会被整个搞乱 —— 所以 `_write` 被重定向到了 RTT。

RTT 内部又分成两条互不干扰的流，对应两个 TCP 端口：

```
RTT ch0 ──> OpenOCD :9090 ──> make rtt / rtt_log.py   (日志 + 命令)
RTT ch1 ──> OpenOCD :9091 ──> Serial Studio           (三相电流波形)
```

## 快速开始

```bash
make                # 编译
make flash          # 烧录（默认 ST-Link）
make rtt            # 起 OpenOCD + 进 RTT 控制台（Ctrl-C 一起退出）
make imon           # 另开一个终端：实时画电流曲线（会自动接上 make rtt）
make wave-live      # 体检 RTT 波形流：帧率 / 列数 / 有没有非法行
make rtt-addr       # 看从编译产物（map / ELF）里取到的 RTT 控制块地址
make stop           # 释放被残留 OpenOCD 占用的调试探针
```

**想在 Serial Studio 里看三相电流波形**：让 `make rtt`（或 `make debug`）跑着，
然后在 Serial Studio 里连 **Network / TCP 客户端 → `127.0.0.1:9091`、模式 Quick Plot**。
详见「Serial Studio 波形」一节。

`make rtt` 起来后直接敲字符就是命令（**要按回车**，终端是行输入的）：

| 键 | 作用 |
|---|---|
| `m` / `n` | 电机启动 / 停止 |
| `+` / `-` | 占空比 ±10% |
| `s` | 暂停 / 恢复串口波形流 |
| `f` | 切换采样率 1k / 2k / 5k / 10k / 20k |
| `c` | 单次触发捕获（看换相细节，不受串口带宽限制） |
| `l` | USART2 回环自检（验证接线 / 电平） |
| `i` | 电流监视（IMON）开 / 关（上电默认开） |
| `v` | 切换 IMON 输出格式 `TABLE` / `CSV` |
| `1`~`5` | IMON 打印周期 1000 / 500 / 200 / 100 / 20 ms |
| `w` | **RTT ch1 波形开关**（Serial Studio 用，上电默认开） |
| `[` / `]` | RTT ch1 波形速率 更慢 / 更快（1000 / 500 / 200 / 100 / 50 Hz） |
| `?` | 打印状态（采样率、帧数、丢帧、**电机开关 / 占空比 / 过流次数**、IMON、RTT 波形） |

**上电即输出波形**：1 kHz、4 通道、电机保持 OFF，接上 USB-TTL 就能看到
ch0=IU、ch1=IV、ch2=电角度、ch3=占空比。

**上电也同时输出电流和转子角度**：RTT 终端里每 100 ms 一行，不用接串口 ——
见下一节。

## 电流数据 + 转子角度（RTT 文本输出）

不用接串口、不用开上位机，`make rtt` 起来就能直接看到：

```
[I] IMON 1kHz 采样, 每 100ms 一行
    t_ms | U/V: 瞬时/峰值/均值(mA)+raw | duty | ang(0.1°) mag
    零电流 raw≈2030; 电机转没转看 ang 在不在变
IMON t=1234ms | U    +12mA pk  +140 av    +8 raw2048 | V ... | duty 80 | ang 187.5 mag 7
```

- **上电默认就开**，不需要敲任何命令
- 采样固定 **1 kHz**（按 SysTick 的 1 ms 节拍），而不是拿主循环次数当采样率 ——
  主循环空闲时一秒能转几十万圈，用它当时钟会读到大量重复值、均值还会随负载漂
- `pk` 是窗口内**绝对值最大的那个点**：没有示波器时，单个瞬时值看不出"这一相
  到底冲到了多少"，用峰值检波来补
- `raw` 是 ADC 原始值：**零电流时实测在 2030 附近**（不是理论上的 2048，
  偏置约 14 mA），电机转起来会跟着抖
- `ang` 是**转子绝对角度**（0~359.9°），`mag` 是 MT6701 的磁场强度

### 电机转没转，看 `ang`

这一列是专门为"没有示波器"加的。**电流变大只说明有电流，不代表在转**（堵转、
换相不对一样吃电流）。`ang` 一直在变才是转子真的在动的证据：

```
敲 'm' 前              ang 恒定不动
敲 'm' 后, 占空比 5.6%  ang 缓慢单调变化   ≈ 12 RPM
按 '+' 到 25%          ang 变化明显加快     ≈ 38 RPM
```

### 命令

| 键 | 作用 |
|---|---|
| `i` | IMON 开 / 关 |
| `v` | 切换输出格式 `TABLE`（终端看）/ `CSV`（落盘画图） |
| `1`~`5` | 打印周期 1000 / 500 / 200 / 100 / 20 ms |
| `m` / `n` | 电机启动 / 停止 |
| `+` / `-` | 占空比 ±10%（1440 的 10%） |
| `?` | 状态：含 `motor=ON/OFF`、占空比百分比、**`oc_trips` 过流停机次数** |

### 想看曲线

```bash
make imon       # 一条命令就行: 没有 OpenOCD 会自己起一个
```

想同时看日志就再开一个终端 `make rtt` —— **谁先起谁负责起 OpenOCD，后起的
自动接上去**，Ctrl-C 只关掉自己、不会误杀对方的 OpenOCD。

图上会画 IU / IV 和两相峰值；如果固件带角度输出，右侧还有一根独立的 Y 轴画
转子角度（电流是 ±1650 mA 量级、角度是 0~360°，共用一根轴会把电流压成直线）。

想事后分析就先把日志落盘：

```bash
tools/rtt_log.py --log output/rtt.log     # 在终端 1 里用它替代 make rtt
python3 tools/imon_plot.py --file output/rtt.log
```

CSV 列序（`v` 切到 CSV 格式时，按 `app/imon.c` 的 `imon_fmt_csv` 输出）：

```
IMON, t_ms, rawU, mAU, pkU, avU, rawV, mAV, pkV, avV, duty, ang10, mag
```

> 末两列 `ang10` / `mag` 是后加的，**上位机脚本对旧格式向后兼容** ——
> 板子上还是旧固件时照常出电流曲线，只是没有角度线，并会明确提示你去
> `make flash`。解析器不认的格式会给出可操作的提示，不会只留一个空白窗口。

### 排版逻辑可以在电脑上单测

`app/imon.c` 只依赖 `<stdio.h>`，**不含任何 STM32 头文件**，所以能用系统编译器
直接跑，不用等芯片、不用接板子：

```bash
make fmt-test    # C 侧: 列宽对齐 / 正负号 / CSV 列序 / 截断保护 / raw→mA 全量程对比
make imon-test   # 上位机解析器 + 两端契约测试
```

"最容易写错的排版"这样就被隔离掉了；固件侧（读 ADC、发 RTT）才需要硬件。

**`make imon-test` 里的契约测试是重点。** 它运行 `output/imon_fmt_test --dump`
（打印的是固件**真正会写进 RTT 的字节**），再把这些字节喂给
`tools/imon_plot.py` 的解析器，断言两边对同一行数据的理解完全一致。

这么做的原因很具体：这个项目踩过的坑就是**两端各写各的** —— 固件默认打
TABLE 格式，而绘图脚本只认 CSV，结果一行都解析不出来，窗口一片空白且毫无
提示，各自的测试却都是绿的。契约测试把两端钉在一起，任何一边改了格式这里
立刻红。

## Serial Studio 波形（RTT 通道 1）

用 **Serial Studio** 把三相电流画成实时曲线。数据不走串口，走 **RTT 通道 1** ——
所以不用接 UART、不占引脚、和 `make rtt` 的日志也互不干扰。

### 它是怎么接上的

```
STM32  RTT ch1: "12,-8,-4\n"  ──SWD──>  OpenOCD  rtt server :9091  ──TCP──>  Serial Studio
                                                                            (Network / TCP 客户端)
                                                                            (模式: Quick Plot)
```

Serial Studio 的 Network 数据源**只支持作为 TCP 客户端往外连**，正好可以直接连
OpenOCD 起的 RTT server，中间不需要任何桥接脚本。

### 为什么单独开一个 RTT 通道

通道 0 上跑的是日志，里面混着 `[I] ...` 这种带逗号的文字。Serial Studio 的
Quick Plot 是"每行按逗号拆、每列画一条线"——日志行混进来会变成一堆乱跳的曲线，
而且**没法靠配置过滤**。所以波形单独走通道 1：

| 端口 | RTT 通道 | 内容 | 谁在用 |
|---|---|---|---|
| 9090 | ch0 | 日志 / 命令 | `make rtt`、`rtt_log.py`、`make imon` |
| 9091 | ch1 | `IU,IV,IW` 三相电流 | Serial Studio |

两个 RTT server 是各自独立的（OpenOCD 里每调一次 `rtt server start` 就是一条
独立服务），可以同时连、互不影响。

### 固件侧做了什么

- 每 **1 kHz**（跟 SysTick 的 1 ms 节拍）采一次 U/V 两相，按档位抽点后输出
- 输出格式就一行纯数字，单位 **mA**：

  ```
  IU,IV,IW
  -14,-16,30
  -15,-16,31
  ```

  第一行是全非数字 → Quick Plot 会把它当**通道名**；后面每行 3 列画 3 条曲线。
  单位用 mA 的整数是因为 `SEGGER_RTT_printf` 不支持 `%f`，而 1 mA 分辨率
  对 ±1.65 A 量程刚好匹配 12 bit ADC，不浪费精度。
- **W 相是推出来的**：板子上只有两路 INA199（只测 U/V 两相），
  所以 `iw = -(iu + iv)`（基尔霍夫电流定律）。它是派生量 ——
  三相平衡时看起来就是第三条正弦，不平衡时误差会全部体现在它身上。
- 通道 1 的 512 B 上行缓冲由 `app/rtt_wave.c` 自己出，用官方 API
  `SEGGER_RTT_ConfigUpBuffer()` 挂上去，**没有改 SEGGER 的源码**。
- 没人读 9091 时缓冲会写满，写入自动跳过 —— **不会阻塞主循环**，
  也不会浪费 SWD 带宽（OpenOCD 在没有客户端时根本不读这个通道）。

### Serial Studio 里的设置（三步）

1. 先让 OpenOCD 跑起来：`make rtt`（或 `make debug`）。
2. Serial Studio → 数据源选 **Network**，**Socket type = TCP**，
   *Remote address* = `127.0.0.1`，*Port* = `9091` → **Connect**。
3. 右侧 **Setup 面板**里选 **"Quick Plot (Comma Separated Values)"**。

然后就会自动生成一个 Data Grid + 一个三通道叠加的 MultiPlot + 每个通道的独立图。

### 想让通道名显示成 IU / IV / IW

Quick Plot 有个**很关键但容易踩的规则**：它只在**连接之后收到的第一行**上做标签
检测，**一旦碰到纯数字帧就再也不找标签了**。所以顺序必须是"先清空、再连、再开流"：

```
1) 在 make rtt 终端敲 w   → 关掉波形流 (同时把缓冲里冻结的旧数据清掉)
2) Serial Studio 里点 Connect   → 此时收不到任何数据, 标签检测保持待命
3) 再敲一次 w            → 缓冲是空的, 于是第一行就是 "IU,IV,IW" → 通道名正确
```

反过来"先连上、再开流"也能看到曲线，只是通道名会是默认的 Channel 1/2/3
（上电默认就是开着的，所以直接连就会是这种情况）。
**通道名只是显示，不影响曲线本身。**

### 体检 / 排错

```bash
make wave-live     # 连上 9091 采 4 秒, 报告帧率、列数、非法行、各列范围
```

正常输出长这样（实测 1000 Hz 档）：

```
  非空行      : 4031
  合法数据行  : 4031
  非法行      : 0
  时长/速率   : 4.00s  ≈ 1006.5 帧/秒
  各行的列数  : {3: 4031}
    IU(mA)   min=    -20  max=     -8  avg=  -14.6
    IV(mA)   min=    -28  max=     -5  avg=  -15.7
    IW(mA)   min=     17  max=     44  avg=   30.2
=== 通过: Quick Plot 可以正常画出 3 条曲线 ===
```

> 静止时三列不是精确的 0 而是约 −15 mA：这是 INA199 + ADC 的偏置
> （零电流时 `raw ≈ 2030` 而不是理论上的 2048）。**注意 IU+IV+IW 恒等于 0**，
> 这正好佐证 W 相的推导是对的。

`?` 状态里的 `RTT-WAVE` 行是固件侧的体检：

```
[I] RTT-WAVE ON  ch=1  rate=1000Hz  free=467/512  sent=5330 drop=18771
```

- `free` 是通道 1 缓冲还剩多少字节。**有人正常读时应该一直接近 512**
- `free=0` 且 `drop` 一路涨 = **没有客户端在连 9091**。这是预期行为，
  不是丢数据：Serial Studio 一点 Connect，OpenOCD 就开始抽，`free` 立刻回升
- 有客户端时实测 1006 行/秒（1000 Hz 档），**基本没有丢帧**

### 排版逻辑也能在电脑上单测

```bash
make wave-test     # C 侧排版单测 + 契约测试(喂给下面的 Quick Plot 解析器)
```

`app/wave_fmt.c` 和 `app/imon.c` 一样刻意不依赖任何 STM32 头文件，所以能在电脑上
直接编译运行。除了逐字节核对输出，还会用 **Quick Plot 的拆分规则**（只按逗号拆、
按行分帧）复检一遍 —— 确保固件吐出去的每一行，上位机都真的能画成 3 条曲线。
`tools/ss_wave_check.py` 就是这套规则的可执行版本，`--file` / `--port` / `--bin`
三种数据源都能吃。

### 这个通道的局限

- **最高 1 kHz**。采样时钟是 SysTick 的 1 ms 节拍，再往上需要独立定时器 +
  中断里采样。想看 PWM 开关频率级别的电流纹波，RTT 这条路带宽不够，
  用串口的 `c` 触发捕获（20 kHz）。
- 所以现在能看到的是**电流的基波形状**（几十 Hz 级的正弦/换相台阶），
  不是纹波细节。

## 换调试器

只用改一个变量，烧录 / 调试 / RTT 三处一起生效：

```bash
make flash DEBUGGER=stlink        # ST-Link V2 / V2-1 / V3（默认）
make flash DEBUGGER=cmsis-dap     # CMSIS-DAP / DAPLink
make flash DEBUGGER=jlink         # J-Link
```

多个探针同时插着时用序列号指定：

```bash
make probe DEBUGGER=cmsis-dap DBG_SERIAL=E47529FB51F487BD
```

> **为什么 ST-Link 不能加 `transport select swd`**
> ST-Link 走的是 OpenOCD 的 hla 驱动，transport 由 `hla_layout` 内部接管，
> 再显式 select 会直接报 `Debug adapter doesn't support 'swd' transport`。
> CMSIS-DAP / J-Link 是通用适配器，显式选 SWD 可以避免落到 JTAG。
> Makefile 里已经按调试器自动区分，不用手工记。

**换调试器后第一个该跑的命令是 `make probe`** —— 它会打印探针能否识别、
能否连上芯片、Flash 容量。连不上就先跑 **`make diag`**：它会

1. 先采样 5 次 IDR（`SWD DPIDR`），按"读回来的值长什么样"判断故障类型 ——
   这一步能把笼统的"连不上"翻译成具体原因（详见下面「IDR 特征速查」）
2. 再逐级降速重试（1 MHz → 100 kHz → 100 kHz + 复位连接）
3. 三档全失败则打印 H8 接线表和供电清单

### IDR 特征速查（`make diag` 的判据）

| 读回的 IDR | 含义 | 下一步 |
|---|---|---|
| `0x1ba01477` | 正常，F103 的 SW-DP | 通了 |
| 恒定 `0xffffffff` | 线上只有上拉，目标没驱动它 | 没上电 / 线断了 |
| 恒定 `0x00000000` | 线被拉死 | 短路到地，或接到输出低的脚 |
| **每次都不同的随机值** | 线上有**持续变化的数据** | **SWDIO 接到 H8-3（PA2 波形流）了** |
| 全部无应答 | 目标没在驱动 SWDIO | 供电 / 接线 |
| 时好时坏 | 接触不良 | 换杜邦线、重插 |

随机值这一条是实测总结出来的，最容易误判成"信号质量问题"而反复降速 ——
**降速对它是没用的**（10 kHz 也一样随机），因为根因是接错脚。

## 硬件接线与供电（**接错必黑屏，先看这节**）

原理图 `doc/原理图V2.pdf` 里有两个容易踩的点，`Error connecting DP: cannot read IDR`
十有八九就是它们。

### 1. 板子的调试口 H8 **不是**标准 ARM 10 针

H8 = `PM254-2-05-W-8.5`（2×5，2.54 mm），引脚定义如下：

| 针 | 信号 | 针 | 信号 |
|---|---|---|---|
| 1 | **+5V**（板子靠它供电） | 2 | 空 |
| 3 | TX → PA2 | 4 | **SWDIO** → PA13 |
| 5 | RX → PA3 | 6 | **SWCLK** → PA14 |
| 7 | **GND** | 8 | **GND** |
| 9 | 空 | 10 | 空 |

标准 ARM 10 针是 `2=SWDIO / 4=SWCLK / 3·5·9=GND`。**直接插标准排线的话，
SWDIO 会落到板子的空脚 2 上**（于是报 `cannot read IDR`），更糟的是把地怼到
板子第 3 脚 —— 那是 PA2 的推挽输出。所以要用杜邦线按上表逐根接：

```
DAPLink SWDIO → H8-4      DAPLink SWCLK → H8-6
DAPLink GND   → H8-7(或8) DAPLink 5V    → H8-1
```

> ⚠ **最容易犯的错：把 SWD 接到左列奇数列（1/3/5/7）。**
> 那样 5V→1、GND→7 都是对的，看起来"接上了"，但 SWDIO 落在 H8-3 (TX/PA2)、
> SWCLK 落在 H8-5 (RX/PA3)。现象非常有特征：**IDR 每次读回一个不同的随机值** ——
> 因为探针正在采 PA2 上那路 1 kHz 的 JustFloat 波形流。
> 而且探针的 SWDIO 驱动和 PA2 的推挽输出在互相"打架"，**改线前务必先断电**。

> 作者原话是配套 DAPLink「盲插即可工作」—— H8 就是为那块专用 DAPLink 的
> 排线顺序设计的。手上是别的调试器时，只能按上表手接。

### 2. 板上没有任何 5V 来源

全板器件清单里唯一的电源芯片是 **ME6206A33XG（5V→3.3V LDO）**，没有 buck、
没有第二颗 LDO。供电链路是：

```
外部 +5V ──> ME6206A33XG ──> +3.3V ──> STM32F103 / 电流采样
   │
   ├─ H8-1   (调试口，原设计由配套 DAPLink 喂 5V)
   ├─ H12-2  (+5V 低压电源口)
   ├─ H13-1  (+5V 预留 IO 口)
   └─ MT6701 磁编码器 VDD (作者有意用 5V 供电)

强电接口 (DC / VM) ──U15 跳线──> VM ──> DRV8313 电机驱动
```

**关键：强电接口（DC/VM）和逻辑 3.3V 不连通。** 只往强电口接 12V，
MCU 依然是黑的 —— 必须先有 +5V，MCU 才活。

`U15` 是 VM↔DC 的跳线，只在给 VM 供电时才有用。

### 3. 上电自检（万用表，3 个点）

| 测点 | 期望 | 说明 |
|---|---|---|
| C35 两端 | ≈ 5V | LDO 输入，没电压 = 外部 5V 没进来 |
| C34 两端 | ≈ 3.3V | LDO 输出，没电压 = LDO 或负载有问题 |
| 板上任意 `+3.3V` 焊盘 | ≈ 3.3V | MCU 供电 |

### 4. 顺带一个好处：DAPLink 自带虚拟串口

探针能打印 `CMSIS-DAP: UART via USB COM port supported`，也就是它本身就是个
USB-TTL。把 H8-3（TX）接到探针的 RX、H8-5（RX）接到探针的 TX，
**串口波形和 RTT 日志就能走同一根 Type-C**，不用再单独买 USB-TTL 模块。

> 注意 DAPLink 会同时暴露**两个** `/dev/cu.usbmodem*`，只有一个是通到 MCU 的
> UART。`make plot` 会自动逐个试读、挑真能读到数据的那个（见 `_score_port` /
> `guess_port`）；想手动指定就 `make plot` 时加 `-p /dev/cu.usbmodemXXXX`。

## RTT 日志

RTT 的原理：固件在内存里放一个控制块 + 上下行环形缓冲，
探针用 SWD 直接读这块内存取日志。所以：

- 不需要额外接 UART 线，只靠 SWD 的 4 根线
- 不需要停下 CPU，可以边跑边看
- ST-Link / CMSIS-DAP / J-Link 全都支持（对探针来说就是"读内存"）

### 两种用法

```bash
# 一条命令：起 OpenOCD + 自动连上，Ctrl-C 一起退出
make rtt

# 或者两个终端分开（方便一边看日志一边干别的）
make debug      # 终端 1：OpenOCD + RTT server
make log        # 终端 2：裸连接（nc）
```

`make rtt` 的客户端比 `nc` 多了时间戳和落盘：

```bash
tools/rtt_log.py --port 9090 --log output/rtt.log   # 带时间戳 + 落盘（自动去 ANSI 色）
tools/rtt_log.py --plain                            # 连终端颜色也去掉，方便贴给别人
```

### 控制块地址：扫描模式 vs 从编译产物取址

默认按 `"SEGGER RTT"` 这个 16 字节 ID 扫描整段 SRAM（20 KB）。

知道控制块在哪儿时可以把搜索窗口缩到 1 KB —— 地址**从编译产物里自动取**，不用手写：

```bash
make rtt RTT_CB=1        # 推荐：先读 map 文件，读不到再退回 ELF 符号表
make debug RTT_CB=1
```

| 写法 | 来源 | 说明 |
|---|---|---|
| `RTT_CB=1` | map，读不到再退 nm | 推荐 |
| `RTT_CB=map` | 只读 `output/stm32f103c8t6.map` | 纯文本，**不依赖工具链在不在 PATH** |
| `RTT_CB=nm` | 只读 ELF 符号表 | `arm-none-eabi-nm` |

想知道到底取到了什么、两个来源是否一致：

```bash
make rtt-addr
```

```
>>> _SEGGER_RTT 地址来源对照
    map output/stm32f103c8t6.map
        -> 0x20002940
    nm  output/stm32f103c8t6.elf
        -> 0x20002940
>>> RTT_CB=1 生效: 0x20002940  [来源: map], 搜索窗口缩到 0x400
```

map 里那一行长这样（`.bss` 段里，符号定义行的**第 2 个字段**才是符号名）：

```
 .bss._SEGGER_RTT
                0x20002940       0x78 rtt/SEGGER_RTT.o
                0x20002940                _SEGGER_RTT     ← 取这行的 $1
```

> ⚠️ **`RTT_CB` 不等于"不扫描"。** OpenOCD 的 `rtt setup <addr> <size> <id>` 永远是
> "在 `[addr, addr+size)` 这个窗口里搜 ID"，**没有**"直接用这个地址"的模式
> （0.12.0 源码里就是 `for (addr = *address; ...) ` 逐字节滑窗比对）。
> 它做的是把窗口 **从 20 KB 缩到 1 KB**：控制块正好在窗口首址，所以第一块 1 KB
> 就读到了（原本要读 20 块），启动更快；窗口小也顺带降低"在别处撞上同样字节序列"的概率。
>
> ⚠️ 地址对应的是**这份产物**。改了代码没重新 `make flash`，板子上的控制块就不在这个
> 地址；**OpenOCD 不会报错**，只会在日志里留一句容易被忽略的提示，现象是"端口连上了
> 但一行日志都没有"。所以顺序永远是 `make flash` → `make rtt RTT_CB=1`。

### 在代码里打日志

```c
#include "log.h"

LOG_INFO("duty=%u", duty);      // [I] duty=720
LOG_WARN("overcurrent %u", raw);
LOG_ERROR("...");
LOG_DEBUG("...");
```

级别用 `LOG_LEVEL` 编译期裁剪（0=关 1=ERROR 2=WARN 3=INFO 4=DEBUG，默认 4）。
调完 FOC 算法后把 `LOG_LEVEL` 调到 2 可以省不少 Flash。

注意 `SEGGER_RTT_printf` 默认**不支持 `%f`**，浮点要拆成整数+小数打。
`main()` 第一行必须调 `log_init()` —— 不调的话控制块里的 ID 不会写进去，探针永远找不到。

## VSCode 图形化调试

1. 装扩展：搜 **Cortex-Debug**（`marus25.cortex-debug`）
2. 打开本工程，`.vscode/` 里已经配好了三个 launch 配置：
   - `CMSIS-DAP + RTT`
   - `ST-Link + RTT`
   - `attach + RTT`（只连不断点，用来"看着日志跑"）

按 F5 直接在 **调试控制台**里看 RTT 日志、敲命令（`m`/`s`/`f`...），
断点和日志可以同时用。

## 串口波形（没有示波器时替代用）

```bash
pip install pyserial pyqtgraph PyQt5    # 或 make deps
make plot                               # 实时画 4 通道
```

不装 Python 也行，用 **VOFA+**：协议选 **JustFloat**，波特率 921600。

两种模式：

- **流模式（`s`）** —— 连续发送，看趋势。带宽上限约 4600 帧/秒（921600 bps、4 通道）
- **捕获模式（`c`）** —— 20 kHz 采到片上 SRAM 再慢速上传。想看高频换相细节只能用这个，
  因为 20 kHz 实时传输物理上不可能

## 排错

| 现象 | 原因 / 处理 |
|---|---|
| `make flash` 报 `Error: CMSIS-DAP command CMD_INFO failed.` 或 `error reading data: Entity not found` | **探针被另一个 OpenOCD 独占**了（最典型是 `make rtt` / `make debug` 的终端窗口被直接叉掉，没走 Ctrl-C）。`make flash` / `probe` / `reset` 现在会**自动检测并释放**，直接重跑即可；想手动清：`make stop`。**跟你的代码无关** |
| `make probe` 报 `open failed` | 探针被别的进程占用 —— `make stop`（等价 `pkill -f openocd`） |
| `make flash` 能连上但 `cannot read IDR` | 那是接线/供电问题，不是占用问题，见下一行 |
| `Error connecting DP: cannot read IDR` + IDR 每次数值都不同 | **SWDIO 接到了 H8-3（PA2 波形流）而不是 H8-4**。断电后把 SWDIO 改到 4、SWCLK 改到 6 |
| `Error connecting DP: cannot read IDR` + IDR 恒定 | 没供电或接线错，见「硬件接线与供电」。SWDIO 读到 0 基本就是板子没上电（未上电时 MCU 的保护二极管会把 SWDIO 钳到 0.7V，读出来就是 0） |
| `make diag` 说"全部无应答" | SWDIO/SWCLK/GND 没接通。万用表蜂鸣档对着 H8 逐根点 |
| `make rtt` 提示端口已被占用 | 上次 OpenOCD 没退干净，`make stop` |
| 自写的测试程序里 `delay_ms(1000)` 像"卡住"，几十秒才走一格 | 老的 `delay_ms` 是手写 `__NOP` 空循环，`-Og` 下内层是 5 条指令，真实延时是标称值的 5~20 倍。已改成轮询 SysTick 的精确实现（不开中断，不占用 `SysTick_Handler`） |
| RTT 连上但一行日志都没有 | ① 固件里 `log_init()` 没调；② **用了 `RTT_CB=1` 但没重新 `make flash`** —— 产物里的地址和板子上的对不上，OpenOCD 不会报错，只留一句容易被忽略的提示 → 先 `make flash`；③ 不加 `RTT_CB` 用默认扫描模式对照一下 |
| `make rtt-addr` 显示 `✗ 没读到` | 那一侧的产物不存在或过期：`output/*.map` / `output/*.elf` 先 `make` 一次。两边都读到就说明取址正常 |
| `make rtt RTT_CB=1` 提示"读不到 _SEGGER_RTT，回退到扫描模式" | 产物还没生成（先 `make`），或 `_SEGGER_RTT` 被 `--gc-sections` 回收了（那就说明 `log_init()` 真的没被调用）。回退后功能不受影响，只是扫 20 KB |
| 波形一片空白 | 先按 `l` 进回环模式、短接 PA2↔PA3 打字看回显，确认是 MCU 侧还是 PC 侧问题 |
| 波形有帧但全是 0 | 大概率 USB-TTL 的 RXD 接到了 PA3 而不是 PA2，或者 TXD↔TXD 接反了 |
| `make plot` 说"都没读到数据" | DAPLink 会同时暴露两个 `/dev/cu.usbmodem*`，只有一个通到 MCU 的 TX。用 `-p` 指定另一个 |
| `dropped` 一直在涨 | 采样率超过串口带宽，按 `f` 降一档，或改用 `c` 捕获模式 |
| IMON 行里 `raw` 恒为 0 或 4095 | ADC 通路没通：PA0/PA1 模拟输入、INA199 供电、或 ADC 没启动 |
| IMON 行里 `raw≈2048` 但电机转起来纹丝不动 | 电流采样没跟着电机。先用 `make plot` 看串口波形里 IU/IV 是否也不动，再查采样电阻 / INA199 焊接 |
| `make imon` 窗口一片空白 | 现在会自己起 OpenOCD，也会在连上 5 秒没数据时打印排查清单。真去查顺序是：`make rtt` 里有没有 `IMON t=` 开头的行 → 有就是解析问题（不可能了，`make imon-test` 会兜住）→ 没有就敲 `i`/`?` 看 IMON 开关和固件版本 |
| `make imon` 提示"固件没有 ang/mag 字段" | 板子上还是旧固件，`make flash` 就有角度读数了（电流曲线不受影响） |
| Serial Studio 连上 9091 但一条曲线都没有 | ① `make rtt` / `make debug` 没在跑 → 没人监听 9091；② 右侧 Setup 面板的模式必须是 **Quick Plot**（不是 Console Only）；③ Socket type 必须是 **TCP** 且地址 `127.0.0.1:9091` |
| Serial Studio 通道名显示成 Channel 1/2/3 | 正常现象，Quick Plot 没拿到标签行。按 `w`（关）→ Connect → `w`（开）重来一次即可，见「Serial Studio 波形」 |
| Serial Studio 曲线上有个别毛刺/断线 | 说明混进了非数字行 —— 你多半连到了 **9090**（日志通道）而不是 **9091**。用 `make wave-live` 验证一下 |
| `?` 里 RTT-WAVE 的 `drop` 一直涨、`free=0` | 没有客户端连 9091。这是预期行为，不是丢数据；连上之后 `free` 会立刻回升到接近 512 |
| `make wave-live` 报"不通过" | 看它打印的非法行样例：如果是 `[I] ...` 之类的日志行，说明连错端口了（要连 9091，不是 9090） |
| 电机"按了 `m` 不转" | 先看 `?` 的 `motor=` 和 `oc_trips=`。`motor=ON` 但不转 → 占空比太小（默认 5.6% 只有约 12 RPM，又慢又轻，很可能你以为它没动），按 `+` 加大；`oc_trips` 涨了 → 被过流保护停机，见「已知限制」的阈值说明 |
| 电机"转着转着突然停了" | 过流保护动作（阈值约 0.61 A）。终端会有告警日志，`?` 里的 `oc_trips` 也会 +1。按 `-` 降占空比 |

**判断"电机到底转没转"，不要看电流，看 `ang`**。电流变大只说明有电流（堵转
一样吃电流）；`ang` 一直在单调变化才是转子真在动。实测：静止 3 秒累计行程
0.0°，`m` 之后 6 秒走 432°（`raw` 那几列也同步变抖）。

**不用 SWD 也能验证 MCU 在不在跑**：把 H8-3（TX/PA2）接到 DAPLink 的 RX，
然后跑 `make plot` —— 它会试读每个候选串口并打印选中了哪个。
能看到 4 条曲线 = MCU 正在跑固件，问题就只剩 SWD 那两根线；
一直是"都没读到数据" = MCU 没起来，回头查供电和 BOOT0。

## 已知限制

- 采样时基目前是 **TIM2 自由跑**。对三步换相够用，但做真正的 FOC 电流环之后
  必须改成 **TIM1 TRGO 触发**（PWM 周期同步采样），否则采到的是随机相位，
  电流波形没有意义。
- 电流采样还**没有和 PWM 同步**。单个 ADC 值的原子性没问题（对齐的 16 位读在
  Cortex-M3 上是一条 `LDRH`，本身不可分割），但 `ina199_read_uv_raw()` 连读两个
  寄存器的那一瞬间 DMA 仍可能插进来更新一次（约 0.5% 概率，最多差一个采样点）。
  真上电流环时要改成 ADC 注入通道 + 转换完成中断里取快照，而不是靠关中断 ——
  `__disable_irq()` 对 DMA 本来就是无效的。
- 引入 `snprintf` 做格式化，把 newlib 的 `_malloc_r/_realloc_r/_sbrk/_impure_data`
  一起拉进了镜像（约 2 KB Flash + 76 B `.data`）。目前 Flash 只用 30%，不构成问题；
  真要省就换掉 `app/imon.c` 里的 `snprintf`（`make fmt-test` 的断言可原样复用）。
  `app/rtt_wave.c` 的热路径（1 kHz）**没有**用 `snprintf`，是手写的整数转换。
- RAM 已用到 73.5%，大户是 8 KB 的捕获缓冲 + RTT 通道 1 的 512 B 波形缓冲。
  真上 FOC 电流环肯定要腾地方，把 `scope.h` 里 `SCOPE_CAP_N` 从 1024 降到 512
  能省 4 KB；觉得波形缓冲太大也可以把 `rtt_wave.h` 的 `RTT_WAVE_BUF_SIZE` 调小
  （512 B 在 1 kHz 下约 28 ms 余量，降到 256 B 也够）。
- **过流保护阈值离实际工作点不远**。`motor.c` 的 `OC_THRESHOLD_ADC = 2800`
  对应约 **0.61 A**；实测 35% 占空比时峰值已经到 0.58 A（`raw 2733`）。
  再往上按 `+`（45%、55%…）会直接触发保护停机 —— 现象是电机"转着转着突然
  停了"，现在 `?` 里的 `oc_trips` 和主循环的告警日志能让你立刻认出它。
  （`motor.c` 里那句"≈1.2A"的旧注释是错的，已改。）
- **`mag` 读数一直是 0**，可疑但未定论。MT6701 的 4-bit 磁场强度正常应在中间
  值附近；不过角度本身是可信的（静止时 3 秒累计行程 0.0°，旋转时单调连续变化），
  所以至少帧对齐没问题。要坐实得做两件事：把 SSI 时序对着 MT6701 手册核一遍
  （当前代码在 CLK 下降沿采样），以及把帧尾那个被忽略的 6-bit CRC 校验上。
  在这之前**回读的角度可以当"相对运动指示"，先别拿去做 FOC 的电角度解算**。

## 目录

```
app/       应用层：motor / drv8313 / mt6701 / ina199 / uart / scope / log
           / imon（纯排版，可单测） / rtt_mon（1kHz 采样 + RTT 打印）
           / wave_fmt（三相电流排版，纯逻辑可单测）
           / rtt_wave（RTT ch1 波形输出，给 Serial Studio）
drivers/   STM32 标准外设库
rtt/       SEGGER RTT（日志通道 0 + 波形通道 1）
startup/   启动文件 + 系统时钟 + 中断入口
tools/     rtt.sh（起 OpenOCD + 两个 RTT server + 挂任意客户端）、rtt_log.py、probe.sh
           scope_plot.py（串口 JustFloat 波形）
           imon_plot.py（RTT 电流曲线）、test_imon_fmt.c（C 侧排版单测）
           test_imon_plot.py（上位机解析 + 两端契约测试）
           ss_wave_check.py（Quick Plot 规则校验：--port 实时 / --file / --bin）
           test_wave_fmt.c（C 侧波形排版单测）
core/      Cortex-M3 内核支持
doc/       原理图与芯片手册
```
