

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "driver/axibram_driver.h"
#if 0
// select map 寄存器结构定义
struct select_map_cfg
{
	/* reg0  */	unsigned int TEST_REG     ;    //测试寄存器1
    /* reg1  */ unsigned int Program_b : 1, R1_NA : 31; // 配置端口，启动配置标志
    /* reg2  */ unsigned int Init_b : 1, R2_NA : 31;    // 配置端口，初始化完成标志，
    /* reg3  */ unsigned int Rdwr_b : 1, R3_NA : 31;    // 配置端口，读写控制位，默认为低，为写数据
    /* reg4  */ unsigned int Done : 1, R4_NA : 31;      // 配置端口，完成状态标志
    /* reg5  */ unsigned int Reset : 1, R5_NA : 31;     // 模块初始状态复位
    /* reg6  */ unsigned int Fs_div : 8, R6_NA : 24;    // 模块发送时钟分频系数
    /* reg7  */ unsigned int Cfg_Start : 1, R7_NA : 31; // 模块发送数据配置开始
    /* reg8  */ unsigned int Cfg_Stop : 1, R8_NA : 31;  // 模块发送数据配置结束
    /* reg9  */ unsigned int Cfg_Data;                  // 配置数据
    /* reg10 */ unsigned int Fifo_count : 16, Fifo_full : 1, Fifo_empty : 1, R10_NA : 14; // 配置数据接口fifo缓存计数/fifo满计数/fifo空计数

} ;
#endif
unsigned int reg_read(unsigned char *base, unsigned int offset)
{
    return *(volatile unsigned int *)(base + offset);
}

void reg_write(unsigned char *base, unsigned int offset, unsigned int value)
{
    *(volatile unsigned int *)(base + offset) = value;
}
/*
标题：SelectMap V7更新
参数：
    membase
        [in] V7地址，由FPGA给出
    pdata
        [in] 发送给V7的程序
    length
        [in] pdata数据文件长度
返回值：
     0：成功
    -1：初始化失败，可能是FPGA程序问题
    -2：V7程序启动失败，可能效验错误，FPGA程序问题等均有可能
    -3：文件CRC效验错误
    -4：未知错误
*/
int SelectMap_Cfg_V7(unsigned char *membase, unsigned int *pdata, unsigned int length)
{
    int ret = 0x0;
    unsigned int WordLen = 0 ;
    
    unsigned int Init_b_status = 0;
    unsigned int Done_status= 0 ;
    unsigned int sendcnt = 0 ;
    unsigned int Wait_cnt = 0;
    WordLen = length >> 2; // 将输入的数据长度由字节统计长度变为 4字节统计长度

    printf("membase = %p.\n", membase);
    printf("******SelectMap Port Test FileSize = %d******\r\n", length);
    // 向基地址写入一个值然后读出，对比正确性
    reg_write(membase, 0x0, 0x1ACFFC1D);
    printf("******SelectMap test 0x%x ******\r\n", reg_read(membase, 0x0));
    if ( 0x1ACFFC1D == reg_read(membase, 0x0) )
    {
        printf("******SelectMap Port ok ******\r\n");
    }
    else
    {
        printf("******SelectMap Port err ******\r\n");
        return -1 ;
    }

    // 上电对模块进行复位操作
    reg_write(membase, 0x14, 0x1);
    usleep(1);
    reg_write(membase, 0x14, 0x0);
    printf("******SelectMap Reset (pull up -> pull down) ******\r\n");

    //配置启动selectmap接口时钟为主时钟的分频比
    reg_write(membase, 0x18, 16);

    // 控制Program_b信号，拉高拉低，开始启动配置
    reg_write(membase, 0x4, 1);
    usleep(100);
    reg_write(membase, 0x4, 0);    
	usleep(100);
    printf("******Program_b is pull down ******\r\n");

    //检查Init_b信号，等待信号拉低
    Init_b_status = reg_read(membase, 0x8);
    Wait_cnt = 0;
    while((Init_b_status & 0x01) == 1)
    {
        if ( Wait_cnt++ > 10000 ) // 约 1s（10000 * 100us）超时保护
        {
            printf("****** SelectMap cfg failed, Init_b no low ******\r\n");
            return -2;
        }
        usleep(100); // 延时等待稳定
        Init_b_status = reg_read(membase, 0x8);
        printf("******Init_b_status = %d******\r\n", Init_b_status);
    }
    printf("******Init_b is Low Succeed******\r\n");

    // Init_b信号拉低后表示芯片回应启动，将Program_b拉高开始启动
    reg_write(membase, 0x4, 1);
    usleep(10);
    printf("******Program_b is pull up ******\r\n");

    // 将Rdwr_b信号拉低，控制为写入模式
    reg_write(membase, 0xC, 0);
    usleep(1000);
    printf("******Rdwr_b pull down ******\r\n");

    //等待10ms,检测Init_b是否拉高 
    Wait_cnt = 0;
    Init_b_status = reg_read(membase, 0x8);
    while((Init_b_status&0x01) == 0 )
    {
        if ( Wait_cnt++ > 2000 ) // 约 1s（1000 * 1000us）超时保护
        {
            printf("****** SelectMap cfg failed, Init_b no high ******\r\n");
            return -2;
        }
        usleep(1000);
        printf("****** Init_b_status = %d ******\r\n", Init_b_status);
        Init_b_status = reg_read(membase, 0x8);
    }
    printf("******Init_b is High Succeed******\r\n");

    // 配置启动底层数据发送接口，开始发送数据
    reg_write(membase, 0x1C, 0);
    usleep(1000);
    reg_write(membase, 0x1C, 1);
    usleep(1000);
    reg_write(membase, 0x1C, 0);
    usleep(100);
    printf("****** Cfg_Start pull up ******\r\n");

    //向数据寄存器中循环写入数据
    Wait_cnt = 0;
    while(WordLen > sendcnt)
    {
        Init_b_status = reg_read(membase, 0x28) >> 16;
        if((Init_b_status & 0x01) == 0)
        {
            reg_write(membase, 0x24, pdata[sendcnt]);
            sendcnt = sendcnt + 1;
            Wait_cnt = 0; // 有进展则重置超时
        }
        else
        {
            if ( Wait_cnt++ > 100000 ) // FIFO 一直满，约 10s 超时保护
            {
                printf("****** SelectMap cfg failed, FIFO full timeout ******\r\n");
                return -2;
            }
            usleep(100); // 延时等待稳定
            printf("****** write %d %d, fifo=0x%x******\r\n", WordLen, sendcnt, Init_b_status);
        }
    }
    
     printf("****** wait file empty ***********\r\n");
    //等待底层FIFO数据写入完毕    
    Wait_cnt = 0;
    Init_b_status = reg_read(membase, 0x28) >> 17;
    while((Init_b_status & 0x01) == 0)
    {
        if ( Wait_cnt++ > 100000 ) // 约 10s 超时保护
        {
            printf("****** SelectMap cfg failed, FIFO empty timeout ******\r\n");
            return -2;
        }
        usleep(100); // 延时等待稳定
		Init_b_status = reg_read(membase, 0x28) >> 17;
    }

    printf("****** Cfg Data ByteNum = %d ******\r\n", sendcnt * 4);
    printf("****** Data Send Done Succeed!!! ******\r\n");
    usleep(100); // 延时等待稳定

    //将Rdwr_b信号拉高，结束写入模式
	reg_write(membase, 0xC, 1);
    printf("******Rdwr_b pull up ******\r\n");

    // 配置停止底层数据发送接口
	reg_write(membase, 0x20, 0);
    usleep(5);
	reg_write(membase, 0x20, 1);
    usleep(5);
	reg_write(membase, 0x20, 0);
    usleep(5);

        printf("****** Cfg_Stop pull up ******\r\n");

    usleep(500);
    //检查配置完成Done状态    
    Done_status   = reg_read(membase, 0x10);
    printf("****** Done_status = %d ******\r\n", Done_status);
    while((Done_status&0x01) == 0)
    {
        usleep(50);
        if(Wait_cnt<=100)
        {
            usleep(1000); // 延时等待稳定
            Wait_cnt = Wait_cnt + 1 ;
            printf("******Wait Done pull High******\r\n");
        }
        else
        {
            printf("****** SelectMap cfg failed, Done no up ******\r\n");
            ret = -2;
            break;
        }
        
        Done_status   = reg_read(membase, 0x10);
    }

    printf("****** SelectMap Cfg Succeed ******\r\n");

    //检查配置完成Init_b状态  
    Init_b_status = reg_read(membase, 0x8);   
    if((Init_b_status&0x01) == 0)
    {
        printf("****** Init_b no up  ******\r\n");
        printf("****** CRC ERR  ******\r\n");
        ret = -3;
    }
    
    if ( ret == 0 )
    {
        if ( 0 == (Done_status&0x01) || 0 == (Init_b_status&0x01) )
        {
            ret = -4;
        }
    }

    printf("****** return status %d.  ******\r\n", ret);
    return ret ;
}

// 获取当前程序完整路径
int GetModulePath(char *lpszPath, int size)
{
    int len = readlink("/proc/self/exe", lpszPath, size);
    if ( len == -1 )
        return -1;
    
    while ( len > 0 && lpszPath[len] != '/' )
    {
        lpszPath[len--] = 0x0;
    }

    lpszPath[len--] = 0x0;
    return len;
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
int axibram_selectmap(int filp, const char *fileName)
{
    // 读取文件大小
    struct axibram_data_transaction tran;
    struct stat info;
    int    rc = 0;
    FILE  *fp = 0; // 打开文件句柄
    if (0x0 != stat(fileName, &info))
    {
        printf("load selectmap V7 Bitstream file %s failed.\n", fileName);
        return -1;
    } else
    {
        printf("Starting load selectmap V7 Bitstream file %s. Size %d Bytes.\r\n",
                 fileName, info.st_size);
    }

    fp = fopen(fileName, "rb");
    if ( NULL == fp)
    {
        printf("open file %s failed.\n", fileName);
        return -2;
    } 

    // 获取硬件寄存器空间大小及寄存器地址
    unsigned int size = axibram_get_reg_size(filp);
    if ( 0 == size )
    {
        printf("get reg size failed.\n");
        fclose(fp);
        return -3;
    }
    void *pPhy = axibram_driver_mmap(filp, size);
    if ( MAP_FAILED == pPhy )
    {
        printf("selectmap mmap memory failed.\n");
        fclose(fp);
        return -3;
    } else
    {
        printf("selectmap mmap memory success, phy addr:0x%p.\n", pPhy);
    }
    // 申请内存用于存放读取的文件数据
    tran.user_addr = malloc(info.st_size + 32);
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
        free(tran.user_addr);
        return -4;
    }
	
    rc = SelectMap_Cfg_V7((unsigned char*)pPhy, (unsigned int *)tran.user_addr, tran.size);
  
    axibram_driver_munmap(pPhy, size);
    free(tran.user_addr);
    return rc;
}

int main(int argc, char *const *argv)
{
    char szFileName[256] = {0};
    char szPath[256] = {0};
    GetModulePath(szPath, 256);

    // 打开驱动
    sprintf(szFileName, "%s/k7.bin", szPath);
    struct stat info;
    if ( 0 != stat(szFileName, &info) )
    {
        printf("V7 Bitstream file %s not found.\r\n", szFileName);
        return 0;
    }  else
    {
        printf("Starting load selectmap V7 Bitstream file %s. Size %d Bytes.\r\n",
                 szFileName, info.st_size);
    }
    
    int filp = axibram_driver_open("/dev/axibram1");
    if ( -1 == filp )
    {
        printf("open driver[/dev/axibram1] failed.\r\n");
        return 0;
    } else
    {
		unsigned int ver = axibram_driver_version(filp);
    	printf("open selectmap driver[/dev/axibram1] success. driver version V%x.%x.%x.%x.\r\n",
                   (ver >> 24) & 0xFF , (ver >> 16) & 0xFF, (ver >> 8) & 0xFF, ver & 0xFF);
    }


    printf("Starting load selectmap V1 file %s...\r\n", szFileName);
    int ret = axibram_selectmap(filp, szFileName);
    axibram_driver_close(filp);

    printf("Selectmap V7 Bitstream file %s load %s. ret = %d.\r\n"
            , szFileName, (0 == ret ? "succeed" : "failed"));
 
    return 0;
}


