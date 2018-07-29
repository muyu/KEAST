/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwloc.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_loc����(֧��dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include <module.h>
#include "dwloc.h"


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��locƫ������ȡexpr buffer
*
* PARAMETERS
*	pDwlcHdl: dwlc���
*	pSize: ����buffer��С
*	LocOff: locƫ����
*	Addr: ��ַ, ��ȥ����ַ, ����THUMB bit
*
* RETURNS
*	NULL: ʧ��
*	����: expr buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *DwlcGetExpr(const DWLC_HDL *pDwlcHdl, TSIZE *pSize, TSIZE LocOff, TADDR Addr)
{
	const TADDR *pAddrAry = (void *)((char *)pDwlcHdl->pBase + LocOff);
	unsigned short Len;

	for (; pAddrAry[0] && pAddrAry[1]; pAddrAry = (void *)((char *)&pAddrAry[2] + 2 + Len)) {
		if (pAddrAry[0] == ~(TADDR)0) /* base address!!! */
			DBGERR("�쳣.debug_loc��ַ: "TADDRFMT"\n", pAddrAry[0]);
		Len = OFFVAL_U2(&pAddrAry[2], 0);
		if (Addr >= pAddrAry[0] && Addr < pAddrAry[1]) {
			*pSize = Len;
			return (char *)&pAddrAry[2] + 2;
		}
	}
	return NULL;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡdwlc���
*
* PARAMETERS
*	pDwlcHdl: ����dwlc���
*	pFpHdl: fp���
*	FOff/Size: .debug_locƫ�Ƶ�ַ/��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: dwlc���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWLC_HDL *GetDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwlcHdl->Size = Size;
	pDwlcHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwlcHdl->pBase)
		goto _ERR1;
	return pDwlcHdl;
_ERR1:
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_loc");
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�dwlc���
*
* PARAMETERS
*	pDwlcHdl: dwlc���
*	pFpHdl: fp���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwlcHdl->pBase, pDwlcHdl->Size);
}
