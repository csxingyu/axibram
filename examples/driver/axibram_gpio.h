/*
 * driver.h
 *
 *  Created on: 2025年5月7日
 *      Author: root
 */

#ifndef SRC_AXI_DRIVER_GPIO_134FF8B_H_
#define SRC_AXI_DRIVER_GPIO_134FF8B_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <bits/mman.h>
#include <linux/ioctl.h>
#include <sys/ioctl.h>
#include "axibram_driver.h"

/*
 标题：打开/关闭GPIO
 参数：
   filp
        [in] MW_GPIO_Open打开返回的句柄
 返回值：
    返回GPIo句柄
*/
int MW_GPIO_Open();
int MW_GPIO_Close(int filp);
/*
 标题：设置/读取PIN脚数据
 参数：
   filp
        [in] MW_GPIO_Open打开返回的句柄
   Pin
       [in] 要设置的PIN脚，0~5，
        0~4：对应GPIO 0、1、2、3、4共5个GPIO
        5：对应复位GPIO
   Data
       [in] 要设置的数据
 返回值：
    返回GPIO对应的值，1：高电平，0:低电平,-1:错误
*/
unsigned int MW_GPIO_ReadPin(int filp, unsigned int Pin);
void MW_GPIO_WritePin(int filp, unsigned int Pin, unsigned int Data);


#endif /* SRC_AXI_DRIVER_SERIAL_134FF8B_H_ */
