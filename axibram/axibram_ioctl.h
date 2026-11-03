/*
 * 湖南迈克森伟电子科技有限公司
 * Copyright (c) 2024
 * All rights reserved.
 *
 * 文件名称：ioctl.h
 * 模 块 名：定义驱动控制命令
 * 相关文件：
 * 实现功能：定义驱动控制命令
 *
 * 当前版本：0.0.0.1
 * 作    者：华锋/软件研发部
 * 日    期：2024-02-20
 * 备    注：
 *
 * 修改记录：
 * 日  期          版 本        修改人      修改内容
 */
#if !defined(_include_axibram_ioctl_h_)
#define _include_axibram_ioctl_h_

#ifdef __KERNEL__
#include <linux/ioctl.h>      /* 内核：_IOR/_IOW 在这里 */
#else
#include <sys/ioctl.h>        /* 用户态：glibc 的 ioctl 宏 */
#endif
//#include <asm/ioctl.h>

struct axibram_register
{
    unsigned int offset; // 要操作寄存器的偏移
    unsigned int data;   // 数据
};

struct axibram_selectmap_transaction
{
    unsigned int  size;
    void         *user_addr;
};



// 定义设备类型
#define AXIBRAM_IOCTL_MAGIC 0x34

// 读取驱动版本号
#define AXIBRAM_IO_VERSION          _IOR(AXIBRAM_IOCTL_MAGIC, 0, unsigned int)
// 读取寄存器值
#define AXIBRAM_IO_READ_REGISTER    _IOR(AXIBRAM_IOCTL_MAGIC, 1, struct axibram_register)
// 向寄存器写入值
#define AXIBRAM_IO_WRITE_REGISTER   _IOW(AXIBRAM_IOCTL_MAGIC, 2, struct axibram_register)

// Select map V7启动
#define AXIBRAM_IO_SELECT_MAP       _IOW(AXIBRAM_IOCTL_MAGIC, 3, struct axibram_selectmap_transaction)

// 安装/卸载中断，0：卸载中断，否则安装中断
#define AXIBRAM_IO_IRQ              _IOW(AXIBRAM_IOCTL_MAGIC, 10, unsigned int)

// DMA 缓冲区物理地址信息（用于配置 FPGA 侧 DMA 引擎）
struct axibram_dma_info
{
    unsigned long long dma_addr;  // DMA 总线地址（本平台 = 物理地址）
    unsigned int       size;      // 缓冲区大小
};

// 获取 mmap DMA 缓冲区的物理地址
#define AXIBRAM_IO_GET_DMA_ADDR     _IOR(AXIBRAM_IOCTL_MAGIC, 11, struct axibram_dma_info)

// RX 环形缓冲控制头（放在 mmap 内存偏移 0，驱动与应用层共享）
struct axibram_rx_ring
{
    volatile unsigned int head;   // 驱动写位置（相对环形区起点）
    volatile unsigned int tail;   // 应用层读位置（相对环形区起点）
};

#endif
