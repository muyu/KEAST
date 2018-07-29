/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwaranges.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_aranges解析(支持dwarf2~4)
**********************************************************************************/
#ifndef _DWARANGES_H
#define _DWARANGES_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWAR_HDL
{
	const void *pBase;
	TSIZE Size;
} DWAR_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据地址获取cu off
*
* PARAMETERS
*	pDwarHdl: dwar句柄
*	Addr: 地址, 包含THUMB bit
*
* RETURNS
*	~(TSIZE)0: 没有对应的cu
*	其他: cu off
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DwarGetCuOff(const DWAR_HDL *pDwarHdl, TADDR Addr);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwar句柄
*
* PARAMETERS
*	pDwarHdl: 返回dwar句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_aranges偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwar句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWAR_HDL *GetDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放dwar句柄
*
* PARAMETERS
*	pDwarHdl: dwar句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl);

#endif
