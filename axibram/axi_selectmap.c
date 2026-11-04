/*
 * tx_reg_map_init.c
 *
 *  Created on: 2023年3月22日
 *      Author: Lenovo
 */


#include "axi_selectmap.h"
#include "axibram.h"
#include <linux/delay.h>

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
int SelectMap_Cfg_V7(void __iomem *membase, unsigned int *pdata, unsigned int length)
{
    int ret = 0x0;
    unsigned int WordLen = 0 ;
    
    unsigned int Init_b_status = 0;
    unsigned int Done_status= 0 ;
    unsigned int sendcnt = 0 ;
    struct select_map_cfg *SelectMap_cfg_0 = (struct select_map_cfg *)membase;
    unsigned int Wait_cnt = 0;
    WordLen = length >> 2; // 将输入的数据长度由字节统计长度变为 4字节统计长度

    axibram_log_info("membase = 0x%p.\n", membase);
    axibram_log_info("******SelectMap Port Test FileSize = %d******\r\n", length);
    // 向基地址写入一个值然后读出，对比正确性
    writel(0x1ACFFC1D, membase + 0x0);
    SelectMap_cfg_0->TEST_REG = 0x1ACFFC1D;

    // if(SelectMap_cfg_0->TEST_REG == 0x1ACFFC1D)
    axibram_log_info("******SelectMap test 0x%x ******\r\n", readl(membase + 0x0));
    if ( 0x1ACFFC1D == readl(membase + 0x0) )
    {
        axibram_log_info("******SelectMap Port ok ******\r\n");
    }
    else
    {
        axibram_log_info("******SelectMap Port err ******\r\n");
        return -1 ;
    }

    // 上电对模块进行复位操作
    writel(1, membase + 0x14);
   // SelectMap_cfg_0->Reset  = 1 ;
    udelay(1);
    writel(0, membase + 0x14);
    //  SelectMap_cfg_0->Reset  = 0 ;
    axibram_log_info("******SelectMap Reset (pull up -> pull down) ******\r\n");

    //配置启动selectmap接口时钟为主时钟的分频比
    SelectMap_cfg_0->Fs_div = 16 ;
    writel(16, membase + 0x18);

    // 控制Program_b信号，拉高拉低，开始启动配置
    writel(1, membase + 0x4);
    SelectMap_cfg_0->Program_b = 1;
    udelay(100);
    writel(0, membase + 0x4);
    SelectMap_cfg_0->Program_b = 0 ;
    axibram_log_info("******Program_b is pull down ******\r\n");

    //检查Init_b信号，等待信号拉低
    Init_b_status = SelectMap_cfg_0->Init_b ;
    Wait_cnt = 0;
    while((Init_b_status&0x01) == 1)
    {
        if ( Wait_cnt++ > 10000 ) // 约 1s（10000 * 100us）超时保护
        {
            axibram_log_info("****** SelectMap cfg failed, Init_b no low ******\r\n");
            return -2;
        }
        udelay(100); // 延时等待稳定
        Init_b_status = SelectMap_cfg_0->Init_b ;
        axibram_log_info("******Init_b_status = %d******\r\n", Init_b_status);
    }
    axibram_log_info("******Init_b is Low Succeed******\r\n");

    // Init_b信号拉低后表示芯片回应启动，将Program_b拉高开始启动
    writel(1, membase + 0x4);
    SelectMap_cfg_0->Program_b = 1;
    udelay(1);
    axibram_log_info("******Program_b is pull up ******\r\n");

    // 将Rdwr_b信号拉低，控制为写入模式
    writel(0, membase + 0xC);
    SelectMap_cfg_0->Rdwr_b = 0;
    udelay(1);
    axibram_log_info("******Rdwr_b pull down ******\r\n");

    //等待10ms,检测Init_b是否拉高 
    Init_b_status = SelectMap_cfg_0->Init_b ;
    Wait_cnt = 0;
    while((Init_b_status&0x01) == 0 )
    {
        if ( Wait_cnt++ > 100000 ) // 约 1s（100000 * 10us）超时保护
        {
            axibram_log_info("****** SelectMap cfg failed, Init_b no high ******\r\n");
            return -2;
        }
        udelay(10);
        Init_b_status = SelectMap_cfg_0->Init_b ;
        axibram_log_info("****** Init_b_status = %d ******\r\n", Init_b_status);
    }
    axibram_log_info("******Init_b is High Succeed******\r\n");

    // 配置启动底层数据发送接口，开始发送数据
    writel(0x0, membase + 0x1C);
    writel(0x1, membase + 0x1C);
    writel(0x0, membase + 0x1C);
    axibram_log_info("****** Cfg_Start pull up ******\r\n");

    //向数据寄存器中循环写入数据
    Wait_cnt = 0;
    while(WordLen > sendcnt)
    {
        if((SelectMap_cfg_0->Fifo_full & 0x01) == 0)
        {
            writel(pdata[sendcnt], membase + 0x24);
            sendcnt = sendcnt + 1;
            Wait_cnt = 0; // 有进展则重置超时
        }
        else
        {
            if ( Wait_cnt++ > 100000 ) // FIFO 一直满，约 10s 超时保护
            {
                axibram_log_info("****** SelectMap cfg failed, FIFO full timeout ******\r\n");
                return -2;
            }
            udelay(100); // 延时等待稳定
            axibram_log_info("****** write %d %d, 0x%x******\r\n", WordLen, sendcnt, SelectMap_cfg_0->Fifo_full);
            continue;
        }
    }
    
     axibram_log_info("****** wait file empty ***********\r\n");
    //等待底层FIFO数据写入完毕    
    Wait_cnt = 0;
    while((SelectMap_cfg_0->Fifo_empty & 0x01) == 0)
    {
        if ( Wait_cnt++ > 100000 ) // 约 10s 超时保护
        {
            axibram_log_info("****** SelectMap cfg failed, FIFO empty timeout ******\r\n");
            return -2;
        }
        udelay(100); // 延时等待稳定
    }

    axibram_log_info("****** Cfg Data ByteNum = %d ******\r\n", sendcnt * 4);
    axibram_log_info("****** Data Send Done Succeed!!! ******\r\n");
    udelay(100); // 延时等待稳定

    //将Rdwr_b信号拉高，结束写入模式
    SelectMap_cfg_0->Rdwr_b = 1 ;
    axibram_log_info("******Rdwr_b pull up ******\r\n");

    // 配置停止底层数据发送接口
      writel(0x0, membase + 0x20);
    SelectMap_cfg_0->Cfg_Stop = 0;
    udelay(5);
    writel(0x1, membase + 0x20);
    SelectMap_cfg_0->Cfg_Stop = 1;
    udelay(5);
    writel(0x0, membase + 0x20);
    SelectMap_cfg_0->Cfg_Stop = 0;
    udelay(5);

        axibram_log_info("****** Cfg_Stop pull up ******\r\n");

    udelay(500);
    //检查配置完成Done状态    
    Done_status   = SelectMap_cfg_0->Done ;
    axibram_log_info("****** Done_status = %d ******\r\n", Done_status);
    while((Done_status&0x01) == 0)
    {
        udelay(50);
        if(Wait_cnt<=100)
        {
            mdelay(1); // 延时等待稳定
            Wait_cnt = Wait_cnt + 1 ;
            axibram_log_info("******Wait Done pull High******\r\n");
        }
        else
        {
            axibram_log_info("****** SelectMap cfg failed, Done no up ******\r\n");
            ret = -2;
            break;
        }
        
        Done_status   = SelectMap_cfg_0->Done ;
    }

    axibram_log_info("****** SelectMap Cfg Succeed ******\r\n");

    //检查配置完成Init_b状态  
    Init_b_status = SelectMap_cfg_0->Init_b ;    
    if((Init_b_status&0x01) == 0)
    {
        axibram_log_info("****** Init_b no up  ******\r\n");
        axibram_log_info("****** CRC ERR  ******\r\n");
        ret = -3;
    }
    
    if ( ret == 0 )
    {
        if ( 0 == (Done_status&0x01) || 0 == (Init_b_status&0x01) )
        {
            ret = -4;
        }
    }

    axibram_log_info("****** return status %d.  ******\r\n", ret);
    return ret ;
}

