

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "driver/axibram_driver.h"

unsigned int checksum(void *data, unsigned int size)
{
    unsigned char *pData = (unsigned char *)data;
    unsigned int it = 0;
    unsigned int sum = 0;
    for (it = 0; it < size; ++it )
    {
        sum += pData[it];
    }

    return sum;
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


int FileExists(char *szFileName)
{
    struct stat st;
    if ( stat(szFileName, &st) == 0 )
    {
        return st.st_size;
    }

    return 0;
}

int main(int argc, char *const *argv)
{
    char szFileName[256] = {0};
    char szPath[256] = {0};
    GetModulePath(szPath, 256);

    // 打开驱动
    sprintf(szFileName, "%s/v7_pl_binstream.bin", szPath);
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
    
    int filp = axibram_driver_open("/dev/axibram2");
    if ( -1 == filp )
    {
        printf("open driver[/dev/axibram2] failed.\r\n");
        return 0;
    } else
    {
		unsigned int ver = axibram_driver_version(filp);
    	printf("open selectmap driver[/dev/axibram2] success. driver version V%x.%x.%x.%x.\r\n",
                   (ver >> 24) & 0xFF , (ver >> 16) & 0xFF, (ver >> 8) & 0xFF, ver & 0xFF);
    }


    printf("Starting load selectmap V1 file %s...\r\n", szFileName);

    int ret = axibram_driver_selectmap(filp, szFileName);

    axibram_driver_close(filp);

    printf("Selectmap V7 Bitstream file %s load %s. ret = %d.\r\n"
            , szFileName, (0 == ret ? "succeed" : "failed"), ret);
 
    return 0;
}


