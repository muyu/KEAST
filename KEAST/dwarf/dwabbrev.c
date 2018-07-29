/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwabbrev.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/6
* Description: .debug_abbrev解析(支持dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include <module.h>
#include "dwarf_int.h"
#include "dwabbrev.h"


#define INIT_ABB_CNT	256

typedef struct _ABB_INT
{
	TSIZE Id;
	const void *pAttr;
	unsigned short Tag;
} ABB_INT;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	对比abb句柄是否一样
*
* PARAMETERS
*	pTblData: 哈希表数据
*	pData: 对比数据
*
* RETURNS
*	0: 一样
*	其他: 不一样
*
* LOCAL AFFECTED
*
*************************************************************************/
static int AbbItemCmp(void *pTblData, void *pData)
{
	return ((ABB_INT *)pTblData)->Id != ((ABB_INT *)pData)->Id;
}


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
const void *DwaGetAttrList(const DWA_HDL *pDwaHdl, unsigned short *pTag, TSIZE CuAbbOff, TSIZE AbbNum)
{
	ABB_INT Abb, *pAbb;

	Abb.Id = CuAbbOff + AbbNum;
	pAbb = HtblLookup(pDwaHdl->pHtblHdl, (unsigned)Abb.Id, &Abb, AbbItemCmp, 0);
	if (pAbb) {
		*pTag = (short)pAbb->Tag;
		return pAbb->pAttr;
	}
	return NULL;
}

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
int DwaAddAbbInf(DWA_HDL *pDwaHdl, TSIZE CuAbbOff)
{
	const void *p = (char *)pDwaHdl->pBase + CuAbbOff;
	TSIZE AbbNum, Attr, Form, Tag;
	ABB_INT *pAbb, *ptAbb;

	p = Getleb128(&AbbNum, p);
	while (AbbNum) {
		pAbb = Malloc(sizeof(*pAbb));
		if (!pAbb) {
			SSTWARN(RS_OOM);
			return 1;
		}
		pAbb->Id = CuAbbOff + AbbNum;
		ptAbb = HtblLookup(pDwaHdl->pHtblHdl, (unsigned)pAbb->Id, pAbb, AbbItemCmp, 1);
		if (ptAbb != pAbb) { /* 失败或相同 */
			Free(pAbb);
			return !ptAbb;
		}
		pAbb->pAttr = p = (char *)Getleb128(&Tag, p) + 1; /* 跳过child */
		pAbb->Tag = (short)Tag;
		do {
			p = Getleb128(&Attr, p);
			p = Getleb128(&Form, p);
		} while (Attr);
		p = Getleb128(&AbbNum, p);
	}
	return 0;
}


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
DWA_HDL *GetDwaHdl(DWA_HDL *pDwaHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwaHdl->pHtblHdl = GetHtblHdl(INIT_ABB_CNT, Free);
	if (!pDwaHdl->pHtblHdl)
		goto _ERR1;
	pDwaHdl->Size = Size;
	pDwaHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwaHdl->pBase)
		goto _ERR2;
	return pDwaHdl;
_ERR2:
	PutHtblHdl(pDwaHdl->pHtblHdl);
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_abbrev");
	return NULL;
_ERR1:
	SSTWARN(RS_OOM);
	return NULL;
}

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
void PutDwaHdl(DWA_HDL *pDwaHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwaHdl->pBase, pDwaHdl->Size);
	PutHtblHdl(pDwaHdl->pHtblHdl);
}
