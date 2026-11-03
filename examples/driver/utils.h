/*
* 湖南数据同步股份有限公司
* Copyright (c) 2013 All rights reserved.
*
* 文件名称：utils.h
* 模 块 名：基本函数封装
* 相关文件：
* 实现功能：基本函数封装
* 
* 当前版本：0.0.0.1
* 作    者：华锋/软件研发部
* 日    期：2011-07-10
* 备    注：
*
* 修改记录：
* 日  期          版 本        修改人      修改内容
*/

#if !defined(_sync_utils_include_132dd7f_h)
#define _sync_utils_include_132dd7f_h


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


// 16进制转10进制
unsigned char HexToDec(const unsigned char ch);
unsigned long StringToDec(const unsigned char *str);
// 十进制转十六进制
unsigned char DecToHex(const unsigned char ch);

/*
 标题：
    将字节数据转换为可打印字符串
 参数：
    pSrc
        [in] 需要转换的字节数据
    pDst
        [in|out] 转换后的字符串
    nSrcLength
        [in] pSrc字符串长度
 返回值：
    返回目标字符串的长度

 作    者：华锋/软件研发部
* 日    期：2025-05-16
 电子邮箱：huafeng_821103@163.com
 备    注：
    {0xC8, 0x32, 0x9B, 0xFD, 0x0E, 0x01} --> "C8329BFD0E01"

 修改记录：
 日  期          版  本        修改人      修改内容
*/
int Bytes2String(const unsigned char *pSrc, char *pDst, int nSrcLength);
/*
 标题：
    将可打印字符串转换为字节数据
 参数：
    pSrc
        [in] 需要转换的字符串
    pDst
        [in|out] 转换后的字节数据
    nSrcLength
        [in] pSrc字符串长度
 返回值：
    返回目标字节数据的长度

 作    者：华锋/软件研发部
* 日    期：2015-05-16
 电子邮箱：huafeng_821103@163.com
 备    注：
    "C8329BFD0E01" --> {0xC8, 0x32, 0x9B, 0xFD, 0x0E, 0x01}

 修改记录：
 日  期          版  本        修改人      修改内容
*/
int String2Bytes(const char *pSrc, unsigned char *pDst, int nSrcLength);

long int GetMicrosecond();     // 微秒
#endif  // _sync_utils_include_132B6CA_D5EE05_h
