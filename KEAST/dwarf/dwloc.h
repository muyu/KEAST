/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwloc.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_loc解析(支持dwarf2~4)
**********************************************************************************/
#ifndef _DWLOC_H
#define _DWLOC_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWLC_HDL
{
	const void *pBase;
	TSIZE Size;
} DWLC_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据地址和loc偏移量获取expr buffer
*
* PARAMETERS
*	pDwlcHdl: dwlc句柄
*	pSize: 返回buffer大小
*	LocOff: loc偏移量
*	Addr: 地址, 已去除基址, 包含THUMB bit
*
* RETURNS
*	NULL: 失败
*	其他: expr buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *DwlcGetExpr(const DWLC_HDL *pDwlcHdl, TSIZE *pSize, TSIZE LocOff, TADDR Addr);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwlc句柄
*
* PARAMETERS
*	pDwlcHdl: 返回dwlc句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_loc偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwlc句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWLC_HDL *GetDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放dwlc句柄
*
* PARAMETERS
*	pDwlcHdl: dwlc句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl);

#endif
