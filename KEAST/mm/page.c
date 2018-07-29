/*********************************************************************************
* Copyright(C),2017-2017,MediaTek Inc.
* FileName:	page.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2017/5/3
* Description: page�ڴ����ϵͳ
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
*	����page�ṹ���ַת��Ϊva
*
* PARAMETERS
*	PageAddr: page�ṹ���ַ
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
*	��ʼ��pageģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: pageģ��ر�
*	1: pageģ�����
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
*	ע��pageģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PageDeInit(void)
{
	
}
