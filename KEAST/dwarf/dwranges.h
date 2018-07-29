/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwranges.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_ranges����(֧��dwarf2~4)
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
TADDR DwrCheckAddr(const DWR_HDL *pDwrHdl, TSIZE RgeOff, TADDR Addr);


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
DWR_HDL *GetDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

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
void PutDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl);

#endif
