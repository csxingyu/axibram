/*
 * tx_reg_map_init.h
 *
 *  Created on: 2023��3��22��
 *      Author: Lenovo
 */

#ifndef SRC_BASEBAND_DSSS_TX_REG_MAP_INIT_H_
#define SRC_BASEBAND_DSSS_TX_REG_MAP_INIT_H_


#include "axibram.h"

struct select_map_cfg
{
	/* reg0  */	unsigned int TEST_REG     ;    //测试寄存器1
    /* reg1  */ unsigned int Program_b : 1, R1_NA : 31; // 配置端口，启动配置标志
    /* reg2  */ unsigned int Init_b : 1, R2_NA : 31;    // 配置端口，初始化完成标志，
    /* reg3  */ unsigned int Rdwr_b : 1, R3_NA : 31;    // 配置端口，读写控制位，默认为低，为写数据
    /* reg4  */ unsigned int Done : 1, R4_NA : 31;      // 配置端口，完成状态标志
    /* reg5  */ unsigned int Reset : 1, R5_NA : 31;     // 模块初始状态复位
    /* reg6  */ unsigned int Fs_div : 8, R6_NA : 24;    // 模块发送时钟分频系数
    /* reg7  */ unsigned int Cfg_Start : 1, R7_NA : 31; // 模块发送数据配置开始
    /* reg8  */ unsigned int Cfg_Stop : 1, R8_NA : 31;  // 模块发送数据配置结束
    /* reg9  */ unsigned int Cfg_Data;                  // 配置数据
    /* reg10 */ unsigned int Fifo_count : 16, Fifo_full : 1, Fifo_empty : 1, R10_NA : 14; // 配置数据接口fifo缓存计数/fifo满计数/fifo空计数

} ;

int SelectMap_Cfg_V7(void __iomem *membase, unsigned int *pdata, unsigned int length);

#endif /* SRC_BASEBAND_DSSS_TX_REG_MAP_INIT_H_ */
