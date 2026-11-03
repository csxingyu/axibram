/*
* 广州市XX通信科技有限公司
* Copyright (c) 2010 
* All rights reserved.
* 
* 文件名称：utils.h
* 模 块 名：基本函数封装
* 相关文件：
* 实现功能：基本函数封装
* 
* 当前版本：0.0.0.1
* 作    者：华锋/软件研发部
* 日    期：2011-07-910
* 备    注：
*
* 修改记录：
* 日  期          版 本        修改人      修改内容
*/
#include "utils.h"
#include <time.h>

/*
 标题：
	16进制转10进制
 参数：
    ch
        [in] 待转换的16进制数
 返回值：
    返回转换后的10进制数

 作    者：华锋/软件研发部
 日    期：2010-06-16
 备    注：
 
 修改记录：
 日  期          版 本        修改人      修改内容
*/

unsigned char HexToDec(const unsigned char ch)
{
    if ( ch >= '0' && ch <= '9' )
        return (unsigned char)(ch - '0');

    if ( ch >= 'A' && ch <= 'F' )
        return (unsigned char)(ch - 'A' + 10);

    if ( ch >= 'a' && ch <= 'f' )
        return (unsigned char)(ch - 'a' + 10);

    return 0;
}

unsigned long StringToDec(const unsigned char *str)
{
    if ( NULL == str )
        return 0;

    char szBuf[32] = {0};
    int pos = 0x0;
    int len = strlen((const char*)str);
    for ( int it = 0; it < len  && it < 30; ++it )
    {
        if ( str[it] != ' ' )
        {
            szBuf[pos++] = str[it];
        }
    }

    szBuf[pos++] = 0x0;

    char *strValue = szBuf;
    unsigned long long value = 0x0;
    if ( strValue[0] == '0' && (strValue[1] == 'x' || strValue[1] == 'X') )
    {
        strValue += 2;
    }

    len = strlen(strValue);
    for ( int it = 0; it < len; ++it )
    {
        value = (value << 4) | HexToDec(strValue[it]);
    }

    return (unsigned long)value;
}

unsigned char DecToHex(const unsigned char ch)
{
    static const unsigned char HEX[] = "0123456789ABCDEF";
    return (ch < 16) ? HEX[ch] : '!';
}


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
int Bytes2String(const unsigned char *pSrc, char *pDst, int nSrcLength)
{
    int it = 0;
    const char tab[] = "0123456789ABCDEF";  // 0x0-0xf的字符查找表
    if ( pSrc == NULL || pDst == NULL )
        return 0;

    for ( it = 0; it < nSrcLength; ++it )
    {
        *pDst++ = tab[*pSrc >> 4];    // 输出高4位
        *pDst++ = tab[*pSrc & 0x0f];  // 输出低4位
        ++pSrc;
    }

    *pDst = '\0';

    return (nSrcLength * 2);

}

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
int String2Bytes(const char *pSrc, unsigned char *pDst, int nSrcLength)
{
    if ( pSrc == NULL || pDst == NULL )
        return 0;


    for ( int i = 0; i < nSrcLength; i += 2 )
    {
        *pDst = HexToDec(*pSrc++) << 4;  // 输出高4位
        *pDst |= HexToDec(*pSrc++); // 输出低4位

        ++pDst;
    }

    // 返回目标长度
    return (nSrcLength / 2);
}

long int GetMicrosecond()     // 微秒
{
    long int sec = 0;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

    sec = ts.tv_sec;
    sec *= 1000000;
    sec += ts.tv_nsec / 1000;
    return sec;
}

