# axibram AXI 接口驱动

## 1. 项目简介

本项目是面向**复旦微 FMQL（ARM + FPGA SoC，类 Zynq 架构）**平台的 AXI 接口字符设备驱动及配套应用程序。

驱动名为 `axibram`，将 FPGA 侧的多个 AXI 外设（寄存器、UART、GPIO、SIM 安全卡、SelectMap 配置口）以统一的字符设备接口（ioctl / mmap）暴露给用户态，并配套一整套测试程序。

| 项 | 值 |
|---|---|
| 内核 | linux-4.14.55-fmsh |
| 交叉编译器 | arm-linux-gnueabihf-（Linaro 7.3.1） |
| 内核模块 | axibram.ko |
| 设备节点 | /dev/axibram0 ~ /dev/axibram16 |
| 设备树 compatible | xlnx,axibram-chrdev0 ~ xlnx,axibram-chrdev16 |

## 2. 目录结构

```
axibram/
├── Makefile              # 顶层 Makefile
├── README.md             # 本文档
├── axibram/              # 内核驱动
│   ├── axi_bram.c        #   platform 驱动入口（probe / remove）
│   ├── axibram_chrdev.c  #   字符设备核心（open/release/poll/mmap/ioctl）
│   ├── axibram_irq.c     #   中断处理（安装/卸载、三种通知方式）
│   ├── axi_selectmap.c   #   FPGA SelectMap 配置
│   ├── axibram.h         #   驱动主头文件
│   └── axibram_ioctl.h   #   ioctl 接口定义（内核/用户态共享）
├── examples/             # 应用程序
│   ├── examples.mk       #   应用程序构建文件
│   ├── driver/           #   用户态接口库
│   │   ├── axibram_driver.c/h  # 基础驱动接口
│   │   ├── axibram_uart.c/h    # PL UART
│   │   ├── axibram_gpio.c/h    # GPIO
│   │   └── utils.c/h           # 工具函数
│   ├── irq.c             #   中断/DMA 测试程序（四种方式）
│   ├── reg.c             #   寄存器读写测试
│   ├── uart.c            #   串口收发测试
│   ├── gpio.c            #   GPIO 翻转测试
│   └── selectmap.c       #   FPGA bitstream 加载
└── outputs/              # 编译产物（axibram.ko + *.elf）
```

## 3. 驱动功能

### 3.1 寄存器读写

通过 ioctl 直接读写 FPGA 侧寄存器：

- `AXIBRAM_IO_READ_REGISTER`：读取寄存器（传入偏移、返回数据）
- `AXIBRAM_IO_WRITE_REGISTER`：写寄存器（传入偏移和数据）

### 3.2 DMA 内存映射（mmap）

`mmap()` 通过 `dma_alloc_coherent` 分配连续、非缓存的 DMA 内存并映射到用户空间，`munmap()` 时自动释放。主要用于 SelectMap 传输 FPGA bitstream。

还可通过 `AXIBRAM_IO_GET_DMA_ADDR` 获取该缓冲区的 DMA 总线地址（本平台即物理地址），供应用写进 FPGA 侧 DMA 引擎的地址寄存器，实现自定义 DMA 传输。

### 3.3 中断通知（三种方式）

驱动在中断到来时**同时**触发三种通知，应用可任选其一或组合使用：

| 方式 | 触发机制 | 特点 |
|---|---|---|
| 实时信号 | `send_sig_info(SIGRTMIN+2)` | 可排队、不丢事件，`si_code` 携带硬件中断号 |
| select/poll | `wake_up + POLLIN` | 同步查询，可与其它 fd 一起等、带超时 |
| SIGIO | `kill_fasync(SIGIO)` | 标准 POSIX 异步通知，但高频会丢事件 |

### 3.4 中断触发方式可配置

通过 ioctl 由应用程序指定中断触发方式（上升沿/下降沿/高电平/低电平），不再硬编码。

### 3.5 FPGA SelectMap 配置

`AXIBRAM_IO_SELECT_MAP` 通过 SelectMap 接口向 FPGA 加载 bitstream，内部实现完整的配置状态机（自检、复位、Init_b/Program_b 握手、FIFO 写、Done 状态校验）。

### 3.6 其它

- 版本查询：`AXIBRAM_IO_VERSION`
- 中断安装/卸载：`AXIBRAM_IO_IRQ`（传 0 卸载，否则传信号号安装）

## 4. 设备节点与映射

| 设备节点 | 用途 |
|---|---|
| /dev/axibram0 | 通用（寄存器读写、GPIO、SIM 卡复位） |
| /dev/axibram2 | FPGA SelectMap 配置 |
| /dev/axibram3/4/5/7/8/9 | UART 串口 |
| /dev/axibram6 | SIM 安全卡 |

> 具体映射以设备树配置为准。带中断的 `axibramN` 设备需在设备树里配置 `interrupts` 资源。

### 4.1 设备树节点配置示例

驱动通过 `compatible` 匹配设备树节点，`xlnx,axibram-chrdevN` 对应 `/dev/axibramN`（N = 0 ~ 16）。

```dts
/ {
    amba_pl: amba_pl {
        #address-cells = <1>;
        #size-cells = <1>;
        compatible = "simple-bus";
        ranges;

        /* 寄存器类设备（无中断），映射到 /dev/axibram0 */
        axibram0: axibram@80000000 {
            compatible = "xlnx,axibram-chrdev0";
            reg = <0x80000000 0x10000>;        /* 基地址 0x80000000，大小 64KB */
        };

        /* 带中断的设备，映射到 /dev/axibram1 */
        axibram1: axibram@80010000 {
            compatible = "xlnx,axibram-chrdev1";
            reg = <0x80010000 0x10000>;
            interrupt-parent = <&intc>;         /* 中断控制器 */
            interrupts = <0 29 4>;              /* GIC SPI 29，高电平触发 */
        };
    };
};
```

**字段说明：**

| 属性 | 必需 | 说明 |
|---|---|---|
| `compatible` | 是 | 必须为 `xlnx,axibram-chrdevN`（N = 0~16），决定设备号 `/dev/axibramN` |
| `reg` | 是 | 寄存器内存映射 `<基地址 大小>`，驱动 ioremap 后用于寄存器读写 |
| `interrupt-parent` | 中断时需要 | 指向中断控制器（如 GIC 的 `&intc`） |
| `interrupts` | 中断时需要 | 中断号与触发方式，格式取决于中断控制器 |

> 说明：
> - 上面 `0x80000000`、`0x80010000` 只是示例地址，实际以 FPGA 工程分配的地址为准；
> - `interrupts` 的格式依中断控制器而定——Zynq/FMQL 的 ARM GIC 通常是三单元 `<类型 中断号 触发方式>`（0=SPI 共享外设中断，4=高电平，1=上升沿，2=下降沿，8=低电平）；
> - 设备树里的触发方式只是初始提示，运行时可通过 `AXIBRAM_IO_IRQ_TRIGGER` 重新指定。

## 5. ioctl 接口定义

接口定义统一在 `axibram/axibram_ioctl.h`（内核与用户态共享同一份）。

```c
#define AXIBRAM_IOCTL_MAGIC 0x34

struct axibram_register {
    unsigned int offset;   // 寄存器偏移（字节，4 的倍数）
    unsigned int data;     // 数据
};

struct axibram_selectmap_transaction {
    unsigned int size;
    void *user_addr;
};

struct axibram_dma_info {
    unsigned long long dma_addr;   // DMA 总线地址（本平台 = 物理地址）
    unsigned int       size;
};
```

| 命令 | 宏 | 功能 |
|---|---|---|
| `AXIBRAM_IO_VERSION` | `_IOR(0x34, 0, unsigned int)` | 读驱动版本号 |
| `AXIBRAM_IO_READ_REGISTER` | `_IOR(0x34, 1, struct axibram_register)` | 读寄存器 |
| `AXIBRAM_IO_WRITE_REGISTER` | `_IOW(0x34, 2, struct axibram_register)` | 写寄存器 |
| `AXIBRAM_IO_SELECT_MAP` | `_IOW(0x34, 3, struct axibram_selectmap_transaction)` | SelectMap 配置 FPGA |
| `AXIBRAM_IO_IRQ` | `_IOW(0x34, 10, unsigned int)` | 装/卸中断（0 卸载，否则信号号） |
| `AXIBRAM_IO_IRQ_TRIGGER` | `_IOW(0x34, 11, unsigned int)` | 设置中断触发方式 |
| `AXIBRAM_IO_GET_DMA_ADDR` | `_IOR(0x34, 12, struct axibram_dma_info)` | 获取 DMA 缓冲区物理地址 |

中断触发方式常量（取值与内核 `IRQF_TRIGGER_*` 一致）：

```c
#define AXIBRAM_IRQ_TRIGGER_NONE     0x0
#define AXIBRAM_IRQ_TRIGGER_RISING   0x1   // 上升沿
#define AXIBRAM_IRQ_TRIGGER_FALLING  0x2   // 下降沿
#define AXIBRAM_IRQ_TRIGGER_HIGH     0x4   // 高电平
#define AXIBRAM_IRQ_TRIGGER_LOW      0x8   // 低电平
```

中断通知信号：`AXIBRAM_IRQ_SIGNAL = (SIGRTMIN + 2)`（用户态定义，避开 glibc/NPTL 保留的 `SIGRTMIN`/`SIGRTMIN+1`）。

## 6. 编译方法

### 6.1 环境准备

交叉编译前需修改两个机器相关的路径：

1. **顶层 [Makefile](Makefile)**：`GNU_PATH` 改为实际交叉编译器目录。
2. **[axibram/axibram.mk](axibram/axibram.mk)**：`KERNEL_PATH` 改为实际内核源码目录。

### 6.2 编译

```bash
make           # 编译驱动 axibram.ko + 全部应用程序
make clean     # 清理
make help      # 查看帮助
```

编译产物输出到 `outputs/` 目录：

```
outputs/axibram.ko
outputs/irq.elf
outputs/reg.elf
outputs/uart.elf
outputs/gpio.elf
outputs/StartV7.elf
```

### 6.3 加载驱动

```bash
insmod axibram.ko
# 设备节点由驱动自动创建（/dev/axibram0 ~ /dev/axibram16）
```

> 注意：不要在 VMware 共享目录 `/mnt/hgfs` 里编译，会有时钟漂移导致 make 误判；建议 `cp -r` 到虚拟机本地目录再编译。

## 7. 应用程序使用说明

### 7.1 寄存器读写（reg.elf）

```bash
./reg.elf read  0           # 读寄存器 0（偏移 = 0 * 4）
./reg.elf write 0 0x123     # 写寄存器 0，值 0x123
```

### 7.2 GPIO 测试（gpio.elf）

```bash
./gpio.elf 0    # 对 GPIO 0 翻转输出波形（pin 范围 0~7，8 为 SF 卡复位）
```

### 7.3 串口收发（uart.elf）

```bash
./uart.elf 0 115200 aabbcc   # 端口 0，波特率 115200，发送 hex 数据 aabbcc
```

### 7.4 FPGA bitstream 加载（StartV7.elf）

```bash
# 将 bitstream 文件 v7_pl_binstream.bin 放到与程序同目录
./StartV7.elf
```

### 7.5 中断/DMA 测试（irq.elf）

```bash
./irq.elf                        # 方式1：实时信号（默认），默认上升沿
./irq.elf /dev/axibram0 2        # 方式2：select/poll
./irq.elf /dev/axibram0 3        # 方式3：SIGIO
./irq.elf /dev/axibram0 4        # 方式4：DMA 物理地址获取
./irq.elf /dev/axibram0 1 0x2    # 方式1 + 下降沿触发
```

- 方式 1：实时信号通知（`sighandler` 参数），`pause()` 等待；
- 方式 2：select/poll 通知，`select()` 等待可读；
- 方式 3：SIGIO 异步通知（`handler` 参数）；
- 方式 4：DMA 物理地址获取（mmap 后取物理地址，用于配置 FPGA DMA 引擎）。

## 8. 用户态接口库（examples/driver）

| 函数 | 功能 |
|---|---|
| `axibram_driver_open/close` | 打开/关闭设备 |
| `axibram_driver_malloc/free` | 申请/释放 DMA 映射内存 |
| `axibram_driver_get_dma_addr` | 获取 DMA 缓冲区物理地址 |
| `axibram_driver_version` | 获取驱动版本号 |
| `axibram_driver_readReg/writeReg` | 读/写寄存器 |
| `axibram_driver_insert_irq` | 注册中断（handler/SIGIO + sighandler/实时信号） |
| `axibram_driver_set_irq_trigger` | 设置中断触发方式 |
| `axibram_driver_selectmap` | SelectMap 加载 FPGA bitstream |
| `MW_GPIO_Open/Close/ReadPin/WritePin` | GPIO 操作 |
| `PLUART_Open/Close/Send/Read` | PL UART 操作 |

## 9. 注意事项

1. **打开设备需 root 权限且必须带 `O_EXCL`**（设备独占访问），用户态 `axibram_driver_open` 已处理。
2. **中断设备必须配置中断资源**：只有设备树里配了 `interrupts` 的 `axibramN` 设备才有中断，否则安装中断会失败。
3. **硬中断应答寄存器偏移需按实际硬件修改**：`axibram/axibram_irq.c` 里的 `AXIBRAM_IRQ_STATUS_OFFSET` 是占位值（默认 0），需按 FPGA 实际的中断状态/清除寄存器改成正确偏移和读写方式，否则电平触发的中断会造成中断风暴。
4. **ioctl 接口定义单源头**：所有命令号、结构体定义都在 `axibram/axibram_ioctl.h`，内核与用户态共用，改接口只改这一处。
5. **`void *user_addr` 结构体字段有 32/64 位 ABI 隐患**：当前 32 位 ARM、内核与用户态一致所以没问题；若将来出现 64 位用户态（compat），`_IOC_SIZE` 会失配。
