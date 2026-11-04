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

#ifndef SRC_AXI_DRIVER_1348C64_H_
#define SRC_AXI_DRIVER_1348C64_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <bits/mman.h>
#include "../../axibram/axibram_ioctl.h"

// 中断通知信号：使用实时信号（可排队），高频中断下不会像标准信号那样丢通知。
// 注意：glibc/NPTL 保留 SIGRTMIN 和 SIGRTMIN+1，应用层应从 SIGRTMIN+2 起使用。
#define AXIBRAM_IRQ_SIGNAL       (SIGRTMIN + 2)


/*
标题：打开axibram驱动
参数：
	pName
		[in] 驱动名称，如："/dev/axibram0"
返回值：
	返回驱动文件描述符
*/
int axibram_driver_open(const char *pName);

/*
标题：关闭axibram驱动
参数：
	fd
		[in] axibram_driver_open返回的文件描述符
返回值：
	无
*/
void axibram_driver_close(int filp);

/*
标题：申请释放DMA内存
参数:
	fd
		[in] axibram_driver_open返回的文件描述符 
	size
		[in] 要申请或释放内存大小
	dma_addr
		[in] 要释放的dma内存，通过axibram_driver_malloc获取
*/
void *axibram_driver_malloc(int filp, unsigned int size);
void axibram_driver_free(void *dma_addr, unsigned int size);
// 读取寄存器空间大小
unsigned int axibram_get_reg_size(int filp);
/*
标题：映射寄存器内存
参数:
	fd
		[in] axibram_driver_open返回的文件描述符 
	size
		[in] 要申请或释放内存大小
	addr
		[in] 要释放的dma内存，通过axibram_driver_mmap获取
*/
void *axibram_driver_mmap(int filp, unsigned int size);
void axibram_driver_munmap(void *addr, unsigned int size);

// 获取驱动版本号
unsigned int axibram_driver_version(int filp);

/*
标题：读axibram寄存器
参数：
	fd
		[in] axibram_driver_open返回的文件描述符 
	reg
		[in] 寄存器地址,地址为4的倍数
返回值：
	返回读到的寄存器的值
*/
unsigned int axibram_driver_readReg(int filp, unsigned int reg);

/*
标题：写axibram寄存器
参数：
	filp
		[in] axibram_driver_open返回的文件描述符 
	reg
		[in] 寄存器地址,地址为4的倍数
	value
		[in] 要写入寄存器的值
返回值：
	无
*/
int axibram_driver_writeReg(int filp, unsigned int reg, unsigned int value);

/*
标题：注册axibram驱动中断
参数：
	filp
		[in] axibram_driver_open返回的文件描述符
	handler
		[in] SIGIO信号回调函数，为NULL则不使用标准SIGIO通知
	sighandler
		[in] 自定义信号回调函数，为NULL则不安装中断
返回值：
	0：成功，-1：失败
*/
int axibram_driver_insert_irq(int filp, __sighandler_t handler, void (*sighandler) (int, siginfo_t *, void *));

/*
标题：selectmap启动
参数：
	filp
		[in] axibram_driver_open返回的文件描述符
	fileName
		[in]
	reg
		[in] 寄存器地址,地址为4的倍数
	value
		[in] 要写入寄存器的值
返回值：
	无
*/
int axibram_driver_selectmap(int filp, const char *fileName);


#endif /* SRC_AXI_DRIVER_H_ */
