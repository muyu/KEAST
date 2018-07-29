/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwabbrev.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/6
* Description: .debug_abbrev����(֧��dwarf2~4)
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
*	�Ա�abb����Ƿ�һ��
*
* PARAMETERS
*	pTblData: ��ϣ������
*	pData: �Ա�����
*
* RETURNS
*	0: һ��
*	����: ��һ��
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
*	����CuAbbOff/Id��ȡattr list
*
* PARAMETERS
*	pDwaHdl: dwa���
*	pTag: ����tag
*	CuAbbOff: cu abbƫ����
*	AbbNum: abbrev number
*
* RETURNS
*	NULL: ʧ��
*	����: attr list
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
*	���abb��Ϣ��dwa���
*
* PARAMETERS
*	pDwaHdl: dwa���
*	CuAbbOff: cu abbƫ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
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
		if (ptAbb != pAbb) { /* ʧ�ܻ���ͬ */
			Free(pAbb);
			return !ptAbb;
		}
		pAbb->pAttr = p = (char *)Getleb128(&Tag, p) + 1; /* ����child */
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
*	��ȡdwa���
*
* PARAMETERS
*	pDwaHdl: ����dwa���
*	pFpHdl: fp���
*	FOff/Size: .debug_abbrevƫ�Ƶ�ַ/��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: dwa���
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
*	�ͷ�dwa���
*
* PARAMETERS
*	pDwaHdl: dwa���
*	pFpHdl: fp���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwaHdl(DWA_HDL *pDwaHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwaHdl->pBase, pDwaHdl->Size);
	PutHtblHdl(pDwaHdl->pHtblHdl);
}
