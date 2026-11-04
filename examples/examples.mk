
APP_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

#包函目录
#INCS := -I/usr/local/libevent-arm/include
INCS := 
#包函库文件
LIBPATH:= #-L/usr/local/libevent-arm/lib/ 
LIBS   :=  -lm -lpthread -ldl

EX_DIR := app

SOURCES = $(APP_PATH)/driver/axibram_driver.c   $(APP_PATH)/driver/utils.c
# 目标文件 *.o
OBJS := $(patsubst %.c, %.o, $(SOURCES))
#目标文件 *.d
DEPS := $(patsubst %.o, %.d, $(OBJS))


REG_TARGETS := $(APP_PATH)/reg.elf
SELECTMAP_TARGETS := $(APP_PATH)/StartV7.elf
IRQ_TARGETS      := $(APP_PATH)/irq.elf

.PHONY: examples examples_clean

# 面向用户的驱动程序编译目标
examples:  $(REG_TARGETS) ${SELECTMAP_TARGETS} $(IRQ_TARGETS)
	@cp $(REG_TARGETS) $(OUTPUT_DIR)
	@cp $(SELECTMAP_TARGETS) $(OUTPUT_DIR)
	@cp $(IRQ_TARGETS) $(OUTPUT_DIR)

#生成目标程序
$(REG_TARGETS):$(OBJS) $(APP_PATH)/reg.o
	@echo -e "\033[32m Building target:[ $@ - Debug " $(APP_PATH)" ] \033[0m" 
	-@echo 'Invoking: GCC C Linker'
	$(CC) $(LIBPATH) $(OBJS) $(APP_PATH)/reg.o  -o $@ $(LIBS)
	@echo -e "\033[31m Finished building target: $@ \033[0m"

$(SELECTMAP_TARGETS):$(OBJS) $(APP_PATH)/selectmap.o
	@echo -e "\033[32m Building target:[ $@ - Debug " $(APP_PATH)" ] \033[0m"
	-@echo 'Invoking: GCC C Linker'
	$(CC) $(LIBPATH) $(OBJS) $(APP_PATH)/selectmap.o  -o $@ $(LIBS)
	@echo -e "\033[31m Finished building target: $@ \033[0m"

$(IRQ_TARGETS):$(OBJS) $(APP_PATH)/irq.o
	@echo -e "\033[32m Building target:[ $@ - Debug " $(APP_PATH)" ] \033[0m"
	-@echo 'Invoking: GCC C Linker'
	$(CC) $(LIBPATH) $(OBJS) $(APP_PATH)/irq.o  -o $@ $(LIBS)
	@echo -e "\033[31m Finished building target: $@ \033[0m"

# 生成中间链接文件
%.o:%.c
	@echo -e "\033[36m Building file:[ $^ ($<)\033[0m" 
	$(CC) $(GLOBAL_CFLAGS) $(INCS) -c -MT"$@" -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"


# 清理编译驱动程序生成的所有文件
examples_clean: 
	rm -f  $(OBJS) $(DEPS) $(APP_PATH)/*.o $(APP_PATH)/*.d $(APP_PATH)/*.elf
	find $(APP_PATH) -type f \( -name '*.o' -o -name '*.d' -o -name '*.elf' \) -delete
	
