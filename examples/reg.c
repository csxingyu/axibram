

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "driver/axibram_driver.h"
#include "driver/utils.h"


void Usage()
{
	printf("Usage:\n" \
           "  reg  <read|write>  <reg> [Data]  -- Data is hex value\n\n" \
           "Examples:\n" \
           "  reg  read  0    - read reg in 0\n" \
           "  reg  write 0 0x123  - write 0x123 to reg 0\n");

}

int main(int argc, char *const *argv)
{
    if ( argc < 3 )
    {
        Usage();
        return 0;
    }

    // 打开驱动
    int filp = axibram_driver_open("/dev/axibram0");
    if ( -1 == filp )
    {
        printf("open driver[/dev/axibram0] failed.\r\n");
        return 0;
    } else
    {
        printf("open driver[/dev/axibram0] success. driver version 0x%x.\r\n",
                   axibram_driver_version(filp));
    }


    unsigned int reg = atoi(argv[2]);
    if ( strcmp(argv[1], "read") == 0 )
    {
        unsigned int value = axibram_driver_readReg(filp, reg * 4);
        printf(" Read Reg%d = 0x%x.\n", reg, value);
    } else if ( strcmp(argv[1], "write")  == 0 ) 
    {
        unsigned int Data = StringToDec((unsigned char*)argv[3]);
        axibram_driver_writeReg(filp, reg * 4, Data);

        unsigned int value = axibram_driver_readReg(filp, reg * 4);
        printf(" Read Reg%d = 0x%x.\n", reg, value);
    }
 
    axibram_driver_close(filp);
    return 0;
}


