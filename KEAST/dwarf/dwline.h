/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	dwline.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/22
* Description: .debug_line����(֧��dwarf2~4)
**********************************************************************************/
#ifndef _DWLINE_H
#define _DWLINE_H

#include <ttypes.h>
#include "os.h"

typedef struct _DWL_SEQINF
{
	const void *pSSeq, *pHead;
	TADDR SAddr, EAddr;
} DWL_SEQINF;

typedef struct _DWL_HDL
{
	const void *pBase;
	DWL_SEQINF *pSeqs;
	TSIZE Size;
	unsigned Cnt;
} DWL_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡ����·�����к�
*
* PARAMETERS
*	pDwlHdl: dwl���
*	pLineNo: �����к�
*	Addr: ��ַ, ����THUMB bit
*
* RETURNS
*	NULL: ʧ��
*	����: ·��������ʱ�����Free()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DwlAddr2Line(DWL_HDL *pDwlHdl, TUINT *pLineNo, TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	����ƫ�ƺ�·��������ȡ·��
*
* PARAMETERS
*	pDwlHdl: dwl���
*	LineOff: lineƫ����
*	PathIdx: ·������
*
* RETURNS
*	NULL: ʧ��
*	����: ·��������ʱ�����Free()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DwlIdx2Path(DWL_HDL *pDwlHdl, TSIZE LineOff, unsigned PathIdx);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡdwl���
*
* PARAMETERS
*	pDwlHdl: ����dwl���
*	pFpHdl: fp���
*	FOff/Size: .debug_lineƫ�Ƶ�ַ/��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: dwl���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWL_HDL *GetDwlHdl(DWL_HDL *pDwlHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	�ͷ�dwl���
*
* PARAMETERS
*	pDwlHdl: dwl���
*	pFpHdl: fp���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwlHdl(DWL_HDL *pDwlHdl, FP_HDL *pFpHdl);

#endif
