/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwranges.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_ranges����(֧��dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include "dwranges.h"


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	ָ����ַ�Ƿ���ranges��
*
* PARAMETERS
*	pDwrHdl: dwr���
*	RgeOff: rangeƫ����
*	Addr: ��ַ, ��ȥ����ַ, ����THUMB bit
*
* RETURNS
*	~(TADDR)0: ����
*	����: �ڲ������׵�ַ
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR DwrCheckAddr(const DWR_HDL *pDwrHdl, TSIZE RgeOff, TADDR Addr)
{
	const TADDR *pAddrAry = (void *)((char *)pDwrHdl->pBase + RgeOff);
	int i;

	for (i = 0; pAddrAry[i]|pAddrAry[i + 1]; i += 2) {
		if (pAddrAry[i] == ~(TADDR)0) /* base address!!! */
			DBGERR("�쳣.debug_ranges��ַ: "TADDRFMT"\n", pAddrAry[i]);
		if (Addr >= pAddrAry[i] && Addr < pAddrAry[i + 1])
			return pAddrAry[0];
	}
	return ~(TADDR)0;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡdwr���
*
* PARAMETERS
*	pDwrHdl: ����dwr���
*	pFpHdl: fp���
*	FOff/Size: .debug_rangesƫ�Ƶ�ַ/��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: dwr���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWR_HDL *GetDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwrHdl->Size = Size;
	pDwrHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwrHdl->pBase)
		goto _ERR1;
	return pDwrHdl;
_ERR1:
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_ranges");
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�dwr���
*
* PARAMETERS
*	pDwrHdl: dwr���
*	pFpHdl: fp���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwrHdl->pBase, pDwrHdl->Size);
}
