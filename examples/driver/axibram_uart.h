/*
 * driver.h
 *
 *  Created on: 2025年5月7日
 *      Author: root
 */

#ifndef SRC_AXI_DRIVER_SERIAL_134FF8B_H_
#define SRC_AXI_DRIVER_SERIAL_134FF8B_H_

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

#define NONE      0
#define ODD       1
#define EVEN      2
// 打开串口
int  PLUART_Open(const char *pName);
void PLUART_Close(int filp);
/*
标题：设置波特率
参数：
    filp
        [in] UART_Open函数返回的句柄
    BaudRate
        [in] 波特率（分频系数）= 时钟/波特率
返回值：
    无
 描述：
    波特率（分频系数）= 时钟/波特率
    如100M时钟，分频系数 = 100M / 115200 = 868
*/
void PLUART_SetBaudRate(int filp, unsigned int BaudRate);
/*
标题：设置校验
参数：
    filp
        [in] UART_Open函数返回的句柄
    Parity
        [in] 参见NONE、ODD、EVEN定义
返回值：
    无
*/
void PLUART_SetParity(int filp, unsigned int Parity);
/*
 标题：串口是否有数据
 参数：
    filp
        [in] UART_Open函数返回的句柄
 返回值：
    >0：可读数据数量
*/
int  PLUART_IsRead(int filp);

unsigned char PLUART_ReadByte(int filp);
/*
 标题：读串口数据
 参数：
    filp
        [in] UART_Open函数返回的句柄
    pReadBuffer
        [in] 串口数据存储缓存
    length
        [in] 要读取串口数据的长度
 返回值：
    >0：读取数据数量
*/
int PLUART_ReadBytes(int filp, unsigned char *pReadBuffer, int length);

/*
 标题：发送数据
 参数：
    filp
        [in] UART_Open函数返回的句柄
    ch
        [in] 要发送的字节
    txBuf
        [in] 发送数据缓存
    txLength
        [in] 发送数据长度
 返回值：
    无
*/
void PLUART_SendByte(int filp, unsigned char ch);
void PLUART_SendBytes(int filp, unsigned char *txBuf, int txLength);

#endif /* SRC_AXI_DRIVER_SERIAL_134FF8B_H_ */
