#!/bin/bash

TARGET := axibram

#obj-m += $(OBJS)
obj-m := $(TARGET).o
$(TARGET)-objs := axibram_chrdev.o axi_bram.o

PWD := $(shell pwd)
KDIR :=/home/fmsh/work/FMQL-Linux-SDK-Prj-20230801/linux-4.14.55-fmsh
ECTRA_CFLAGS = -g -O0


ARCH		= arm
CROSS_COMPILE=arm-linux-gnueabihf-

all:
	@echo $(PWD)
	@echo -e "\033[32m Building target:[ $@ - Debug " $(PWD)" ] \033[0m" 
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) modules
	@echo -e "\033[31m Finished building target: $@ \033[0m" 

clean:
	rm -rf $(OBJS) *.o *.ko *.symvers *.order *.mod.c .*.cmd
