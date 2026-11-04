
#include "axibram_driver.h"
#include <signal.h>



/*
标题：打开axibram驱动
参数：
	pName
		[in] 驱动名称，如："/dev/axibram0"
返回值：
	返回驱动文件描述符
*/
int axibram_driver_open(const char *driverName)
{
    int filp = open(driverName, O_RDWR | O_EXCL);
    if ( -1 == filp )
    {
    	printf("open(\"%s\", O_RDWR | O_EXCL) error\r\n", driverName);
    	return -1;
    } else
    {
		unsigned long ver = axibram_driver_version(filp);
    	printf("open(\"%s\", O_RDWR | O_EXCL)(0x%x) success. driver version V%x.%x.%x.%x.\r\n", driverName, filp,
                   (ver >> 24) & 0xFF , (ver >> 16) & 0xFF, (ver >> 8) & 0xFF, ver & 0xFF);
    }

    return filp;
}
/*
标题：关闭axibram驱动
参数：
	filp
		[in] axibram_driver_open返回的文件描述符
返回值：
	无
*/
void axibram_driver_close(int filp)
{
	if ( 0 < filp )
	{
	    close(filp);
	}
}
/*
标题：申请释放DMA内存
参数:
	filp
		[in] axibram_driver_open返回的文件描述符 
	size
		[in] 要申请或释放内存大小
	dma_addr
		[in] 要释放的dma内存，通过axibram_driver_malloc获取
*/
void *axibram_driver_malloc(int filp, unsigned int size)
{
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, filp, 0);
}

void axibram_driver_free(void *addr, unsigned int size)
{
    munmap(addr, size);
}
// 读取寄存器空间大小
unsigned int axibram_get_reg_size(int filp)
{
    unsigned int size = 0;
    if ( -1 == ioctl(filp, AXIBRAM_IO_GET_REG_SIZE, &size) )
        return 0;

    return size;
}
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
void *axibram_driver_mmap(int filp, unsigned int size)
{
    return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, filp, getpagesize()); 
}

void axibram_driver_munmap(void *addr, unsigned int size)
{
    munmap(addr, size);
}
// 获取驱动版本号
unsigned int axibram_driver_version(int filp) 
{
    unsigned int ver = 1;
    if ( -1 == ioctl(filp, AXIBRAM_IO_VERSION, &ver) )
        return 0;

    return ver;
}
/*
标题：读axibram寄存器
参数：
	filp
		[in] axibram_driver_open返回的文件描述符 
	reg
		[in] 寄存器地址,地址为4的倍数
返回值：
	返回读到的寄存器的值
*/
unsigned int axibram_driver_readReg(int filp, unsigned int reg)
{
    struct axibram_register data;
    data.offset = reg;
    data.data = 0;
    if ( -1 == ioctl(filp, AXIBRAM_IO_READ_REGISTER, &data) )
        return 0;

    return data.data;
}
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
int axibram_driver_writeReg(int filp, unsigned int reg, unsigned int value)
{
    struct axibram_register data;
    data.offset = reg;
    data.data = value;
    if ( 0 == ioctl(filp, AXIBRAM_IO_WRITE_REGISTER, &data) )
    	return 0;
    return -1;
}

/*
标题：注册axibram驱动中断
参数：
	filp
		[in] axibram_driver_open返回的文件描述符
	handler
		[in] SIGIO信号回调函数，暂未用到
	sighandler
		[in] 自定义信号回调函数
返回值：
	返回驱动文件描述符
*/
int axibram_driver_insert_irq(int filp, __sighandler_t handler, void (*sighandler) (int, siginfo_t *, void *))
{
    int rc = 0;
    if ( handler )
    {
    	int flags = 0;
        signal(SIGIO, handler);
    	//将当前进程PID设置为fd文件所对应驱动程序将要发送SIGIO,SIGUSR信号进程PID
        flags = fcntl(filp, F_SETOWN, getpid());
        flags = fcntl(filp, F_GETFL);
        fcntl(filp, F_SETFL, flags | FASYNC); // 进程状态为FASYNC异步通知，，驱动程序中的 fasync 函数就会执行
    }

    if ( sighandler )
    {
    	struct sigaction siga;
		sigemptyset(&siga.sa_mask);
		siga.sa_sigaction = sighandler;
		siga.sa_flags = SA_RESTART | SA_SIGINFO;

		if ( -1 == sigaction(AXIBRAM_IRQ_SIGNAL, &siga, NULL) )
        {
            printf("sigaction error.\r\n");
            rc = -1;
        }

        // 通知驱动安装中断，中断到来时驱动给本进程发实时信号
        if ( -1 == ioctl(filp, AXIBRAM_IO_IRQ, AXIBRAM_IRQ_SIGNAL) )
        {
            printf("ioctl(AXIBRAM_IO_IRQ) error, irq install failed.\r\n");
            rc = -1;
        }

    }

    return rc;
}


/*
标题：selectmap启动
参数：
	filp
		[in] axibram_driver_open返回的文件描述符
	fileName
		[in]
返回值：
	-1：找不到文件
	-2：打开文件失败
	-3：申请内存失败
	-4：读取文件失败
*/
int axibram_driver_selectmap(int filp, const char *fileName)
{
    // 读取文件大小
    int    fileLength = 0;
    struct axibram_data_transaction tran;
    struct stat info;
    int    rc = 0;
    FILE  *fp = 0; // 打开文件句柄

    if (0x0 != stat(fileName, &info))
    {
        printf("read file %s size failed.\n", fileName);
        return -1;
    } else
    {
        printf("read file %s size(%d).\n", fileName, info.st_size);
    }

    fp = fopen(fileName, "rb");
    if ( NULL == fp)
    {
        printf("open file %s failed.\n", fileName);
        return -2;
    } 

    // 映射内存
    tran.user_addr = axibram_driver_malloc(filp, info.st_size + 32);
    if ( NULL == tran.user_addr )
    {
        printf("selectmap malloc memory failed.\n");
        fclose(fp);
        return -3;
    }
	
    tran.size = fread(tran.user_addr, 1, info.st_size, fp);
    fclose(fp);
    if ( tran.size != info.st_size )
    {
        printf("selectmap read file failed.\n");
        axibram_driver_free(tran.user_addr, info.st_size + 32);
        return -4;
    }
	
    rc = ioctl(filp, AXIBRAM_IO_SELECT_MAP, &tran);
  
    axibram_driver_free(tran.user_addr, info.st_size + 32);
    return rc;
}
