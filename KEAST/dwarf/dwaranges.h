/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwaranges.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_aranges����(֧��dwarf2~4)
**********************************************************************************/
#ifndef _DWARANGES_H
#define _DWARANGES_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWAR_HDL
{
	const void *pBase;
	TSIZE Size;
} DWAR_HDL;


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
TSIZE DwarGetCuOff(const DWAR_HDL *pDwarHdl, TADDR Addr);


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
DWAR_HDL *GetDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

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
void PutDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl);

#endif
