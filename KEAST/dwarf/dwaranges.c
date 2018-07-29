/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwaranges.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_aranges����(֧��dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include <module.h>
#include "dwaranges.h"


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡaranges head��Ϣ
*
* PARAMETERS
*	pARgeh: aranges head
*	ppAddrAry: ���ص�ַ�б�, ΪNULL��ʾ��aranges head�쳣!
*	pCuOff: ����cu off
*
* RETURNS
*	��һ��aranges head
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static const void *GetARgeHead(const void *pARgeh, const TADDR **ppAddrAry, TSIZE *pCuOff)
{
	unsigned Ver, PtrOff;
	const void *pEnd;
/* �ṹ
	struct {
		TUINT/TSIZE Len;
		unsigned short Ver; // DWARF version
		TUINT/TSIZE CuOff; // cu offset
		unsigned char PtrSz, SegSz; // ָ����/segment���, 4/8�ֽ�
	}
*/
#ifdef SST64
	if (OFFVAL_U4(pARgeh, 0) != 0xFFFFFFFF) {
#endif
		pEnd = (char *)pARgeh + 4 + OFFVAL_U4(pARgeh, 0);
		Ver = OFFVAL_U2(pARgeh, 4);
		*pCuOff = OFFVAL_U4(pARgeh, 6);
		PtrOff = 10;
#ifdef SST64
	} else {
		pEnd = (char *)pARgeh + 12 + OFFVAL_U8(pARgeh, 4);
		*pCuOff = OFFVAL_U8(pARgeh, 14);
		Ver = OFFVAL_U2(pARgeh, 12);
		PtrOff = 22;
	}
#endif
	if (Ver != 2/* DWARF2~5 */ || OFFVAL_U2(pARgeh, PtrOff) != sizeof(TADDR)) {
		*ppAddrAry = NULL;
		SSTERR(RS_SEG_VERSION, NULL, ".debug_aranges", Ver|(OFFVAL_U2(pARgeh, PtrOff) << 16));
	} else {
		*ppAddrAry = (void *)((char *)pARgeh + ((PtrOff + 2 + 2 * sizeof(TADDR) - 1)&~(2 * sizeof(TADDR) - 1))/* ���� */);
	}
	return pEnd;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡcu off
*
* PARAMETERS
*	pDwarHdl: dwar���
*	Addr: ��ַ, ����THUMB bit
*
* RETURNS
*	~(TSIZE)0: û�ж�Ӧ��cu
*	����: cu off
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DwarGetCuOff(const DWAR_HDL *pDwarHdl, TADDR Addr)
{
	const void *pARgeh, * const pEnd = (char *)pDwarHdl->pBase + pDwarHdl->Size;
	const TADDR *pAddrAry;
	TSIZE CuOff;

	for (pARgeh = pDwarHdl->pBase; (void *)pARgeh < pEnd; ) {
		pARgeh = GetARgeHead(pARgeh, &pAddrAry, &CuOff);
		if (!pAddrAry)
			continue;
		while (pAddrAry[1]) {
			if (Addr - pAddrAry[0] < pAddrAry[1])
				return CuOff;
			pAddrAry += 2;
		}
	}
	return ~(TSIZE)0;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡdwar���
*
* PARAMETERS
*	pDwarHdl: ����dwar���
*	pFpHdl: fp���
*	FOff/Size: .debug_arangesƫ�Ƶ�ַ/��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: dwar���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWAR_HDL *GetDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwarHdl->Size = Size;
	pDwarHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwarHdl->pBase)
		goto _ERR1;
	return pDwarHdl;
_ERR1:
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_aranges");
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�dwar���
*
* PARAMETERS
*	pDwarHdl: dwar���
*	pFpHdl: fp���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwarHdl->pBase, pDwarHdl->Size);
}
