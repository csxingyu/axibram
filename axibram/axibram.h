/*
 * 湖南迈克森伟电子科技有限公司
 * Copyright (c) 2024
 * All rights reserved.
 *
 * 文件名称：axibram.h
 * 模 块 名：axibram驱动
 * 相关文件：
 * 实现功能：axibram驱动
 *
 * 当前版本：0.0.0.1
 * 作    者：华锋/软件研发部
 * 日    期：2024-05-8
 * 备    注：
 *
 * 修改记录：
 * 日  期          版 本        修改人      修改内容
 */
#if !defined(_include_device_driver_axibram_134d75c_h_)
#define _include_device_driver_axibram_134d75c_h_

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/io.h>
#include <linux/sched/task.h>

#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/dynamic_debug.h>

#define AXIBRAM_VERSION  0x1000001 // V1.0.0.0
// 定义模块驱动名
#define AXIBRAM_DEV_NAME "axibram"
// 定义最大支持驱动数量
#define AXIBRAM_DEVICE_MAX_NUM 18
// 定义最大支持资源数量
#define AXIBRAM_RSOURCE_MAX_NUM 4

// Default minor number for the device
#define MINOR_NUMBER 0
// The default number of character devices
#define NUM_DEVICES 1

// Truncates the full __FILE__ path, only displaying the basename
#define __FILENAME__ \
    (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Convenient macros for printing out messages to the kernel log buffer
#define axibram_log_err(fmt, ...)                               \
    printk(KERN_ERR AXIBRAM_DEV_NAME ": %s: %d: " fmt, __FILENAME__, \
           __LINE__, ##__VA_ARGS__)
#define axibram_log_info(fmt, ...)                               \
    printk(KERN_INFO AXIBRAM_DEV_NAME ": %s: %d: " fmt, __FILENAME__, \
           __LINE__, ##__VA_ARGS__)

typedef struct axibram_resource
{
    unsigned int irqCount; // 中断数量
    unsigned int memCount; // 内存资源数量
    struct
    {
        int          irq;       // 中断号
        unsigned int trigger;   // 中断触发方式
        char         name[16];  // 中断名
    } irq[AXIBRAM_RSOURCE_MAX_NUM];

    struct
    {
        resource_size_t start; // 资源起始地址
        resource_size_t size;  // 资源大小
        void __iomem *membase;
    } mem[AXIBRAM_RSOURCE_MAX_NUM];
} axibram_resource;

// DMA
struct axibram_dma_allocation
{
    size_t size;         // Size of the buffer（内存大小）
    void *user_addr;     // User virtual address of the buffer（用户地址）
    void *kern_addr;     // Kernel virtual address of the buffer（内核地址）
    dma_addr_t dma_addr; // DMA bus address of the buffer（物理地址）
    void *private_data;  // DMA私有数据
    struct list_head list;// List node pointers for allocation list
};

struct axibram_siginfo
{
	struct task_struct *process;
    int  notify_signal; // 信号 
};

typedef struct axibram_device
{
    struct cdev chrdev;           // 字符设备
    struct class *dev_class;      // The device class for the chardevice
    struct device *device;        // Device structure for the char device
    dev_t dev_num;                // 设备的设备编号
    int dev_cnt;                  // 各种axidma设备的数量序号
    unsigned int minor_num;       // 设备的次要编号
    char chrdev_name[10];         // 驱动名
    struct platform_device *pdev; // The platofrm device from the device tree

    axibram_resource resource;    // 驱动资源
    unsigned int poll_reval_mask;
    struct fasync_struct *poll_async;
    wait_queue_head_t poll_wait;
    struct mutex poll_lock; // 用户数据事件锁
    void *private_data;

    struct list_head dmabuf_list;   // mmap 申请的 DMA 内存 list
    struct axibram_dma_allocation *dma_irq_alloc;  // DMA 收发/中断初始化申请的内存

	struct axibram_siginfo   sinfo; // 注册中断信息
    unsigned int irq_installed;     // 中断是否已安装，1：已安装，0：未安装

} axibram_device;

/*
标题：初始化字符设备
参数：
   dev
       [in] 字符设备驱动
   dev_cnt
       [in] 序号值
 返回值：
    返回寄存器的实际值
作    者：
日    期：2024-05-8
备    注：
*/
int axibram_chdrv_init(struct axibram_device *dev, const int dev_cnt);
/*
标题：关闭字符设备驱动
参数：
   dev
       [in] 字符设备驱动
   dev_cnt
       [in] 序号值
 返回值：
    无
作    者：
日    期：2024-05-8
备    注：
*/
void axibram_chdrv_exit(struct axibram_device *dev, const int dev_cnt);
/*
标题：加载资源
参数：
   dev
       [in] 字符设备驱动
 返回值：
    无
作    者：
日    期：2024-05-9
备    注：
*/
int axibram_chdrv_load_resouce(struct axibram_device *dev);
/*
标题：释放资源
参数：
   dev
       [in] 字符设备驱动
 返回值：
    无
作    者：
日    期：2024-05-9
备    注：
*/
void axibram_chdrv_release_resource(struct axibram_device *dev);

/*
标题：发送信号
参数：
   pid
       [in] 接收信号的进程ID
   signo
       [in] 信号名
   code
       [in] 代码，可任意
 返回值：
    无
作    者：
日    期：2024-02-22
备    注：
*/
void axibram_send_signal(int pid, int signo, int code);
void axibram_send_signal_ex(struct task_struct *task, int signo, int code);
/*
标题：中断安装/卸载
参数：
   dev
       [in] 字符设备驱动
 返回值：
    0:成功
作    者：
日    期：2025-05-22
备    注：
*/
int axibram_install_irq(struct axibram_device *dev);
void axibram_uninstall_irq(struct axibram_device *dev);

/*
标题：用户虚拟地址转内核/物理地址
参数：
   dev
       [in] 字符设备驱动
    user_addr
        [in] 用户虚拟地址
    size
        [in] 用户虚拟地址大小
 返回值：
    0:成功
作    者：
日    期：2025-05-22
备    注：
    将给定的用户空间虚拟地址转换为DMA地址，转换失败时返回NULL
*/
void* axibram_user_to_kernel(struct axibram_device *dev, void *user_addr, 
                                size_t size);
dma_addr_t axibram_user_to_dma(struct axibram_device *dev, void *user_addr,
                                size_t size);
#endif


