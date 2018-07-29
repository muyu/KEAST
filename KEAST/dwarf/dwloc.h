/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwloc.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_loc����(֧��dwarf2~4)
**********************************************************************************/
#ifndef _DWLOC_H
#define _DWLOC_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWLC_HDL
{
	const void *pBase;
	TSIZE Size;
} DWLC_HDL;


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
const void *DwlcGetExpr(const DWLC_HDL *pDwlcHdl, TSIZE *pSize, TSIZE LocOff, TADDR Addr);


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
DWLC_HDL *GetDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

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
void PutDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl);

#endif
