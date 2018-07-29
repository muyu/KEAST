/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	dwarf_int.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/23
* Description: dwarf�ڲ�ͷ�ļ�
**********************************************************************************/
#ifndef _DWARF_INT_H
#define _DWARF_INT_H

#include <ttypes.h>
#include "dwarf.h"


/*************************************************************************
*=========================== internal section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡCIE��Ϣ
*
* PARAMETERS
*	pCie: ����CIE�ṹ��
*	p: CIE buffer
*	pEhAugFunc: .eh_frame��չ�ַ��������ص�����
*		pCie: ����CIE�ṹ��
*		pAug: ��չ�ַ���
*		p: CIE buffer
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef struct _CIE_INF
{
	unsigned long long AddrOff; /* .eh_frame��ַ��������, ���ͳ��ȱ�����MAX(sizeof(TADDR), sizeof(void *)) */
	TSIZE CodeAlign;
	TSSIZE DataAlign;
	TUINT RtIdx; /* ���ؼĴ������� */
	const unsigned char *pSInst, *pEInst; /* [��ʼָ��, ����ָ��) */
	const char *pName, *pSeg;
	unsigned char HasAug;
	unsigned char Enc; /* .eh_frame��ַ���� */
} CIE_INF;
int ExtractCie(CIE_INF *pCie, const void *p, int (*pEhAugFunc)(CIE_INF *pCie, const char *pAug, const void *p));

/*************************************************************************
* DESCRIPTION
*	����FDE��ԭ1��ջ֡
*
* PARAMETERS
*	pFde: FDE�ṹ��
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_SP/REG_PC������Ч!!!), �ᱻ����!!!
*	Pc: ��������ַ��THUMB bit��PC
*	pGetTAddrFunc: ��ȡĿ���ַ�ص�����, ���ض�ȡ���buffer
*		pCie: CIE�ṹ��
*		pAddr: ����Ŀ���ַ
*		p: buffer
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef struct _FDE_INF
{
	TADDR FAddr; /* ��������ַ��THUMB bit */
	TSIZE Size;
	CIE_INF Cie;
	const unsigned char *pSInst, *pEInst; /* [��ʼָ��, ����ָ��) */
} FDE_INF;
int FdeUnwind(const FDE_INF *pFde, TSIZE *pRegAry, TSIZE *pRegMask, TSIZE Pc, const void *(*pGetTAddrFunc)(const CIE_INF *pCie, TADDR *pAddr, const void *p));


/*************************************************************************
* DESCRIPTION
*	�������ʽ
*
* PARAMETERS
*	p: ָ��buffer
*	Len: ָ���
*	pRt: ���ؼ���Ľ��
*	pRegAry/RegMask: �ο��ļĴ�����/�Ĵ�����Чλͼ
*	pSegName: ������, ��.debug_frame/.debug_loc��
*	pFileName: �ļ���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DwEvalExpr(const void *p, TSIZE Len, TSIZE *pRt, const TSIZE *pRegAry, TSIZE RegMask, const char *pSegName, const char *pFileName);

#endif
