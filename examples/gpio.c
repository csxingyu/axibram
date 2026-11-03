

#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include "driver/axibram_gpio.h"


void Usage()
{
	printf("Usage:\n" \
           "  gpio  <pin> \n\n" \
           "Examples:\n" \
           "  gpio   0     -- pin: rang 0~7 gpio, 8: sf card reset\n");
/*
	printf("Usage:\n" \
           "  gpio  <read|write>  <pin> [Data]  -- pin: rang 0~7 gpio, 8: sf card reset\n\n" \
           "Examples:\n" \
           "  gpio  read  0    - read gpio in 0\n" \
           "  gpio  write 0 1  - write 1 to gpio 0\n");
*/
}

int main(int argc, char *const *argv)
{
    if ( argc < 1 )
    {
        Usage();
        return 0;
    }

    // 打开驱动
    int filp = MW_GPIO_Open();
    if ( -1 == filp )
    {
        printf("open driver[/dev/axibram0] failed.\r\n");
        return 0;
    } else
    {
        printf("open driver[/dev/axibram0] success. driver version 0x%x.\r\n",
                   axibram_driver_version(filp));
    }


    unsigned int pin = atoi(argv[1]);

    printf("test gpio %d\r\n", pin);

    MW_GPIO_WritePin(filp, pin, 1);
    sleep(1);
    unsigned nValue[] = {1,0,  1, 0, 1, 0,  0, 1, 0, 1, 1};
    for ( int it = 0; it < 11; ++it )
    {
            MW_GPIO_WritePin(filp, pin, nValue[it]);
            usleep(104);
    }
 
    MW_GPIO_Close(filp);
    return 0;
}


