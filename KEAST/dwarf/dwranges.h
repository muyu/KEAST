/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwranges.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_ranges解析(支持dwarf2~4)
**********************************************************************************/
#ifndef _DWRANGES_H
#define _DWRANGES_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWR_HDL
{
	const void *pBase;
	TSIZE Size;
} DWR_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	指定地址是否在ranges里
*
* PARAMETERS
*	pDwrHdl: dwr句柄
*	RgeOff: range偏移量
*	Addr: 地址, 已去除基址, 包含THUMB bit
*
* RETURNS
*	~(TADDR)0: 不在
*	其他: 在并返回首地址
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR DwrCheckAddr(const DWR_HDL *pDwrHdl, TSIZE RgeOff, TADDR Addr);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwr句柄
*
* PARAMETERS
*	pDwrHdl: 返回dwr句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_ranges偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwr句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWR_HDL *GetDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放dwr句柄
*
* PARAMETERS
*	pDwrHdl: dwr句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl);

#endif
