/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	dwframe.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/22
* Description: .debug_frame解析(支持dwarf2~4)
**********************************************************************************/
#ifndef _DWFRAME_H
#define _DWFRAME_H

#include <ttypes.h>

typedef const void * DWF_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	还原1个栈帧
*
* PARAMETERS
*	pDwfHdl: dwf句柄
*	pRegAry/pRegMask: 寄存器组/寄存器有效位图(REG_SP/REG_PC必须有效!!!), 会被更新!!!
*	Pc: 不包含基址和THUMB bit的PC
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DwfUnwind(DWF_HDL *pDwfHdl, TSIZE *pRegAry, TSIZE *pRegMask, TSIZE Pc);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwf句柄
*
* PARAMETERS
*	pFpHdl: fp句柄
*	Off/Size: .debug_frame偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwf句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWF_HDL *GetDwfHdl(const void *pFpHdl, TSIZE Off, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放dwf句柄
*
* PARAMETERS
*	pDwfHdl: dwf句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwfHdl(DWF_HDL *pDwfHdl);

#endif
