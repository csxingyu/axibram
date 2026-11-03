版本：V1.0.1
日期：2026-09-27
1、驱动中增加中断

版本：V1.0.0
日期：2025-07-16

1、本程序为bcd驱动和调用例程，编译前需修改axictrl/driver.mk文件，将变量KERNER_DIR值修改为内核实际路径；
2、在当前目录下运行make编译驱动和调用例程，运行make help查看编译帮助；
3、可将axictrl放入linux内核驱动/linux-4.14.55-fmsh/drivers/maxwell目录下，编译内核时将驱动自动编译进内核或手动编译驱动；
4、axictrl放入linux内核驱动进行编译时，请将axictrl目录下的Kconfig和Makefile文件放入/linux-4.14.55-fmsh/drivers/maxwell目录下，
   a、修改/linux-4.14.55-fmsh/drivers/KConfig文件并添加source "drivers/maxwell/Kconfig"编译选项；
   b、修改/linux-4.14.55-fmsh/drivers/Makefile文件添加obj-y  += maxwell/编译选项；
   c、其它根据实际情况进行修改，此处不再详细介绍，请查看复旦微linux移植相关文档。
