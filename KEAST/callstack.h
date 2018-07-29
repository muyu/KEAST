/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	callstack.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: 调用栈还原
**********************************************************************************/
#ifndef _CALLSTACK_H
#define _CALLSTACK_H

#include <stdio.h>
#include <ttypes.h>
#include <symfile.h>


#define REG_ALLMASK	 (((TSIZE)1 << REG_NUM) - 1)

typedef const struct _CS_PARA
{
	BASE_VAL Val;
	const char *pName; /* 可能为NULL */
	BASE_TYPE Type;
} CS_PARA;

typedef const struct _CS_FRAME
{
	const struct _CS_FRAME *pNext;
	TADDR FAddr; /* 函数头地址，为0则表示扩展帧, 包含THUMB bit */
	TUINT Off, LineNo; /* 偏移量(字节)/行号 */

#define CS_MNAME	(1 << 0) /* 需要释放pMName */
#define CS_EXCP		(1 << 1) /* 异常帧(irq,abort) */
#define CS_INLINE	(1 << 2) /* 表示这帧是inline */
	unsigned Flag, ArgCnt;
	const char *pMName; /* 内存区域名 */
	const char *pFName; /* 函数名，可能为NULL */
	const char *pPath; /* 代码路径，可能为NULL */
	CS_PARA Para[0]; /* 参数内容 */
} CS_FRAME, CS_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	显示调用栈
*
* PARAMETERS
*	fp: 文件句柄
*	pCsHdl: cs句柄
*	pHeadStr: 输出头信息
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispCallStack(FILE *fp, CS_HDL *pCsHdl, const char *pHeadStr);

/*************************************************************************
* DESCRIPTION
*	获取本地调用栈, 注意: pRegAry[REG_SP],pRegAry[REG_PC]必须设置正确的值!!!
*
* PARAMETERS
*	pRegAry: 寄存器组(REG_NUM个寄存器)
*	Flag: 信息开关
*		bit0: 显示文件名/行号, 参数
*		bit1: 显示inline函数
*
* RETURNS
*	NULL: 失败
*	其他: 调用栈，不用时需调用PutCsHdl()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
CS_HDL *GetNativeCsHdl(const TSIZE *pRegAry, int Flag);

/*************************************************************************
* DESCRIPTION
*	根据LR数组建立本地调用栈
*
* PARAMETERS
*	pBts/Cnt: LR数组/个数
*
* RETURNS
*	NULL: 失败
*	其他: 调用栈，不用时需调用PutCsHdl()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
CS_HDL *Bt2NativeCsHdl(const TSIZE *pBts, TUINT Cnt);

/*************************************************************************
* DESCRIPTION
*	释放cs句柄
*
* PARAMETERS
*	pCsHdl: cs句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutCsHdl(CS_HDL *pCsHdl);

/*************************************************************************
* DESCRIPTION
*	获取第1帧异常帧的寄存器信息, 用于mrdump调用栈!
*
* PARAMETERS
*	pRegAry: 返回寄存器组(REG_NUM个寄存器)
*
* RETURNS
*	寄存器有效位图, 0表示失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE GetFirstExcpCntx(TSIZE *pRegAry);

#endif
