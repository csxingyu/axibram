

#include "axibram.h"
#include "axibram_ioctl.h"
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/poll.h>
#include <linux/of_device.h>
#include <linux/dma-buf.h>
#include <linux/interrupt.h>    // 中断
#include <linux/sched/signal.h> // 信号 
#include <linux/irq.h>          // irq_get_trigger_type
#include "axi_selectmap.h"

static struct axibram_device *g_axibram_dev[AXIBRAM_DEVICE_MAX_NUM];


static bool valid_dma_request(void *user_start, size_t dma_size, void *user_addr,
                              size_t user_size)
{
    return user_start <= user_addr &&
           (char *)user_addr + user_size <= (char *)user_start + dma_size;
}

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
日    期：2026-11-3
备    注：
    将给定的用户空间虚拟地址转换为内核地址，转换失败时返回NULL
*/
void* axibram_user_to_kernel(struct axibram_device *dev, void *user_addr,
                                  size_t size)
{
    bool valid;
    unsigned int offset;
    struct list_head *iter;
    struct axibram_dma_allocation *dma_alloc;

    // 遍历当前设备DMA缓存列表，查找与给定用户空间地址匹配的DMA缓存
    list_for_each(iter, &dev->dmabuf_list)
    {
        dma_alloc = container_of(iter, struct axibram_dma_allocation, list);
        valid = valid_dma_request(dma_alloc->user_addr, dma_alloc->size,
                                  user_addr, size);
        if (valid) 
        {
            void *kernel_addr = dma_alloc->kern_addr;
            offset = (unsigned int)(user_addr - dma_alloc->user_addr);
            if ( offset > 0 )
            {
                kernel_addr = (char*)dma_alloc->kern_addr + offset;
            }

            axibram_log_info("axibram user to kernel addr offset: %d, kernel addr:0x%p.\n",
                            offset, kernel_addr);
            return kernel_addr;
        }
    }

    // 未找到匹配的内核缓存
    return NULL;
}

dma_addr_t axibram_user_to_dma(struct axibram_device *dev, void *user_addr,
                                size_t size)
{
    bool valid;
    dma_addr_t offset;
    struct list_head *iter;
    struct axibram_dma_allocation *dma_alloc;

    // 遍历当前设备DMA缓存列表，查找与给定用户空间地址匹配的DMA缓存
    list_for_each(iter, &dev->dmabuf_list)
    {
        dma_alloc = container_of(iter, struct axibram_dma_allocation, list);
        valid = valid_dma_request(dma_alloc->user_addr, dma_alloc->size,
                                  user_addr, size);
        if (valid) 
        {
            dma_addr_t dma_addr = dma_alloc->dma_addr;
            offset = (dma_addr_t)(user_addr - dma_alloc->user_addr);
            if ( offset > 0 )
            {
                dma_addr = dma_alloc->dma_addr + offset;
            }

            axibram_log_info("axibram user to kernel addr offset: %d, phy addr:0x%x.\n",
                            offset, dma_addr);
            return dma_addr;
        }
    }

    // 未找到匹配的内核缓存
    return (dma_addr_t)NULL;
}
//---------------------------------------------------------------------------
//       device resource
//---------------------------------------------------------------------------
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
int axibram_chdrv_load_resouce(struct axibram_device *dev)
{
    int it = 0;
	int irqCount = 0;
    dev->resource.irqCount = 0;
    dev->resource.memCount = 0;

    // 加载内存资源
    for ( it = 0; it < AXIBRAM_RSOURCE_MAX_NUM; ++it )
    {
        struct resource *r_mem; /* IO mem resources */
        r_mem = platform_get_resource(dev->pdev, IORESOURCE_MEM, it);
        dev->resource.mem[it].membase = NULL;
        if (r_mem)
        {
            int index = dev->resource.memCount;
            dev->resource.mem[index].start = r_mem->start;
            dev->resource.mem[index].size = resource_size(r_mem);
            dev->resource.mem[index].membase = devm_ioremap_resource(&dev->pdev->dev, r_mem);
            if ( IS_ERR(dev->resource.mem[index].membase) )
            {
                axibram_log_err("Unable to map registers phy addr = 0x%x, size = 0x%x.\n",
                                dev->resource.mem[index].start, dev->resource.mem[index].size);
                dev->resource.mem[index].membase = NULL;
            }
            else
            {
                dev->resource.memCount += 1;
                axibram_log_info("mmap axibram controller registers phy addr = 0x%x, membase = 0x%p, size = 0x%x.\n",
                                 dev->resource.mem[index].start, dev->resource.mem[index].membase, dev->resource.mem[index].size);
            }
        } else
        {
            if ( 0 == it )
            {
                axibram_log_err("load axibram resource failed, not found memory resource.");  
            }
            
            break;
        }
    }

    // 加载中断资源
	irqCount = platform_irq_count(dev->pdev);
    for ( it = 0; it < AXIBRAM_RSOURCE_MAX_NUM && it < irqCount; ++it )
    {
        struct resource *r_irq; /* Interrupt resources */
        int index = dev->resource.irqCount;
        r_irq = platform_get_resource(dev->pdev, IORESOURCE_IRQ, it);
        dev->resource.irq[index].irq = 0x0;
        if (r_irq)
        {
            dev->resource.irq[index].irq = r_irq->start;
            dev->resource.irqCount += 1;
            if ( r_irq->name )
            {
                strncpy(dev->resource.irq[index].name, r_irq->name, 16);
            }

            // 从设备树读取中断触发方式，作为默认触发方式（应用可用 AXIBRAM_IO_IRQ_TRIGGER 覆盖）
            dev->resource.irq[index].trigger = irq_get_trigger_type(dev->resource.irq[index].irq);
           
            axibram_log_info("IRQ resource found (%s) irq = %d, trigger = %d\r\n"
                            , dev->resource.irq[index].name
                            , dev->resource.irq[index].irq
                            , dev->resource.irq[index].trigger);
        } else
        {
            // 获取中断号
            dev->resource.irq[index].irq = platform_get_irq(dev->pdev, it);
            if ( dev->resource.irq[index].irq > 0 )
            {
                dev->resource.irqCount += 1;
                axibram_log_info("IRQ resource found (0x%x).\r\n", dev->resource.irq[index].irq);
            }
        }
    }

    // IO资源
    for ( it = 0; it < AXIBRAM_RSOURCE_MAX_NUM ; ++it )
    {
        struct resource *r_io; /* Interrupt resources */
        r_io = platform_get_resource(dev->pdev, IORESOURCE_IO, it);
        if (r_io)
        {
            axibram_log_info("IO resource found start=0x%08x - 0x%08x!\r\n", r_io->start, r_io->end);
        }
        else
        {
            break;
        }
    }

    // DMA资源
    for ( it = 0; it < AXIBRAM_RSOURCE_MAX_NUM; ++it )
    {
        struct resource *r_dma; /* Interrupt resources */
        r_dma = platform_get_resource(dev->pdev, IORESOURCE_DMA, it);
        if (r_dma)
        {
            axibram_log_info("DMA resource found start=0x%08x - 0x%08x!\r\n", r_dma->start, r_dma->end);
        }
        else
        {
            break;
        }
    }

    return 0;
}

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
void axibram_chdrv_release_resource(struct axibram_device *dev)
{
    // 因为使用了 devm_ioremap_resource()，所以不需要手动释放内存映射资源
    (void)dev;
}
//---------------------------------------------------------------------------
//      File Operations
//---------------------------------------------------------------------------
static int axibram_open(struct inode *inode, struct file *filp)
{
    struct axibram_device *dev;
    // Only the root user can open this device, and it must be exclusive
    if ( !capable(CAP_SYS_ADMIN) )
    {
        axibram_log_err("Only root can open this device.\n");
        return -EINVAL;
    } else if ( !(filp->f_flags & O_EXCL) ) // 必须使用O_EXCL属性打开
    {
        axibram_log_err("O_EXCL must be specified for open().\n");
        return -EINVAL;
    }

    dev = container_of(inode->i_cdev, struct axibram_device, chrdev);

    filp->private_data = dev;

    return 0;
}

static int axibram_fasync(int fd, struct file *filp, int on)
{
    int err = 0;
    struct axibram_device *dev = filp->private_data;
    err = fasync_helper(fd, filp, on, &dev->poll_async);
    if ( err < 0 )
    {
        axibram_log_err("Axibram fasync error %d.\n", err);
        return -EIO;
    }

    return 0;
}

static int axibram_release(struct inode* inode,struct file* filp)
{
    struct axibram_device *dev = filp->private_data;

    axibram_fasync(-1, filp, 0);
    axibram_uninstall_irq(dev);

    filp->private_data = NULL;
    return 0;
}
/*
标题：设备被read调用，读取数据
参数：
    buf
        [in] 表示用户空间接收数据的缓冲区
    cnt
        [in] 表示请求传输的数据长度
    offt
        [in] 表示从文件中读取数据的起始位置
返回值：
    返回读取的字节数据（长度）

*/
static ssize_t axibram_read(struct file *filp, char __user *user_buf, size_t cnt, loff_t *offt)
{
    struct axibram_device *dev = filp->private_data;
	if ( NULL == dev )
	{
		return -1;
	}

    return 0;
}

/*
标题：设备被write调用，写入数据
参数：
	buf
		[in] 表示来自用户空间的数据缓冲区
	cnt
		[in] 表示请求传输的数据长度
	offt
		[in] 表示数据在文件中应写入的起始位置
返回值：
	返回写入的字节数据（长度）

*/
static ssize_t axibram_write(struct file* filp,const char __user *user_buf, size_t cnt, loff_t* offt)
{
    struct axibram_device *dev = filp->private_data;
	if ( NULL == dev )
	{
		return -1;
	}

    return 0;
}

static void axibram_vma_close(struct vm_area_struct *vma)
{
    struct axibram_dma_allocation *dma_alloc = vma->vm_private_data;
    struct axibram_device *dev = dma_alloc->private_data;


    axibram_log_info("unmmp free address %p to userspace address %p, size %zu.\n",
                    dma_alloc->kern_addr, dma_alloc->user_addr, dma_alloc->size);
    dma_free_coherent(&dev->pdev->dev, dma_alloc->size, dma_alloc->kern_addr,
                    dma_alloc->dma_addr);
    vma->vm_private_data = NULL;
    
    // 从DMA列表中移除 allocation 结构，并释放 allocation 结构的内存
    list_del(&dma_alloc->list);
    kfree(dma_alloc);
}

static const struct vm_operations_struct axibram_vm_ops = {
    .close = axibram_vma_close,
};

// 用于将设备内存映射到进程空间
int axibram_mmap(struct file *filp, struct vm_area_struct * vma)
{
    int rc = 0;
    struct axibram_device *dev = filp->private_data;
    struct axibram_dma_allocation *dma_alloc;
    size_t size = vma->vm_end - vma->vm_start;
    // 获取用户传进来的字节偏移量
    axibram_log_info("mmp remap offset 0x%lx.\n", vma->vm_pgoff);
    if ( 0 != vma->vm_pgoff )
    {
        if ( size > dev->resource.mem[0].size )
        {
            axibram_log_err("Unable to remap address 0x%p to userspace address 0x%p, size %zu.\n",
                           dev->resource.mem[0].membase, (void *)vma->vm_start, size);
            return -EAGAIN;
        } else  if ( dev->resource.mem[0].membase )
        {
            // 设置 VMA 属性标志：I/O 内存、禁止物理页扩展、禁止被 dump 核心转储
            vma->vm_flags |= VM_IO | VM_DONTEXPAND | VM_DONTDUMP;
            // 强制关闭 Cache。寄存器操作必须直接作用于硬件
            vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
            // 建立映射
            rc = remap_pfn_range(vma, vma->vm_start,
                                 (dev->resource.mem[0].start >> PAGE_SHIFT),
                                 size, vma->vm_page_prot);
            if ( rc < 0 )
            {
                axibram_log_err("Unable to remap address 0x%p to userspace address 0x%p, size %zu.\n",
                               dev->resource.mem[0].membase, (void *)vma->vm_start, size);
                return -EAGAIN;
            } else
            {
                axibram_log_info("mmp remap address 0x%p to userspace address 0x%p, size %zu.\n",
                                dev->resource.mem[0].membase, (void *)vma->vm_start, size);
                return 0;
            }
        } else
        {
            axibram_log_err("Unable to remap address 0x%p to userspace address 0x%p, size %zu.\n",
                           dev->resource.mem[0].membase, (void *)vma->vm_start, size);
            return -EAGAIN;
        }
    }
    

    dma_alloc = (struct axibram_dma_allocation *)kmalloc(sizeof(struct axibram_dma_allocation), GFP_KERNEL);
    if (NULL == dma_alloc)
    {
        axibram_log_err("Unable to allocate VMA data structure.\n");
        return -ENOMEM;
    }

    dma_alloc->private_data = dev;
    dma_alloc->size         = size;
    dma_alloc->user_addr    = (void *)vma->vm_start;

    // Configure the DMA device
    of_dma_configure(&dev->pdev->dev, NULL);

    // Allocate the requested region a contiguous and uncached for DMA
    // 内核侧：申请一致性 DMA 内存（ARM 上返回的是 uncached 内核地址）
    dma_alloc->kern_addr = dma_alloc_coherent(&dev->pdev->dev, dma_alloc->size,
                                              &dma_alloc->dma_addr, GFP_KERNEL);
    if ( NULL == dma_alloc->kern_addr )
    {
        axibram_log_err("Unable to allocate contiguous DMA memory region of size "
                       "%zu.\n",
                       dma_alloc->size);
        axibram_log_err("Please make sure that you specified cma=<size> on the "
                        "Kernel command line, and the size is large enouge.\n");
        kfree(dma_alloc);
        return -ENOMEM;
    }

    // Map the region into userspace 将区域映射到用户空间
	// 应用层侧：把这块内存映射给用户态（映射出的也是 uncached 用户地址）
    rc = dma_mmap_coherent(&dev->pdev->dev, vma, dma_alloc->kern_addr,
                           dma_alloc->dma_addr, dma_alloc->size);
    if (rc < 0)
    {
        axibram_log_err("Unable to remap address 0x%p to userspace address 0x%p, size %zu.\n",
                       dma_alloc->kern_addr, dma_alloc->user_addr, dma_alloc->size);
        dma_free_coherent(&dev->pdev->dev, dma_alloc->size,
                      dma_alloc->kern_addr, dma_alloc->dma_addr);
        kfree(dma_alloc);
        return -ENOMEM;
    } else
    {
        axibram_log_info("mmp remap address 0x%p to userspace address 0x%p, size %zu.\n",
                        dma_alloc->kern_addr, dma_alloc->user_addr, dma_alloc->size);
    }

    vma->vm_ops = &axibram_vm_ops;
    vma->vm_private_data = dma_alloc;

    // 如果当前进程被分叉，则不要复制此内存区域。
    vma->vm_flags |= VM_DONTCOPY;

    // Add the allocation to the driver's list of DMA buffers
    list_add(&dma_alloc->list, &dev->dmabuf_list);

    return 0;
}

static long axibram_ioctl(struct file *filp, unsigned int cmd, unsigned long data)
{
    long ret = 0;
    void __user *arg_ptr = (void __user *)data;
    struct axibram_device *dev = (struct axibram_device *)filp->private_data;

    if ( _IOC_TYPE(cmd) != AXIBRAM_IOCTL_MAGIC )
    {
        axibram_log_err("IOCTL command magic number does not match.\n");
        ret = -EFAULT;
    }

    switch ( cmd )
    {
        case AXIBRAM_IO_VERSION:  // 版本号
        {
            unsigned int ver = AXIBRAM_VERSION;
            if ( 0 != copy_to_user(arg_ptr, &ver, sizeof(unsigned int)) )
            {
                ret = -EFAULT;
            }

            break;
        }
        case AXIBRAM_IO_READ_REGISTER: // 读寄存器
        {
            struct axibram_register reg;
            if ( 0 != copy_from_user(&reg, arg_ptr, sizeof(reg)) )
            {
                ret = -EFAULT;
            } else
            {
                reg.data = readl(dev->resource.mem[0].membase + reg.offset);
        	//axibram_log_err("IOCTL read baseAddr=0x%p, offset 0x%x=0x%x.\n", dev->resource.mem[0].membase, reg.offset, reg.data);
                if ( copy_to_user(arg_ptr, &reg, sizeof(reg)) )
                {
                    axibram_log_err("Unable to copy data to userspace for "
                           "AXIBRAM_IO_READ_REGISTER.\n");
                    ret = -EFAULT;
                }
            }

            break;
        }
        case AXIBRAM_IO_WRITE_REGISTER: // 写寄存器
        {
            struct axibram_register reg;
            if ( 0 != copy_from_user(&reg, arg_ptr, sizeof(reg)) )
            {
                ret = -EFAULT;
            } else
            {
        	//axibram_log_err("IOCTL write baseAddr=0x%p, offset 0x%x=0x%x.\n", dev->resource.mem[0].membase, reg.offset, reg.data);
                writel(reg.data, dev->resource.mem[0].membase + reg.offset);
            }

            break;
        }
        case AXIBRAM_IO_SELECT_MAP:
        {
            void *kernel_addr = NULL;
            struct axibram_data_transaction file;
            if ( copy_from_user(&file, arg_ptr, sizeof(file)) != 0 )
            {
                axibram_log_info("Unable to copy transfer info from userspace for AXIBRAM_IO_SELECT_MAP.\n");
                return -EFAULT;
            }

            axibram_log_info("start slectmap addr=%p, size=%d\r\n", file.user_addr, file.size);
            kernel_addr = axibram_user_to_kernel(dev, file.user_addr, file.size);
            if ( kernel_addr )
            {
                SelectMap_Cfg_V7(dev->resource.mem[0].membase, (unsigned int *)kernel_addr, file.size);
            }
            break;
        }
        case AXIBRAM_IO_IRQ: // 安装卸载中断
        {
            axibram_log_err("install irq pid = %ld.\n", data);
            if ( 0x0 == data )
            {
                axibram_uninstall_irq(dev);
            } else
            {
                // 重复安装：先卸载旧的（内部 free_irq 并 put_task_struct 旧进程），
                // 避免 task_struct 引用泄漏，也避免重复 request_irq 返回 -EBUSY。
                if ( dev->irq_installed )
                {
                    axibram_uninstall_irq(dev);
                }

                dev->sinfo.process = get_current();
                get_task_struct(dev->sinfo.process); // 增加引用计数，防止进程先退出后指针悬空
                dev->sinfo.notify_signal = data;
                axibram_log_err("install irq notify_signal=%d.\n", dev->sinfo.notify_signal);

                if ( 0 != axibram_install_irq(dev) )
                {
                    // 安装失败，回退引用计数
                    axibram_uninstall_irq(dev);
                    ret = -EFAULT;
                }
            }


            break;
        }
    }

    return ret;
}


/*
标题：设备被select/poll调用，被动等待
参数：
返回值：
	POLL_OUT | POLLWRNORM可写
	POLL_IN | POLLWRNORM可读
	POLL_OUT | POLL_IN | POLLWRNORM可读写

*/
unsigned int axibram_poll(struct file *filp, struct poll_table_struct *poll_table)
{
    unsigned int masks = 0;
    struct axibram_device *dev = filp->private_data;
    if ( !dev )
        return POLLERR;

    // 注册等待队列，中断到来时 wake_up 会唤醒 select/poll
    poll_wait(filp, &dev->poll_wait, poll_table);

    // 寄存器接口始终可写
    masks |= POLLOUT | POLLWRNORM;

    // 有中断事件则报告可读（边沿触发：每次 select 消费一次事件）
    mutex_lock(&dev->poll_lock);
    if ( dev->poll_reval_mask & POLLIN )
    {
        masks |= POLLIN | POLLRDNORM;
        dev->poll_reval_mask &= ~POLLIN;
    }
    mutex_unlock(&dev->poll_lock);

    return masks;
}

static struct file_operations axibram_file_ops = {
    .owner   = THIS_MODULE,
    .read    = axibram_read,
    .write   = axibram_write,
	.poll    = axibram_poll,
	.mmap    = axibram_mmap,
    .open    = axibram_open,
    .release = axibram_release,
	.fasync  = axibram_fasync,
	.unlocked_ioctl   = axibram_ioctl,
	.compat_ioctl   = axibram_ioctl,
};

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
int axibram_chdrv_init(struct axibram_device *dev, const int dev_cnt)
{
    int rc = 0;
    dev->dev_cnt = dev_cnt;
    dev->sinfo.notify_signal = 0x0;
    if ( 0 == dev_cnt )
    {
        // Allocate a major and minor number region for the character device
        rc = alloc_chrdev_region(&dev->dev_num, 0, 1, AXIBRAM_DEV_NAME);
        if ( rc < 0 )
        {
            axibram_log_err("Unable to allocate charachter device region.\n");
            goto ret;
        }

        // Create a device class for our device
        dev->dev_class = class_create(THIS_MODULE, AXIBRAM_DEV_NAME);
        if ( IS_ERR(dev->dev_class) )
        {
            axibram_log_err("Unable to create device class.\n");
            rc = PTR_ERR(dev->dev_class);
            goto cleanup_chrdev_region;
        }
    } else
    {
        dev->dev_num = MKDEV(MAJOR(g_axibram_dev[0]->dev_num), dev_cnt);
        rc = register_chrdev_region(dev->dev_num, 1, AXIBRAM_DEV_NAME);
        if ( rc < 0 )
        {
            axibram_log_err("Unable to register charachter device region.\n");
            goto ret;
        }

        dev->dev_class = g_axibram_dev[0]->dev_class;
    }

    // Create a device for our module. this will create a file on the filesystem.
    // under "/dev/dev->dev_name"
    dev->device = device_create(dev->dev_class, NULL, dev->dev_num, NULL, dev->chrdev_name);
    if ( IS_ERR(dev->device) )
    {
        axibram_log_err("Unable to create a device.\n");
        rc = PTR_ERR(dev->device);
        goto cleanup_class;
    }

    // register our characher device with the kernel.
    cdev_init(&dev->chrdev, &axibram_file_ops);
    rc = cdev_add(&dev->chrdev, dev->dev_num, 1);
    if ( rc < 0 )
    {
        axibram_log_err("Unable to add a charachter device.\n");
        goto cleanup_device;
    }

    dev->poll_reval_mask = 0x0;
    mutex_init(&dev->poll_lock);
    init_waitqueue_head(&dev->poll_wait);

    // Initialize the list for DMA mmap'ed allocations
    INIT_LIST_HEAD(&dev->dmabuf_list);

    g_axibram_dev[dev_cnt] = dev;  // 成功后才挂进全局数组
    return 0;

cleanup_device:
    device_destroy(dev->dev_class, dev->dev_num);
cleanup_class:
    if ( 0 == dev_cnt ) // 只销毁自己创建的 class
    {
        class_destroy(dev->dev_class);
    }
cleanup_chrdev_region:
    unregister_chrdev_region(dev->dev_num, 1);
ret:
    return rc;
}
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
void axibram_chdrv_exit(struct axibram_device *dev, const int dev_cnt)
{
    cdev_del(&dev->chrdev);
    device_destroy(dev->dev_class, dev->dev_num);
    if ( 0 == dev_cnt )
    {
        class_destroy(dev->dev_class);
    }

    unregister_chrdev_region(dev->dev_num, 1);
    g_axibram_dev[dev->dev_cnt] = NULL;
}
