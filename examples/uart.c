

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include "driver/axibram_uart.h"
#include "driver/axibram_driver.h"
#include "driver/utils.h"


int GetRandom()
{   
    static int grand_initrandom = 0;
    static unsigned long hash = 0;
    if ( 0 == grand_initrandom )
    {
	    const char *randchars_ = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890";
        for ( int it = 0; it < 62; ++it )
        {
            hash = ((hash << 2) + hash) + randchars_[it];
        }

        hash ^= (unsigned long)time(NULL);
        grand_initrandom = 1;
    }


    return ((hash = hash * 214013L + 2531011L) >> 16) & 0x7fff;
}



void Usage()
{

	printf("Usage:\n" \
           "  uart  <port> <baudrate> <data>  -- port     : uart number rang 0~5.\n\n" \
           "                                  -- baudrate : baudrate.\n\n" \
           "                                  -- data     : assic characters.\r\n" \
           "Examples:\n"
           "  uart  0 115200   -test uart\n");
}

int filp = 0;
void uart_recv_thread(void *lpThreadParameter);
int main(int argc, char *const *argv)
{
	pthread_t threadid = 0;
    unsigned char txBuf[256] = {0};
// axibram串口名
    const char *UartName[32] = {"/dev/axibram3", "/dev/axibram4", "/dev/axibram5", "/dev/axibram7", "/dev/axibram8", "/dev/axibram9"};
    int port = 0;
    int baudrate = 0;
    int iValue = 0;
    int it = 0;
    if ( argc < 4 )
    {
        printf("param failed.\n");
        Usage();
        return 0;
    }

    port     = atol(argv[1]);
    baudrate = atol(argv[2]);
    if ( port > 5 )
    {
        printf("uart number failed.\n");
        Usage();
        return 0;
    } 

    // 打开驱动
    int filp = PLUART_Open(UartName[port]);
    if ( -1 == filp )
    {
        printf("open driver[%s] failed.\r\n", UartName[port]);
        return 0;
    } else
    {
        printf("open driver[%s] success. driver version 0x%x(%d).\r\n",
                   UartName[port], axibram_driver_version(filp), filp);
    }

    printf("baudrate: %d\r\n", baudrate);
    PLUART_SetBaudRate(filp, baudrate);// 设置波特率，115200
   // PLUART_SetParity(filp, ODD);     // 设置偶效验
#if 0
    // 发送随机数后一直接收数据
    printf("send data:\r\n");
    for ( int it = 0; it < 16; ++it )
    {
        txBuf[it] = GetRandom() & 0xFF;
        printf(" 0x%02x", txBuf[it]);
    }
    printf("\r\n");

   
    PLUART_SendBytes(filp, txBuf, 16);
#else
    
    int len = String2Bytes(argv[3], txBuf, strlen(argv[3]));
    printf("\r\nSend %s len: %d\r\n", argv[3], len);
    for ( it = 0; it < len; ++it )
    {
        printf(" %02X", txBuf[it]);
    }

    PLUART_SendBytes(filp, txBuf, len);

  /*  printf("send data:\r\n");
    PLUART_SendBytes(filp, argv[3], strlen(argv[3]));
*/
    printf("\r\n");
#endif

    printf("\r\nrecv data: \r\n");
    while (  1 )
    {
        if ( PLUART_IsRead(filp) > 0 )
        {
            int length = PLUART_ReadBytes(filp, txBuf, 128);
           // txBuf[length] = 0x0;
            for ( int it = 0; it < length; ++it )
            {
                printf(" 0x%02x\r\n", txBuf[it]);
            }
        }
        
      //  printf(txBuf);
      //  printf("\r\n");
        usleep(10);
    }

#if 0
    // 用线程读数据
	pthread_create(&threadid, NULL, uart_recv_thread, (void *)filp);
    int total = 0;
    while ( 1 )
    {
        txBuf[0] = total & 0xFF;
        for ( it = 1; it < 16; ++it, ++iValue )
        {
            txBuf[it] = iValue & 0xFF;
        }

        PLUART_SendBytes(filp, txBuf, 16);
        ++total;
        sleep(1);
    }


 #endif
    PLUART_Close(filp);
    return 0;
}


void uart_recv_thread(void *lpThreadParameter)
{
    int filp = (int)lpThreadParameter;
    unsigned char rxBuf[512] = {0};
    int total = 0;
    int it = 0;
    while (  1 )
    {
        if ( PLUART_IsRead(filp) > 0 )
        {
            int length = PLUART_ReadBytes(filp, rxBuf, 256);
            if ( length > 0 )
            {
                printf("\r\nrecv length = %d\r\n", length);
            }
            for ( it = 0; it < length; ++it )
            {
                if ( total++ % 16 == 0 )
                {
                    printf("\r\n%08x:", total);
                }

                printf(" 0x%02x", rxBuf[it]);
            }
        }

        sleep(1);
    }
}


