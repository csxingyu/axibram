/*
 * 中断示例程序：演示三种中断通知方式
 *
 * 用法：./irq.elf [设备名] [方式] [触发方式]
 *   设备名：默认 /dev/axibram0（须是设备树里配置了中断的 axibramN 设备）
 *   方式：
 *     1 —— 实时信号（sighandler），默认
 *     2 —— select/poll
 *     3 —— SIGIO 异步通知（handler）
 *     4 —— DMA 物理地址获取
 *     5 —— DMA 发送数据
 *   触发方式（可省，默认上升沿；可传十进制或 0x 十六进制）：
 *     0x1 上升沿  0x2 下降沿  0x4 高电平  0x8 低电平
 *
 * 说明：
 *   1. 中断必须先安装，走 axibram_driver_insert_irq 的 sighandler 参数，
 *      内部会 ioctl(AXIBRAM_IO_IRQ) 通知驱动安装硬件中断。
 *   2. 驱动在每次中断到来时会同时做三件事：发实时信号、唤醒 select/poll、
 *      发 SIGIO；因此三种方式可任选其一，也可组合使用。
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/select.h>
#include "driver/axibram_driver.h"

/* 中断计数与最近一次硬件中断号（异步信号安全） */
static volatile sig_atomic_t g_irq_count  = 0;
static volatile sig_atomic_t g_irq_number = 0;


/*
 * 实时信号处理器（SA_SIGINFO 三参形式）。
 * 驱动在中断线程里 send_sig_info 时，把硬件中断号放进了 si_code。
 */
static void rt_signal_handler(int signo, siginfo_t *info, void *context)
{
    (void)signo;
    (void)context;

    g_irq_number = info->si_code;   // 硬件中断号
    g_irq_count++;
}

/* 空操作处理器：仅用于“装中断但不关心信号”的场景（方式 2、3） */
static void dummy_handler(int signo, siginfo_t *info, void *context)
{
    (void)signo;
    (void)info;
    (void)context;
}

/* SIGIO 处理器（方式 3，简单 signal() 单参形式） */
static void sigio_handler(int signo)
{
    (void)signo;
    g_irq_count++;
}

/* 方式 1：实时信号 */
static void demo_signal(const char *devName)
{
    int filp = axibram_driver_open(devName);
    if ( -1 == filp )
    {
        printf("open driver[%s] failed.\n", devName);
        return;
    }

    if ( -1 == axibram_driver_insert_irq(filp, NULL, rt_signal_handler) )
    {
        printf("install irq failed.\n");
        axibram_driver_close(filp);
        return;
    }

    printf("[方式1 实时信号] 等待中断 (Ctrl-C 退出)...\n");
    long last = 0;
    while ( 1 )
    {
        pause();   // 阻塞等待信号，处理器执行完后 pause 返回

        if ( g_irq_count != last )
        {
            printf("收到实时信号: 硬件中断号=%d 累计=%ld\n",
                   (int)g_irq_number, (long)g_irq_count);
            last = g_irq_count;
        }
    }

    axibram_driver_close(filp);
}

/* 方式 2：select/poll */
static void demo_poll(const char *devName)
{
    int filp = axibram_driver_open(devName);
    if ( -1 == filp )
    {
        printf("open driver[%s] failed.\n", devName);
        return;
    }

    if ( -1 == axibram_driver_insert_irq(filp, NULL, dummy_handler) )
    {
        printf("install irq failed.\n");
        axibram_driver_close(filp);
        return;
    }

    printf("[方式2 select] 等待中断 (Ctrl-C 退出)...\n");
    while ( 1 )
    {
        fd_set rfds;
        struct timeval tv = { 5, 0 };
        FD_ZERO(&rfds);
        FD_SET(filp, &rfds);

        int ret = select(filp + 1, &rfds, NULL, NULL, &tv);
        if ( ret < 0 )
        {
            if ( EINTR == errno )
                continue;   /* 被信号打断，重新等 */
            perror("select");
            break;
        }
        else if ( 0 == ret )
        {
            printf("select 超时（5 秒内无中断）\n");
        }
        else if ( FD_ISSET(filp, &rfds) )
        {
            printf("select 返回可读（有中断到来）\n");
        }
    }

    axibram_driver_close(filp);
}

/* 方式 3：SIGIO 异步通知 */
static void demo_sigio(const char *devName)
{
    int filp = axibram_driver_open(devName);
    if ( -1 == filp )
    {
        printf("open driver[%s] failed.\n", devName);
        return;
    }

    if ( -1 == axibram_driver_insert_irq(filp, sigio_handler, dummy_handler) )
    {
        printf("install irq failed.\n");
        axibram_driver_close(filp);
        return;
    }

    printf("[方式3 SIGIO] 等待中断 (Ctrl-C 退出)...\n");
    long last = 0;
    while ( 1 )
    {
        pause();   // SIGIO 到来时 sigio_handler 执行，pause 返回

        if ( g_irq_count != last )
        {
            printf("收到 SIGIO: 累计=%ld\n", (long)g_irq_count);
            last = g_irq_count;
        }
    }

    axibram_driver_close(filp);
}

/* 方式 4：DMA 物理地址获取 */
static void demo_dma(const char *devName)
{
    int filp = axibram_driver_open(devName);
    if ( -1 == filp )
    {
        printf("open driver[%s] failed.\n", devName);
        return;
    }

    // 1. 申请 DMA 内存（驱动 dma_alloc_coherent + dma_mmap_coherent）
    unsigned int size = 4096;
    void *buf = axibram_driver_malloc(filp, size);
    if ( NULL == buf )
    {
        printf("dma malloc failed.\n");
        axibram_driver_close(filp);
        return;
    }

    // 2. 获取 DMA 物理地址（用于配置 FPGA 侧 DMA 引擎）
    unsigned int actualSize = 0;
    unsigned long long phy = axibram_driver_get_dma_addr(filp, &actualSize);
    if ( 0 == phy )
    {
        printf("get dma addr failed.\n");
        axibram_driver_free(buf, size);
        axibram_driver_close(filp);
        return;
    }

    printf("[方式4 DMA] 虚拟地址=%p 物理地址=0x%llx 大小=%u\n",
           buf, phy, actualSize);

    // 3. 写点数据进缓冲区（dma_alloc_coherent 是非缓存一致内存，设备能直接看到）
    unsigned int *p = (unsigned int *)buf;
    for ( unsigned int i = 0; i < size / 4; ++i )
        p[i] = i;
    printf("已写缓冲区前 4 个字：0x%08x 0x%08x 0x%08x 0x%08x\n",
           p[0], p[1], p[2], p[3]);

    // 4. 把物理地址写进 FPGA 的 DMA 地址寄存器（寄存器偏移按实际硬件修改）
    // axibram_driver_writeReg(filp, DMA_ADDR_REG,  (unsigned int)phy);
    // axibram_driver_writeReg(filp, DMA_SIZE_REG,  actualSize);
    // axibram_driver_writeReg(filp, DMA_CTRL_REG,  1);  // 启动 DMA

    axibram_driver_free(buf, size);
    axibram_driver_close(filp);
}

/* FPGA DMA 引擎寄存器偏移（字节，4 的倍数） */
#define DMA_REG_RESET     0x00   // 写 1 复位
#define DMA_REG_START     0x04   // 写 1 启动数据发送
#define DMA_REG_ENABLE    0x08   // 写 1 使能 DMA
#define DMA_REG_IRQ_EN    0x0C   // 写 1 使能中断
#define DMA_REG_IRQ_CLR   0x10   // 写 1 清除中断
#define DMA_REG_LENGTH    0x14   // 要发送的数据长度
#define DMA_REG_ADDR      0x18   // DMA 的 DDR 物理地址
#define DMA_REG_STATUS    0x20   // FPGA 中 DMA 状态（读）

/* DMA FIFO 满度描述（状态寄存器低 2 位） */
static const char *dma_fifo_desc(unsigned int status)
{
    switch ( status & 0x3 )
    {
        case 0: return "不满 1/4";
        case 1: return "1/4 满";
        case 2: return "1/2 满";
        case 3: return "3/4 满";
        default: return "未知";
    }
}

/* 方式 5：DMA 发送数据 */
static void demo_dma_send(const char *devName)
{
    int filp = axibram_driver_open(devName);
    if ( -1 == filp )
    {
        printf("open driver[%s] failed.\n", devName);
        return;
    }

    // 1. 申请 DMA 内存并填入要发送的数据
    unsigned int length = 1024;   // 要发送的字节数
    unsigned char *buf = (unsigned char *)axibram_driver_malloc(filp, length);
    if ( NULL == buf )
    {
        printf("dma malloc failed.\n");
        axibram_driver_close(filp);
        return;
    }
    for ( unsigned int i = 0; i < length; ++i )
        buf[i] = (unsigned char)(i & 0xFF);

    // 2. 取 DMA 物理地址
    unsigned int sz = 0;
    unsigned long long phy = axibram_driver_get_dma_addr(filp, &sz);
    if ( 0 == phy )
    {
        printf("get dma addr failed.\n");
        axibram_driver_free(buf, length);
        axibram_driver_close(filp);
        return;
    }

    printf("[方式5 DMA发送] 长度=%u 物理地址=0x%llx\n", length, phy);

    // 3. 复位 DMA（写 1 复位，再写 0 结束复位）
    axibram_driver_writeReg(filp, DMA_REG_RESET, 1);
    usleep(100);
    axibram_driver_writeReg(filp, DMA_REG_RESET, 0);

    // 4. 使能 DMA
    axibram_driver_writeReg(filp, DMA_REG_ENABLE, 1);

    // 5. 写 DDR 物理地址 + 数据长度
    axibram_driver_writeReg(filp, DMA_REG_ADDR, (unsigned int)phy);
    axibram_driver_writeReg(filp, DMA_REG_LENGTH, length);

    // 6. 若用中断通知完成，使能中断并注册中断处理器：
    //    axibram_driver_writeReg(filp, DMA_REG_IRQ_EN, 1);
    //    axibram_driver_insert_irq(filp, NULL, rt_signal_handler);

    // 7. 启动数据发送
    axibram_driver_writeReg(filp, DMA_REG_START, 1);

    // 8. 轮询 DMA FIFO 状态（观察发送过程中的 FIFO 满度）
    for ( int i = 0; i < 10; ++i )
    {
        unsigned int status = axibram_driver_readReg(filp, DMA_REG_STATUS);
        printf("  DMA FIFO 状态[%d] = %s\n", i, dma_fifo_desc(status));
        usleep(10000);   // 10ms
    }

    // 9. 若使用中断，发送完成后清除中断标志
    axibram_driver_writeReg(filp, DMA_REG_IRQ_CLR, 1);

    axibram_driver_free(buf, length);
    axibram_driver_close(filp);
}

int main(int argc, char *const *argv)
{
    const char *devName = "/dev/axibram0";
    int mode = 1;

    if ( argc > 1 )
        devName = argv[1];
    if ( argc > 2 )
        mode = atoi(argv[2]);

    switch ( mode )
    {
        case 2:  demo_poll(devName);  break;
        case 3:  demo_sigio(devName); break;
        case 4:  demo_dma(devName);   break;
        case 5:  demo_dma_send(devName); break;
        case 1:
        default: demo_signal(devName); break;
    }

    return 0;
}
