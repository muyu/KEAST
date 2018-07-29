/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	armdis.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/11/13
* Description: arm反汇编
**********************************************************************************/
#ifndef _ARMDIS_H
#define _ARMDIS_H

#include <stdio.h>
#include <ttypes.h>

typedef enum _EX_TYPE
{
	EX_FAIL,	 /* 失败 */
	EX_NORMAL,   /* 正常, 其他进程或HW引起 */
	EX_INST_EA,  /* 错误的PC地址(地址无效/未对齐/无执行权限) */
	EX_INST_UND, /* 未定义指令 */
	EX_DATA_UNA, /* 地址未对齐 */
	EX_DATA_EAR, /* 在错误的地址读(地址无效/无读权限) */
	EX_DATA_EAW, /* 在错误的地址写(地址无效/无写权限) */
} EX_TYPE;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	显示函数的异常相关反汇编指令
*
* PARAMETERS
*	fp: 文件句柄
*	FAddr/Size: 函数起始地址
*	pRegAry/RegMask: 寄存器组/寄存器有效位图
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispFuncAsm(FILE *fp, TADDR FAddr, const TSIZE *pRegAry, TSIZE RegMask);

/*************************************************************************
* DESCRIPTION
*	获取异常信息
*
* PARAMETERS
*	pVal: 返回存取的数据或指令(为EX_NORMAL时)
*	pFaultAddr: 返回异常地址
*	pRegAry/Cpsr: 异常时刻的寄存器信息
*
* RETURNS
*	异常类型
*
* LOCAL AFFECTED
*
*************************************************************************/
EX_TYPE GetExInf(TADDR *pFaultAddr, TSIZE *pVal, const TSIZE *pRegAry, TSIZE Cpsr);

/*************************************************************************
* DESCRIPTION
*	寄存器反向推导，用于推导函数参数
*
* PARAMETERS
*	FAddr: 函数地址
*	pRegAry: 寄存器信息，会被更新
*	RegMask: 寄存器有效位图
*	TarMask: 预期达到的有效位图
*
* RETURNS
*	更新后的有效位图
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE RegDerivation(TADDR FAddr, TSIZE *pRegAry, TSIZE RegMask, TSIZE TarMask);

#endif
