

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
    dev = dev;
    // 应答/清除中断源：读状态寄存器（多数 FPGA 外设为读即清）。
    // 若不清除，电平触发的中断在重新使能后会立即再次触发，造成中断风暴。
    if ( 29 == irq) // 接收1中断
    {
    } else if ( 30 == irq )// 接收2中断
    {
    } else if ( 31 == irq ) // 发送中断
    {
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
// 中断处理线程
static irqreturn_t irq_interrupt_thread_callback(int irq, void *dev_id)
{
    struct axibram_device *dev = (struct axibram_device *)dev_id;
    if ( 29 == irq) // 接收1中断
    {
    } else if ( 30 == irq)// 接收2中断
    {
    }

    //axibram_log_info("dma irq %d\n", irq);
    // 1、 实时信号（sighandler 参数路径）
    axibram_send_signal_ex(dev->sinfo.process, dev->sinfo.notify_signal, irq);

    // 2、置可读标志并唤醒 select/poll 等待者，配合axibram_poll函数使用
    mutex_lock(&dev->poll_lock);
    dev->poll_reval_mask |= POLLIN;
    mutex_unlock(&dev->poll_lock);
    wake_up_interruptible(&dev->poll_wait);

     // 3、 SIGIO 异步通知（handler 参数路径）
    kill_fasync(&dev->poll_async, SIGIO, POLL_IN); // 通知 SIGIO 异步通知等待者

	return IRQ_RETVAL(IRQ_HANDLED);//IRQ_HANDLED;
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

    // 释放进程引用计数。free_irq 会等待中断线程退出，之后进程指针不再被使用。
    if ( dev->sinfo.process )
    {
        put_task_struct(dev->sinfo.process);
        dev->sinfo.process = NULL;
    }
}

