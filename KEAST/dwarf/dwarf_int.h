/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	dwarf_int.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/23
* Description: dwarf内部头文件
**********************************************************************************/
#ifndef _DWARF_INT_H
#define _DWARF_INT_H

#include <ttypes.h>
#include "dwarf.h"


/*************************************************************************
*=========================== internal section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	提取CIE信息
*
* PARAMETERS
*	pCie: 返回CIE结构体
*	p: CIE buffer
*	pEhAugFunc: .eh_frame扩展字符串解析回调函数
*		pCie: 返回CIE结构体
*		pAug: 扩展字符串
*		p: CIE buffer
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef struct _CIE_INF
{
	unsigned long long AddrOff; /* .eh_frame地址编码所需, 类型长度必须是MAX(sizeof(TADDR), sizeof(void *)) */
	TSIZE CodeAlign;
	TSSIZE DataAlign;
	TUINT RtIdx; /* 返回寄存器索引 */
	const unsigned char *pSInst, *pEInst; /* [起始指令, 结束指令) */
	const char *pName, *pSeg;
	unsigned char HasAug;
	unsigned char Enc; /* .eh_frame地址编码 */
} CIE_INF;
int ExtractCie(CIE_INF *pCie, const void *p, int (*pEhAugFunc)(CIE_INF *pCie, const char *pAug, const void *p));

/*************************************************************************
* DESCRIPTION
*	根据FDE还原1个栈帧
*
* PARAMETERS
*	pFde: FDE结构体
*	pRegAry/pRegMask: 寄存器组/寄存器有效位图(REG_SP/REG_PC必须有效!!!), 会被更新!!!
*	Pc: 不包含基址和THUMB bit的PC
*	pGetTAddrFunc: 获取目标地址回调函数, 返回读取后的buffer
*		pCie: CIE结构体
*		pAddr: 返回目标地址
*		p: buffer
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef struct _FDE_INF
{
	TADDR FAddr; /* 不包含基址和THUMB bit */
	TSIZE Size;
	CIE_INF Cie;
	const unsigned char *pSInst, *pEInst; /* [起始指令, 结束指令) */
} FDE_INF;
int FdeUnwind(const FDE_INF *pFde, TSIZE *pRegAry, TSIZE *pRegMask, TSIZE Pc, const void *(*pGetTAddrFunc)(const CIE_INF *pCie, TADDR *pAddr, const void *p));


/*************************************************************************
* DESCRIPTION
*	解析表达式
*
* PARAMETERS
*	p: 指令buffer
*	Len: 指令长度
*	pRt: 返回计算的结果
*	pRegAry/RegMask: 参考的寄存器组/寄存器有效位图
*	pSegName: 段名称, 如.debug_frame/.debug_loc等
*	pFileName: 文件名
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DwEvalExpr(const void *p, TSIZE Len, TSIZE *pRt, const TSIZE *pRegAry, TSIZE RegMask, const char *pSegName, const char *pFileName);

#endif
