/*
    GPIO口对应寄存器，25~32共8个GPIO
    33为SF Card复位 
*/
#include "axibram_gpio.h"
#include <signal.h>
/*
 标题：打开/关闭GPIO
 参数：
   filp
        [in] MW_GPIO_Open打开返回的句柄
 返回值：
    返回GPIo句柄
*/
int MW_GPIO_Open()
{
    return axibram_driver_open("/dev/axibram0");
}
int MW_GPIO_Close(int filp)
{
    axibram_driver_close(filp);
}
/*
 标题：设置/读取PIN脚数据
 参数：
   filp: axibram_driver_open打开返回的句柄
   Pin
       [in] 要设置的PIN脚，0~5，
        0~4：对应GPIO 0、1、2、3、4共5个GPIO
        5：对应复位GPIO
   Data
       [in] 要设置的数据
 返回值：
    返回GPIO对应的值，1：高电平，0:低电平,-1:错误
*/
unsigned int MW_GPIO_ReadPin(int filp, unsigned int Pin)
{
    return axibram_driver_readReg(filp , (Pin + 25) * 4);
}

void MW_GPIO_WritePin(int filp, unsigned int Pin, unsigned int Data)
{
    axibram_driver_writeReg(filp, ((Pin + 25) * 4), Data);
}


