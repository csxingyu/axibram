/*
 * 湖南星语科技有限公司
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

struct axibram_data_transaction
{
    unsigned int  size;
    void         *user_addr;
};



// 定义设备类型
#define AXIBRAM_IOCTL_MAGIC 0x34

// 读取驱动版本号
#define AXIBRAM_IO_VERSION          _IOR(AXIBRAM_IOCTL_MAGIC, 0, unsigned int)
// 读取寄存器值
#define AXIBRAM_IO_READ_REGISTER    _IOWR(AXIBRAM_IOCTL_MAGIC, 1, struct axibram_register)
// 向寄存器写入值
#define AXIBRAM_IO_WRITE_REGISTER   _IOW(AXIBRAM_IOCTL_MAGIC, 2, struct axibram_register)
// Select map V7启动
#define AXIBRAM_IO_SELECT_MAP       _IOW(AXIBRAM_IOCTL_MAGIC, 3, struct axibram_data_transaction)
// 获取寄存器空间大小
#define AXIBRAM_IO_GET_REG_SIZE     _IOR(AXIBRAM_IOCTL_MAGIC, 4, unsigned int)
// 安装/卸载中断，0：卸载中断，否则安装中断
#define AXIBRAM_IO_IRQ              _IOW(AXIBRAM_IOCTL_MAGIC, 10, unsigned int)


#endif
