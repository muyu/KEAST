/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwabbrev.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/6
* Description: .debug_abbrev����(֧��dwarf2~4)
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
const void *DwaGetAttrList(const DWA_HDL *pDwaHdl, unsigned short *pTag, TSIZE CuAbbOff, TSIZE AbbNum);

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
int DwaAddAbbInf(DWA_HDL *pDwaHdl, TSIZE CuAbbOff);


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
DWA_HDL *GetDwaHdl(DWA_HDL *pDwaHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

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
void PutDwaHdl(DWA_HDL *pDwaHdl, FP_HDL *pFpHdl);

#endif
