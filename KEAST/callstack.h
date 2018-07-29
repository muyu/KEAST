/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	callstack.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: ����ջ��ԭ
**********************************************************************************/
#ifndef _CALLSTACK_H
#define _CALLSTACK_H

#include <stdio.h>
#include <ttypes.h>
#include <symfile.h>


#define REG_ALLMASK	 (((TSIZE)1 << REG_NUM) - 1)

typedef const struct _CS_PARA
{
	BASE_VAL Val;
	const char *pName; /* ����ΪNULL */
	BASE_TYPE Type;
} CS_PARA;

typedef const struct _CS_FRAME
{
	const struct _CS_FRAME *pNext;
	TADDR FAddr; /* ����ͷ��ַ��Ϊ0���ʾ��չ֡, ����THUMB bit */
	TUINT Off, LineNo; /* ƫ����(�ֽ�)/�к� */

#define CS_MNAME	(1 << 0) /* ��Ҫ�ͷ�pMName */
#define CS_EXCP		(1 << 1) /* �쳣֡(irq,abort) */
#define CS_INLINE	(1 << 2) /* ��ʾ��֡��inline */
	unsigned Flag, ArgCnt;
	const char *pMName; /* �ڴ������� */
	const char *pFName; /* ������������ΪNULL */
	const char *pPath; /* ����·��������ΪNULL */
	CS_PARA Para[0]; /* �������� */
} CS_FRAME, CS_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʾ����ջ
*
* PARAMETERS
*	fp: �ļ����
*	pCsHdl: cs���
*	pHeadStr: ���ͷ��Ϣ
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispCallStack(FILE *fp, CS_HDL *pCsHdl, const char *pHeadStr);

/*************************************************************************
* DESCRIPTION
*	��ȡ���ص���ջ, ע��: pRegAry[REG_SP],pRegAry[REG_PC]����������ȷ��ֵ!!!
*
* PARAMETERS
*	pRegAry: �Ĵ�����(REG_NUM���Ĵ���)
*	Flag: ��Ϣ����
*		bit0: ��ʾ�ļ���/�к�, ����
*		bit1: ��ʾinline����
*
* RETURNS
*	NULL: ʧ��
*	����: ����ջ������ʱ�����PutCsHdl()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
CS_HDL *GetNativeCsHdl(const TSIZE *pRegAry, int Flag);

/*************************************************************************
* DESCRIPTION
*	����LR���齨�����ص���ջ
*
* PARAMETERS
*	pBts/Cnt: LR����/����
*
* RETURNS
*	NULL: ʧ��
*	����: ����ջ������ʱ�����PutCsHdl()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
CS_HDL *Bt2NativeCsHdl(const TSIZE *pBts, TUINT Cnt);

/*************************************************************************
* DESCRIPTION
*	�ͷ�cs���
*
* PARAMETERS
*	pCsHdl: cs���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutCsHdl(CS_HDL *pCsHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡ��1֡�쳣֡�ļĴ�����Ϣ, ����mrdump����ջ!
*
* PARAMETERS
*	pRegAry: ���ؼĴ�����(REG_NUM���Ĵ���)
*
* RETURNS
*	�Ĵ�����Чλͼ, 0��ʾʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE GetFirstExcpCntx(TSIZE *pRegAry);

#endif
