/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	dwline.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/22
* Description: .debug_line解析(支持dwarf2~4)
**********************************************************************************/
#ifndef _DWLINE_H
#define _DWLINE_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWL_SEQINF
{
	const void *pSSeq, *pHead;
	TADDR SAddr, EAddr;
} DWL_SEQINF;

typedef struct _DWL_HDL
{
	const void *pBase;
	DWL_SEQINF *pSeqs;
	TSIZE Size;
	unsigned Cnt;
} DWL_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据地址获取所在路径和行号
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	pLineNo: 返回行号
*	Addr: 地址, 包含THUMB bit
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DwlAddr2Line(DWL_HDL *pDwlHdl, TUINT *pLineNo, TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	根据偏移和路径索引获取路径
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	LineOff: line偏移量
*	PathIdx: 路径索引
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DwlIdx2Path(DWL_HDL *pDwlHdl, TSIZE LineOff, unsigned PathIdx);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwl句柄
*
* PARAMETERS
*	pDwlHdl: 返回dwl句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_line偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwl句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWL_HDL *GetDwlHdl(DWL_HDL *pDwlHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放dwl句柄
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwlHdl(DWL_HDL *pDwlHdl, FP_HDL *pFpHdl);

#endif
