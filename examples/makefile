SHELL = /bin/bash
# 配置编译环境
ARCH		= arm
CROSS_COMPILE = arm-linux-gnueabihf-
# 允许用户从命令行指定交叉编译
CC = $(CROSS_COMPILE)gcc

# 用于编译的标准gcc标志
GLOBAL_CFLAGS = -Wall -std=gnu99 -g -O0 -g3

#包函目录
#INCS := -I/usr/local/libevent-arm/include
INCS := 
#包函库文件
LIBPATH:= #-L/usr/local/libevent-arm/lib/ 
LIBS   :=  -lm -lpthread -ldl


SOURCES = $(PWD)/driver/axibram_driver.c  $(PWD)/driver/axibram_uart.c $(PWD)/driver/axibram_gpio.c $(PWD)/driver/utils.c
# 目标文件 *.o
OBJS := $(patsubst %.c, %.o, $(SOURCES))
#目标文件 *.d
DEPS := $(patsubst %.o, %.d, $(OBJS))

REG_TARGETS := reg.elf
SELECTMAP_TARGETS := selectmap.elf

.PHONY: all clean 
# 面向用户的驱动程序编译目标
all: $(GPIO_TARGETS) $(REG_TARGETS) ${SELECTMAP_TARGETS} $(OUTPUT_DIR)


#生成目标程序
$(REG_TARGETS):$(OBJS) reg.o
	@echo -e "\033[32m Building target:[ $@ - Debug " $(PWD)" ] \033[0m" 
	-@echo 'Invoking: GCC C Linker'
	$(CC) $(LIBPATH) $(OBJS) reg.o  -o $@ $(LIBS)
	@echo -e "\033[31m Finished building target: $@ \033[0m"

$(SELECTMAP_TARGETS):$(OBJS) selectmap.o
	@echo -e "\033[32m Building target:[ $@ - Debug " $(PWD)" ] \033[0m" 
	-@echo 'Invoking: GCC C Linker'
	$(CC) $(LIBPATH) $(OBJS) selectmap.o  -o $@ $(LIBS)
	@echo -e "\033[31m Finished building target: $@ \033[0m"

# 生成中间链接文件
%.o:%.c
	@echo -e "\033[36m Building file:[ $^ ($<)\033[0m" 
	$(CC) $(GLOBAL_CFLAGS) $(INCS) -c -MT"$@" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"




# 清理编译驱动程序生成的所有文件
clean: 
	rm -f $(OBJS) $(DEPS) *.o *.d *.elf
	
