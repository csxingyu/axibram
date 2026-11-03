

#include "axibram.h"
#include "axibram_ioctl.h"
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/poll.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>
#include <linux/interrupt.h>    // 中断
#include <linux/sched/signal.h> // 信号
#include <linux/dma-mapping.h>  // DMA 连续内存
#include <linux/delay.h>        // udelay/mdelay
#include <linux/string.h>       // memcpy

#define DMA_RX_ADDR1  0x40
#define DMA_RX_ADDR2  0xC0
#define DMA_TX_ADDR   0x00

// DMA 缓冲区大小（字节）
#define DMA_TX_BUF_SIZE   4096   // 发送缓冲区大小
#define DMA_RX_BUF_SIZE   4096   // 单个接收缓冲区大小
#define DMA_RX_BUF_NUM    3      // 每路接收 DMA 的缓冲区数量（轮循）
// mmap 内存（dev->dma_alloc）布局：
// [控制头 16B][TX 8K][RX1 环形区 32K][RX2 环形区 32K]
#define AXIBRAM_RX_CTRL_SIZE   16
#define AXIBRAM_TX_OFFSET      AXIBRAM_RX_CTRL_SIZE
#define AXIBRAM_TX_SIZE        8192
#define AXIBRAM_RX1_OFFSET     (AXIBRAM_TX_OFFSET + AXIBRAM_TX_SIZE)  // 8200
#define AXIBRAM_RX1_SIZE       32768
#define AXIBRAM_RX2_OFFSET     (AXIBRAM_RX1_OFFSET + AXIBRAM_RX1_SIZE)  // 8200 + 32768
#define AXIBRAM_RX2_SIZE       32768

/*
标题：发送信号到应用程序
参数：
   pid
       [in] 接收信号的进程ID
   signum
       [in] 信号名
   code
       [in] 代码，可任意
 返回值：
    无
作    者：
日    期：202４-02-22
备    注：
*/
void axibram_send_signal(int pid, int signo, int code)
{
	struct task_struct *task = NULL;
	if ( 0 == pid )
		return;


	rcu_read_lock();
	task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if ( task )
    {
        get_task_struct(task);  // 增加引用，task 在锁外也能存活
    }
	rcu_read_unlock();

    if ( task )
    {
        axibram_send_signal_ex(task, signo, code);
        put_task_struct(task);
    }
}

void axibram_send_signal_ex(struct task_struct *task, int signo, int code)
{
	struct siginfo sig_info = {0};
	if ( NULL == task )
		return;

	sig_info.si_signo = signo;
	sig_info.si_errno = 0x0;
	sig_info.si_code = code;
    //sig_info.si_int  = code;

	send_sig_info(signo, &sig_info, task);

}

// 硬件中断，如果使用线程中断的话需要返回，IRQ_WAKE_THREAD,否 则返回IRQ_HANDLED
// 中断状态/清除寄存器偏移（相对 membase），用于在硬中断里应答中断源。
// 不同 FPGA 外设的中断清除方式不同（读清零/写1清零），请按实际硬件修改此偏移和读写方式。
static irqreturn_t irq_interrupt_callback(int irq, void *dev_id)
{
    struct axibram_device *dev = (struct axibram_device *)dev_id;

    // 应答/清除中断源：读状态寄存器（多数 FPGA 外设为读即清）。
    // 若不清除，电平触发的中断在重新使能后会立即再次触发，造成中断风暴。
    if ( 29 == irq) // 接收1中断
    {
        writel(0x1, dev->resource.mem[0].membase + DMA_RX_ADDR1 + 0x0C);   // 清 DMA RX 中断（关键！）
        writel(0x1, dev->resource.mem[0].membase + DMA_RX_ADDR1 + 0x24);   // 清接收状态
    } else if ( 30 == irq )// 接收2中断
    {
        writel(0x1, dev->resource.mem[0].membase + DMA_RX_ADDR2 + 0x0C);   // 清 DMA RX 中断（关键！）
        writel(0x1, dev->resource.mem[0].membase + DMA_RX_ADDR2 + 0x24);   // 清接收状态
    } else if ( 31 == irq ) // 发送中断
    {
        writel(0x1, dev->resource.mem[0].membase + DMA_TX_ADDR + 0x10);   // 清 DMA RX 中断（关键！）
    }

	//return IRQ_RETVAL(IRQ_HANDLED);//IRQ_HANDLED;
    return IRQ_WAKE_THREAD; // 交给线程处理
}
/*
	                  实时信号 SIGRTMIN+2	     select/poll	                SIGIO
模型	               异步 push                  同步 pull                      异步 push
事件排队	           ✅ 排队（直到队列上限）     ——（同步查询）                   ❌ 标准信号，不排队
高频中断丢事件          基本不丢	               不丢                     	    会丢（合并）
能携带数据	           ✅ si_code 带中断号        ❌ 只有"有/无"，数据要再读	     弱（只有 band 标志）
处理上下文	           信号处理器（受限）           主流程（无限制）	             信号处理器（受限）
能否带超时/多fd复用	    ❌	                     ✅ 天然支持	                    ❌
安装复杂度	           中                         低                                 高（F_SETOWN + FASYNC 两步）
与主流程并发	       是（需小心同步）             否                              是
*/
// RX 环形缓冲写（驱动侧，生产者）
static void axibram_rx_ring_write(struct axibram_rx_ring *ring, unsigned char *saveBuf, 
                                  unsigned int ring_size,  const void *src, unsigned int len)
{
    unsigned char *buf = saveBuf;
    unsigned int head = ring->head;
    unsigned int tail = ring->tail;
    unsigned int used, free, first;

    used = (head >= tail) ? (head - tail) : ((ring_size - tail) + head);
    free = ring_size - used - 1;  // 保留 1 字节区分满/空

    // 空间不够，丢弃本次所有数据
    if ( len > free )
        return;  // 缓冲区满，丢弃本次数据

    first = ring_size - head;
    if ( len <= first )
    {
        memcpy(buf + head, src, len); // 如果未尾可存放所有数据，则将数据数据存至未尾
    }
    else
    {
        memcpy(buf + head, src, first); // 先将部份数据存至未尾
        memcpy(buf, (const unsigned char *)src + first, len - first); // 再将余下的数据存至环形区起点
    }

    wmb();  // 数据写完后再更新 head
    ring->head = (head + len) % ring_size;
  //  axibram_log_info("head %d\n", ring->head);
}

// 中断处理线程
static irqreturn_t irq_interrupt_thread_callback(int irq, void *dev_id)
{
    struct axibram_device *dev = (struct axibram_device *)dev_id;
    if ( 29 == irq) // 接收1中断
    {
        int index = readl(dev->resource.mem[0].membase + DMA_RX_ADDR1 + 0x44);
        int length = readl(dev->resource.mem[0].membase + DMA_RX_ADDR1 + 0x40);
       // axibram_log_info("index %d, length %d\n", index, length);

        // 环形缓冲写：把 RX1 硬件 DMA 缓冲数据写入 mmap 内存的环形区
        if ( dev->dma_irq_alloc && dev->dma_alloc
             && index >= 0 && index < DMA_RX_BUF_NUM
             && length > 0 && length <= DMA_RX_BUF_SIZE )
        {
            struct axibram_rx_ring *ring = (struct axibram_rx_ring *)dev->dma_alloc->kern_addr;
            unsigned char *buf = dev->dma_alloc->kern_addr + AXIBRAM_RX1_OFFSET;
            unsigned char *src = dev->dma_irq_alloc->kern_addr + index * DMA_RX_BUF_SIZE;
            axibram_rx_ring_write(ring, buf, AXIBRAM_RX1_SIZE, src, length);
        }
    } else if ( 30 == irq)// 接收2中断
    {
        int index = readl(dev->resource.mem[0].membase + DMA_RX_ADDR2 + 0x44);
        int length = readl(dev->resource.mem[0].membase + DMA_RX_ADDR2 + 0x40);
        // 环形缓冲写：把 RX2 硬件 DMA 缓冲数据写入 mmap 内存的环形区
        if ( dev->dma_irq_alloc && dev->dma_alloc
             && index >= 0 && index < DMA_RX_BUF_NUM
             && length > 0 && length <= DMA_RX_BUF_SIZE )
        {
            struct axibram_rx_ring *ring = (struct axibram_rx_ring *)(dev->dma_alloc->kern_addr + sizeof(struct axibram_rx_ring)); // RX2 控制头（RX1 控制头之后）
            unsigned char *buf = dev->dma_alloc->kern_addr + AXIBRAM_RX2_OFFSET;
            unsigned char *src = dev->dma_irq_alloc->kern_addr + DMA_RX_BUF_NUM * DMA_RX_BUF_SIZE + index * DMA_RX_BUF_SIZE;
            axibram_rx_ring_write(ring, buf, AXIBRAM_RX2_SIZE, src, length);
        }
    }

    //axibram_log_info("dma irq %d\n", irq);
    // 1、 实时信号（sighandler 参数路径）
    axibram_send_signal_ex(dev->sinfo.process, dev->sinfo.notify_signal, irq);
#if 0
    // 2、置可读标志并唤醒 select/poll 等待者，配合axibram_poll函数使用
    mutex_lock(&dev->poll_lock);
    dev->poll_reval_mask |= POLLIN;
    mutex_unlock(&dev->poll_lock);
    wake_up_interruptible(&dev->poll_wait);

     // 3、 SIGIO 异步通知（handler 参数路径）
    kill_fasync(&dev->poll_async, SIGIO, POLL_IN); // 通知 SIGIO 异步通知等待者
#endif
	return IRQ_RETVAL(IRQ_HANDLED);//IRQ_HANDLED;
}
void acibram_dma_init_ch(struct axibram_device *dev, unsigned int addr)
{
    // 初始化 接收DMA1 控制器
    // 复位接收DMA1
    writel(0x1, dev->resource.mem[0].membase + addr + 0x00);   // 复位 DMA 控制器
    udelay(50);
    writel(0x0, dev->resource.mem[0].membase + addr + 0x00);   // 取消复位
    // 设置最大接收数据长度
    writel(4096, dev->resource.mem[0].membase + addr + 0x14); 
    // 设置超时
    writel(1000000*10, dev->resource.mem[0].membase + addr + 0x10);

}
void axibram_dma_enable_irq(struct axibram_device *dev, unsigned int addr)
{
    writel(0x1, dev->resource.mem[0].membase + addr + 0x08);   // 接收中断使能
    writel(0x1, dev->resource.mem[0].membase + addr + 0x04);   // 接收DMA使能
}

void axibram_dma_init(struct axibram_device *dev)
{
    struct axibram_dma_allocation *dma_alloc;
    dma_addr_t dma_addr;
    size_t total_size;
    int it;
    struct axibram_rx_ring *ring = (struct axibram_rx_ring *)dev->dma_alloc->kern_addr;
    ring->head = 0;
    ring->tail = 0;

    // 初始化 发送DMA 控制器
    // 复位发送DMA
    writel(0x1, dev->resource.mem[0].membase + 0x00);   // 复位 DMA 控制器
    udelay(10);
    writel(0x0, dev->resource.mem[0].membase + 0x00);   // 取消复位
    // 初始化接收DMA1/2 控制器
    acibram_dma_init_ch(dev, DMA_RX_ADDR1);
    acibram_dma_init_ch(dev, DMA_RX_ADDR2);

    // 申请接收 DMA 连续内存：RX1 3块 + RX2 3块（TX 由应用层 mmap 控制）
    total_size = 2 * DMA_RX_BUF_NUM * DMA_RX_BUF_SIZE;

    // 若已申请过，先释放旧的，避免重复安装中断时泄漏
    if ( dev->dma_irq_alloc )
    {
        dma_free_coherent(&dev->pdev->dev, dev->dma_irq_alloc->size,
                          dev->dma_irq_alloc->kern_addr, dev->dma_irq_alloc->dma_addr);
        kfree(dev->dma_irq_alloc);
        dev->dma_irq_alloc = NULL;
    }

    dma_alloc = kzalloc(sizeof(*dma_alloc), GFP_KERNEL);
    if ( NULL == dma_alloc )
    {
        axibram_log_err("dma init: kzalloc failed\n");
        return;
    }

    dma_alloc->kern_addr = dma_alloc_coherent(&dev->pdev->dev, total_size, &dma_addr, GFP_KERNEL);
    if ( NULL == dma_alloc->kern_addr )
    {
        axibram_log_err("dma init: dma_alloc_coherent(%zu) failed\n", total_size);
        kfree(dma_alloc);
        return;
    }

    dma_alloc->size = total_size;
    dma_alloc->dma_addr = dma_addr;
    dma_alloc->private_data = dev;
    dev->dma_irq_alloc = dma_alloc;

    // 注意：发送 DMA 使用应用层 mmap 申请的内存（零拷贝），驱动把其物理地址写到寄存器 0x18。
    // 要求：应用层必须先 mmap、再安装中断，否则 dev->dma_alloc 为空、跳过 TX 配置。
    if ( dev->dma_alloc )
    {
        writel((u32)((dev->dma_alloc->dma_addr + AXIBRAM_TX_OFFSET) & 0xFFFFFFFF),
               dev->resource.mem[0].membase + DMA_TX_ADDR + 0x18);
    } else
    {
        axibram_log_err("dma init: dma_alloc is NULL, TX addr not configured (mmap first)\n");
    }

    // 接收 DMA1 三个物理地址 -> 寄存器 0x58/0x5C/0x60（0x18 起始连续 3 个）
    for ( it = 0; it < DMA_RX_BUF_NUM; ++it )
    {
        dma_addr_t rx_addr = dma_addr + it * DMA_RX_BUF_SIZE;
        writel((u32)(rx_addr & 0xFFFFFFFF),
               dev->resource.mem[0].membase + DMA_RX_ADDR1 + 0x18 + it * 4);
    }

    // 接收 DMA2 三个物理地址 -> 寄存器 0xD8/0xDC/0xE0（0x18 起始连续 3 个）
    for ( it = 0; it < DMA_RX_BUF_NUM; ++it )
    {
        dma_addr_t rx_addr = dma_addr + DMA_RX_BUF_NUM * DMA_RX_BUF_SIZE + it * DMA_RX_BUF_SIZE;
        writel((u32)(rx_addr & 0xFFFFFFFF),
               dev->resource.mem[0].membase + DMA_RX_ADDR2 + 0x18 + it * 4);
    }

    axibram_log_info("dma init: total=%zu, dma_addr=0x%llx\n",
                     total_size, (unsigned long long)dma_addr);
}
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
int axibram_install_irq(struct axibram_device *dev)
{ 
    int it = 0;
    int irqCount = dev->resource.irqCount;
    axibram_dma_init(dev);
    for ( it = 0; it < irqCount; ++it )
    {
        // 安装中断，使用硬件中断方式
        // IRQF_TRIGGER_RISING   上升沿触
        // IRQF_TRIGGER_FALLING  下降沿触发 
        // IRQF_TRIGGER_HIGH     高电平触发
        // IRQF_TRIGGER_LOW      低电平触发
		// IRQF_ONESHOT：中断处理完后完成才启用中断
        
        //int err = request_irq(dev->resource.irq[it].irq, irq_interrupt_callback,
         //            IRQF_TRIGGER_RISING | IRQF_ONESHOT, AXIBRAM_DEV_NAME, (void*)dev);

        // 申请中断，使用中断线程的方式
        int err = request_threaded_irq(dev->resource.irq[it].irq, irq_interrupt_callback,
                                    irq_interrupt_thread_callback,
                                    dev->resource.irq[it].trigger | IRQF_ONESHOT, AXIBRAM_DEV_NAME, (void*)dev);
        if ( err )
        {
            axibram_log_info("installation interrupted failed %d, err = %d .\r\n", dev->resource.irq[it].irq, err);
            return err;
        } else
        {
            dev->irq_installed |= (1 << it);
		    axibram_log_info("installation interrupted successfully(%d).\r\n", dev->resource.irq[it].irq);
        }

    }


    writel(0x0, dev->resource.mem[0].membase + 0x0C);   // 启用发送中断
    writel(0x1, dev->resource.mem[0].membase + 0x08);   // 启动发送 DMA
    axibram_dma_enable_irq(dev, DMA_RX_ADDR1);
    axibram_dma_enable_irq(dev, DMA_RX_ADDR2);

    return 0;
}

void axibram_uninstall_irq(struct axibram_device *dev)
{
    int it = 0;
    int irqCount = dev->resource.irqCount;

    for ( it = 0; it < irqCount; ++it )
    {
        if ( dev->irq_installed & (1 << it) )
        {
            free_irq(dev->resource.irq[it].irq, dev);
            dev->irq_installed &= ~(1 << it);
            axibram_log_info("uninstall interrupted successfully(%d).\r\n", dev->resource.irq[it].irq);
        }
    }
    
    dev->irq_installed = 0;

    // 释放 DMA 收发/中断初始化时申请的内存
    if ( dev->dma_irq_alloc )
    {
        dma_free_coherent(&dev->pdev->dev, dev->dma_irq_alloc->size,
                            dev->dma_irq_alloc->kern_addr, dev->dma_irq_alloc->dma_addr);
        kfree(dev->dma_irq_alloc);
        dev->dma_irq_alloc = NULL;
    }

    // 释放进程引用计数。free_irq 会等待中断线程退出，之后进程指针不再被使用。
    if ( dev->sinfo.process )
    {
        put_task_struct(dev->sinfo.process);
        dev->sinfo.process = NULL;
    }
}

