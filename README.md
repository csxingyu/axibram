# AXI BRAM 驱动使用说明书

> 长沙星语科技有限公司 · 软件研发部

## 1. 项目简介

本仓库实现一个运行在 **复旦微 FMQL（类 Zynq）平台** 上的 Linux 字符设备驱动。CPU 通过 AXI 总线访问 FPGA 侧的 Block RAM / SelectMap 配置口，为用户态程序提供以下能力：

- **寄存器读写**：通过 `ioctl` 或 `mmap` 直接读写 FPGA 外设寄存器；
- **DMA 一致性内存**：申请非缓存（uncached）的连续 DMA 内存并映射到用户态，供 FPGA 侧 DMA 引擎使用；
- **FPGA 中断通知**：支持实时信号、`select/poll`、`SIGIO` 三种异步/同步通知方式；
- **SelectMap 在线烧写**：通过 SelectMap 接口在线配置 Xilinx 7 系列 FPGA bitstream，支持内核加载与应用层直接加载两种方式。

设备节点形如 `/dev/axibram0`、`/dev/axibram1` ……，最多支持 17 个实例（`axibram0` ~ `axibram16`）。

## 2. 开发环境

| 项目 | 说明 |
|---|---|
| 目标平台 | 复旦微 FMQL（ARM Cortex-A9，类 Xilinx Zynq） |
| 内核版本 | `linux-4.14.55-fmsh`（复旦微定制内核） |
| 架构 | ARM 32 位（`ARCH=arm`） |
| 交叉编译器 | `arm-linux-gnueabihf-gcc`（gcc-linaro 7.3.1） |
| 编译主机 | Linux（交叉编译） |

交叉工具链路径与内核源码路径在 Makefile 中为作者本机路径，编译前需按实际情况修改，见下文「编译方法」。

## 3. 程序结构

```
axibram/
├── Makefile               # 顶层构建入口（driver + examples）
├── axibram/               # 内核驱动源码
│   ├── axibram.h          # 设备结构体、宏定义、函数声明
│   ├── axibram_chrdev.c   # 字符设备：file_operations、mmap、ioctl、poll
│   ├── axibram_irq.c      # 中断安装/卸载、信号发送、三种中断通知
│   ├── axi_bram.c         # platform 驱动入口：probe / remove
│   ├── axi_selectmap.c    # SelectMap V7 配置时序
│   ├── axi_selectmap.h    # SelectMap 寄存器位域定义
│   ├── axibram_ioctl.h    # ioctl 命令与参数结构定义（内核/用户态共用）
│   ├── Kbuild             # 内核模块构建规则
│   ├── Makefile           # 驱动独立编译入口
│   └── axibram.mk         # 驱动构建配置（被顶层 Makefile include）
└── examples/              # 用户态示例
    ├── driver/
    │   ├── axibram_driver.c/.h  # 驱动访问封装库
    │   └── utils.c/.h           # 工具函数（StringToDec 等）
    ├── reg.c              # 寄存器读写示例
    ├── irq.c              # 中断通知演示示例
    ├── selectmap.c        # SelectMap 应用层烧写示例（mmap 寄存器后直接配置）
    ├── makefile           # 示例独立编译入口
    └── examples.mk        # 示例构建配置（被顶层 Makefile include）
```

## 4. 编译方法

### 4.1 修改路径

- 顶层 [Makefile](Makefile) 中的 `GNU_PATH`：交叉工具链 bin 目录；
- [axibram/axibram.mk](axibram/axibram.mk) 中的 `KERNEL_PATH`：内核源码树路径；
- （若单独用）[axibram/Makefile](axibram/Makefile) 中的 `KDIR`：内核源码树路径。

### 4.2 编译命令

```bash
make            # 编译驱动 + 示例（等价于 make all）
make driver     # 只编译内核驱动，生成 axibram/axibram.ko
make examples   # 只编译示例，生成 reg.elf / irq.elf / StartV7.elf
make clean      # 清理所有中间文件与输出
```

产物说明：

- 驱动：`axibram/axibram.ko`；
- 示例：`examples/reg.elf`、`examples/irq.elf`、`examples/StartV7.elf`（SelectMap 烧写）。

### 4.3 装载驱动

将 `axibram.ko` 和示例放到目标板后：

```bash
insmod axibram.ko
# 设备树匹配成功后自动生成 /dev/axibramN 节点
```

## 5. 设备树配置

驱动通过 `compatible` 属性匹配设备，支持 `xlnx,axibram-chrdev0` ~ `xlnx,axibram-chrdev16`。每个设备节点至少需要配置寄存器地址区间，需要中断的节点还需配置中断资源。示例：

```dts
axibram0: axibram@43c00000 {
    compatible = "xlnx,axibram-chrdev0";
    reg = <0x43c00000 0x10000>;
    interrupt-parent = <&intc>;
    interrupts = <0 29 4>;   /* 中断号与触发方式，驱动通过 irq_get_trigger_type 读取 */
};
```

- `reg`：FPGA 外设寄存器物理地址与大小，驱动用 `devm_ioremap_resource` 映射；
- `interrupts`：可选；触发方式由设备树决定（0x1 上升沿 / 0x2 下降沿 / 0x4 高电平 / 0x8 低电平）。

## 6. 接口说明

### 6.1 打开设备

```c
int fd = open("/dev/axibram0", O_RDWR | O_EXCL);
```

- 必须以 **root**（`CAP_SYS_ADMIN`）打开；
- 必须带 **`O_EXCL`**，且设备为独占打开。

### 6.2 ioctl 命令

命令定义见 [axibram_ioctl.h](axibram/axibram_ioctl.h)，magic 号为 `0x34`：

| 命令 | 方向 | 参数类型 | 说明 |
|---|---|---|---|
| `AXIBRAM_IO_VERSION` | 读 | `unsigned int` | 读取驱动版本号（当前 `0x01000001`） |
| `AXIBRAM_IO_READ_REGISTER` | 读 | `struct axibram_register` | 读寄存器（offset 为字节偏移，须 4 对齐） |
| `AXIBRAM_IO_WRITE_REGISTER` | 写 | `struct axibram_register` | 写寄存器 |
| `AXIBRAM_IO_SELECT_MAP` | 写 | `struct axibram_data_transaction` | SelectMap 烧写 bitstream（内核方式） |
| `AXIBRAM_IO_GET_REG_SIZE` | 读 | `unsigned int` | 读取寄存器空间大小（配合 mmap 寄存器映射） |
| `AXIBRAM_IO_IRQ` | 写 | `unsigned int` | 安装/卸载中断，0 卸载，非 0 为通知信号号 |

```c
struct axibram_register {
    unsigned int offset;  /* 寄存器字节偏移 */
    unsigned int data;    /* 数据 */
};

struct axibram_data_transaction {
    unsigned int size;    /* bitstream 字节数 */
    void *user_addr;      /* 存放 bitstream 的用户地址（须为 mmap DMA 内存） */
};
```

### 6.3 mmap 双语义

`mmap` 通过 offset 区分两种映射（见 `axibram_mmap`）：

| offset | 含义 | 实现 |
|---|---|---|
| `0` | 申请一块 **DMA 一致性内存** | `dma_alloc_coherent` + `dma_mmap_coherent`，非缓存，供 FPGA DMA / SelectMap 数据使用 |
| `getpagesize()`（一页） | 映射 **FPGA 寄存器** | `remap_pfn_range` + `pgprot_noncached`，直接操作硬件寄存器 |

映射寄存器前，通常先通过 `AXIBRAM_IO_GET_REG_SIZE` 获取寄存器空间大小，再以该大小 `mmap`。

对应封装（[axibram_driver.c](examples/driver/axibram_driver.c)）：

```c
void *axibram_driver_malloc(int fd, unsigned int size);  /* DMA 内存，mmap offset=0 */
void *axibram_driver_mmap(int fd, unsigned int size);    /* 寄存器，mmap offset=页 */
unsigned int axibram_get_reg_size(int fd);               /* 获取寄存器空间大小 */
```

### 6.4 中断通知

应用先 `sigaction` 注册信号处理器，再 `ioctl(AXIBRAM_IO_IRQ, 信号号)` 通知驱动安装中断。驱动使用 `request_threaded_irq`，中断到来时**同时**做三件事：

1. **实时信号**：向安装进程发送指定信号（示例用 `SIGRTMIN+2`），`si_code` 携带硬件中断号，可排队、高频基本不丢；
2. **select/poll**：置可读标志并唤醒等待者；
3. **SIGIO**：`kill_fasync` 异步通知（需 `F_SETOWN` + `FASYNC`）。

三种方式对比如下（详见 [axibram_irq.c](axibram/axibram_irq.c) 注释）：

| 特性 | 实时信号 | select/poll | SIGIO |
|---|---|---|---|
| 事件排队 | ✅ 排队 | ——（同步查询） | ❌ 合并丢失 |
| 携带数据 | ✅ `si_code` 带中断号 | ❌ 只有有无 | 弱 |
| 超时/多 fd 复用 | ❌ | ✅ 天然支持 | ❌ |
| 处理上下文 | 信号处理器（受限） | 主流程 | 信号处理器 |

## 7. 工作流程

### 7.1 驱动初始化（probe）

```
platform 总线匹配 compatible
  → axibram_probe
    → kzalloc 设备结构
    → dma_set_mask_and_coherent(32 位)
    → of_device_get_match_data 取设备 ID，生成设备名 axibramN
    → axibram_chdrv_init：alloc/register_chrdev_region、class_create、
      device_create、cdev_add、初始化锁/等待队列/DMA 链表
    → axibram_chdrv_load_resouce：devm_ioremap_resource 映射寄存器，
      读取中断号与触发方式
    → dev_set_drvdata
```

### 7.2 寄存器读写

```
应用 ioctl(READ/WRITE_REGISTER)
  → 驱动 readl/writel(membase + offset)
```
或应用 `mmap`（offset=页）后直接读写映射地址。

### 7.3 DMA 内存

```
应用 mmap(offset=0)
  → 驱动 dma_alloc_coherent + dma_mmap_coherent
  → 挂入 dmabuf_list（支持多块）
  → 应用 munmap / vma close 时 dma_free_coherent 释放
```

驱动提供 `axibram_user_to_kernel` / `axibram_user_to_dma`，遍历 `dmabuf_list` 将用户虚拟地址转换为内核地址 / DMA 总线地址。

### 7.4 中断处理

```
应用 ioctl(AXIBRAM_IO_IRQ, 信号号)
  → 驱动记录进程 + get_task_struct + request_threaded_irq
硬件中断到来
  → 硬中断 irq_interrupt_callback（应答中断源）→ IRQ_WAKE_THREAD
  → 线程 irq_interrupt_thread_callback
      ├─ send_sig_info 实时信号
      ├─ 置 poll 可读 + wake_up
      └─ kill_fasync SIGIO
应用 close / ioctl(IRQ, 0)
  → axibram_uninstall_irq：free_irq + put_task_struct
```

### 7.5 SelectMap 配置

支持两种方式，推荐使用应用层方式：

**方式一：应用层加载（推荐）**

```
应用 ioctl(GET_REG_SIZE) 获取寄存器空间大小
  → mmap(offset=页) 映射 FPGA 寄存器到用户区
  → 读 bitstream 到普通内存
  → 应用层 SelectMap_Cfg_V7 通过 reg_read/reg_write 直接操作寄存器：
    测试寄存器 → 复位 → 分频 → Program_b 启动 → 等 Init_b 拉低 →
    Rdwr_b 写模式 → 写数据（受 Fifo_full 节流）→ 等 FIFO 空 →
    停止 → 等 Done 拉高 → 检查 Init_b
```

**方式二：内核加载（ioctl）**

```
应用读 bitstream 到 mmap DMA 内存
  → ioctl(AXIBRAM_IO_SELECT_MAP)
  → 驱动 user_to_kernel 解析内核地址
  → 内核 SelectMap_Cfg_V7 执行同样的配置时序
```

两种方式的每个等待环节均带超时保护，异常时返回错误码而非死循环。

## 8. 示例程序使用

### 8.1 reg —— 寄存器读写

```bash
./reg.elf read 0          # 读寄存器 0（字地址）
./reg.elf write 0 0x123   # 向寄存器 0 写入 0x123 并回读
```

### 8.2 irq —— 中断通知演示

```bash
./irq.elf [设备名] [方式]
# 方式：1 实时信号（默认） 2 select/poll 3 SIGIO 4 DMA 内存申请
```

### 8.3 StartV7 —— SelectMap 烧写（应用层加载）

将 bitstream 命名为 `k7.bin` 放在与程序相同目录，然后：

```bash
./StartV7.elf
```

程序打开 `/dev/axibram1`，先 `ioctl(GET_REG_SIZE)` 获取寄存器空间大小并 `mmap` 映射到用户区，再在应用层直接操作寄存器完成 SelectMap 配置。

## 9. SelectMap 返回码

`SelectMap_Cfg_V7`（内核与应用层两个版本返回码一致）：

| 返回值 | 含义 |
|---|---|
| 0 | 成功 |
| -1 | 初始化失败（测试寄存器读写不匹配） |
| -2 | 启动失败（Init_b / FIFO / Done 超时） |
| -3 | CRC 校验错误（配置完成后 Init_b 未拉高） |
| -4 | 未知错误 |

## 10. 注意事项

- 设备为 root 独占（`O_EXCL`），同一时刻仅一个进程可打开；
- DMA 内存由 `dma_alloc_coherent` 申请，需内核命令行配置足够的 CMA（如 `cma=64M`）；
- `read`/`write` 目前为空实现，数据交互统一通过 `mmap` + `ioctl` 完成；
- 中断硬处理函数中的「中断源应答/清除」需按实际 FPGA 外设的清除方式（读清/写 1 清）补齐，见 [axibram_irq.c](axibram/axibram_irq.c) 中注释。
