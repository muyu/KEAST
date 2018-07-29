/*********************************************************************************
* Copyright(C),2017-2017,MediaTek Inc.
* FileName:	page.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2017/5/3
* Description: page内存管理系统
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <module.h>
#include "string_res.h"
#include <mrdump.h>
#include "page.h"


static struct
{
	TADDR MemMapAddr;
	TUINT PageSz;
} PageInf;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/

/*************************************************************************
* DESCRIPTION
*	根据page结构体地址转换为va
*
* PARAMETERS
*	PageAddr: page结构体地址
*
* RETURNS
*	va
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR PageAddr2VAddr(TADDR PageAddr)
{
	return ((PageAddr - PageInf.MemMapAddr) / PageInf.PageSz) << PAGE_BITS;
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化page模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: page模块关闭
*	1: page模块可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PageInit(void)
{
	struct _DBG_HDL fDbgHdl, DbgHdl;

	if (GetKExpInf()->mType == MRD_T_NONE)
		return 1;
	if (!SymFile2DbgHdl(pVmSHdl, &fDbgHdl, "page_alloc.c")) {
		if (!DbgGetCDbgHdl(&fDbgHdl, &DbgHdl, "page"))
			PageInf.PageSz = (TUINT)DbgGetByteSz(&DbgHdl);
	}
#ifdef SST64
	//TryMergeVma(0xFFFFFFBC00000000, 0xFFFFFFBE00000000, SetMemName, "vmemmap");
	PageInf.MemMapAddr = 0xFFFFFFBDBF000000;//GetKExpInf()->MemMapAddr;
#else
#endif
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	注销page模块
*
* PARAMETERS
*	无
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PageDeInit(void)
{
	
}
