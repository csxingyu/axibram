
#include "axibram_uart.h"
#include "axibram_driver.h"
#include <signal.h>

#define U_BAUDRATE    0x0
#define U_MODE        0x4  // 0:无效验，1：奇效验，2：偶效验
#define U_TXDATA      0x8
#define U_TXEMPTY     0xC
#define U_TXFULL      0x10
#define U_TXNUM       0x14

#define U_RXDATA      0x18
#define U_RXEMPTY     0x1C
#define U_RXFULL      0x20
#define U_RXNUM       0x24

// 打开串口
int PLUART_Open(const char *pName)
{
    return axibram_driver_open(pName);
}

void PLUART_Close(int filp)
{
    axibram_driver_close(filp);
}

/*
标题：设置波特率
参数：
    pUart
        [in] UART_Open函数返回的句柄
    BaudRate
        [in] 波特率（分频系数）= 时钟/波特率
返回值：
    无
 描述：
    波特率（分频系数）= 时钟/波特率
    如100M时钟，分频系数 = 100M / 115200 = 868
*/
void PLUART_SetBaudRate(int filp, unsigned int BaudRate)
{
    axibram_driver_writeReg(filp, U_BAUDRATE, 100000000 / BaudRate);
}
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
void PLUART_SetParity(int filp, unsigned int Parity)
{
    axibram_driver_writeReg(filp, U_MODE, Parity);
}
/*
 标题：串口是否有数据
 参数：
    pUart
        [in] UART_Open函数返回的句柄
 返回值：
    >0：可读数据数量
*/
int  PLUART_IsRead(int filp)
{
//printf("rxNum = %d\r\n", axibram_driver_readReg(filp, U_RXNUM));
    return axibram_driver_readReg(filp, U_RXNUM);
}

unsigned char PLUART_ReadByte(int filp)
{
    return axibram_driver_readReg(filp, U_RXDATA) & 0xFF;
}

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
int PLUART_ReadBytes(int filp, unsigned char *pReadBuffer, int length)
{
    int recvLength = 0;
    //printf("empty = %d\r\n", axibram_driver_readReg(filp, U_RXEMPTY));
    while ( (recvLength < length) && !(axibram_driver_readReg(filp, U_RXEMPTY) & 0x1))
    {
        pReadBuffer[recvLength++] = axibram_driver_readReg(filp, U_RXDATA) & 0xFF;
    }

    return recvLength;
}


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
void PLUART_SendByte(int filp, unsigned char ch)
{
    axibram_driver_writeReg(filp, U_TXDATA, ch);
}

void PLUART_SendBytes(int filp, unsigned char *txBuf, int txLength)
{
    if ( NULL == txBuf || txLength < 1 )
        return;

    while ( txLength > 0 )
    {
        if ( (axibram_driver_readReg(filp, U_TXFULL) & 0x1) )
        {
            usleep(1);
        } else
        {
            axibram_driver_writeReg(filp, U_TXDATA, *txBuf++);
            --txLength;
        }
    }
}


