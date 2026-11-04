
AXIBRAM_PATH := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
# 生成的目标
export TARGET := axibram

# 内核符号文件，用于确定内核是否已更新
KERNEL_PATH = /media/fmsh/work/FMQL-Linux-SDK-Prj-20240923/linux-4.14.55-fmsh
KERNEL_SYMS = $(KERNEL_PATH)/Module.symvers
# 驱动程序的源文件列表。将名称导出到Kbuild文件中。
DRIVER_DIR := axibram
# 需要编译的文件
export BUILD_FILES := axi_bram.c  axi_selectmap.c axibram_chrdev.c axibram_irq.c
DRIVER_PATHS := $(addprefix $(AXIBRAM_PATH)/,$(BUILD_FILES))

# 编译生成的内核对象文件
DRIVER_OBJECT := $(AXIBRAM_PATH)/$(TARGET).ko
DRIVER_OUTPUT_OBJECT := $(OUTPUT_DIR)/$(TARGET).ko

#LIBAXIDMA_INC_DIRS = include
# 包函目录
INC_DIRS := $(AXIBRAM_PATH)/$(LIBAXIDMA_INC_DIRS)


################################################################################
# Targets
################################################################################

.PHONY: driver driver_clean 

# 面向用户的驱动程序编译目标
driver: $(DRIVER_OUTPUT_OBJECT)


$(DRIVER_OBJECT): $(DRIVER_PATHS) $(KERNEL_SYMS)
	@echo -e "\033[32m Building target:[ $@ - Debug " $(AXIBRAM_PATH)" ] \033[0m" 
	$(MAKE) -C $(KERNEL_PATH) M=$(AXIBRAM_PATH) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	@echo -e "\033[31m Finished building target: $@ \033[0m" 

# 将编译后的驱动程序复制到指定的输出目录
$(DRIVER_OUTPUT_OBJECT): $(DRIVER_OBJECT) $(OUTPUT_DIR)
	@cp $< $@

# 清理编译驱动程序生成的所有文件
driver_clean:
	rm -f $(DRIVER_OUTPUT_OBJECT)
	make -C $(KERNEL_PATH) SUBDIRS=$(AXIBRAM_PATH) clean
