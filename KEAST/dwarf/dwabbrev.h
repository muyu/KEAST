/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwabbrev.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/6
* Description: .debug_abbrev解析(支持dwarf2~4)
**********************************************************************************/
#ifndef _DWABBREV_H
#define _DWABBREV_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWA_HDL
{
	const void *pBase;
	void *pHtblHdl;
	TSIZE Size;
} DWA_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据CuAbbOff/Id获取attr list
*
* PARAMETERS
*	pDwaHdl: dwa句柄
*	pTag: 返回tag
*	CuAbbOff: cu abb偏移量
*	AbbNum: abbrev number
*
* RETURNS
*	NULL: 失败
*	其他: attr list
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *DwaGetAttrList(const DWA_HDL *pDwaHdl, unsigned short *pTag, TSIZE CuAbbOff, TSIZE AbbNum);

/*************************************************************************
* DESCRIPTION
*	添加abb信息到dwa句柄
*
* PARAMETERS
*	pDwaHdl: dwa句柄
*	CuAbbOff: cu abb偏移量
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DwaAddAbbInf(DWA_HDL *pDwaHdl, TSIZE CuAbbOff);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwa句柄
*
* PARAMETERS
*	pDwaHdl: 返回dwa句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_abbrev偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwa句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWA_HDL *GetDwaHdl(DWA_HDL *pDwaHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放dwa句柄
*
* PARAMETERS
*	pDwaHdl: dwa句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwaHdl(DWA_HDL *pDwaHdl, FP_HDL *pFpHdl);

#endif
