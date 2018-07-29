/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	ac.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/3/11
* Description: AC��ģʽ�ַ���ƥ��ģ��
**********************************************************************************/
#ifndef _AC_H
#define _AC_H

typedef void * AC_HDL;

/*************************************************************************
* DESCRIPTION
*	AC�Զ��������ַ���
*
* PARAMETERS
*	Hdl: ac handle
*	ppStr: input: �������ַ���, output: ���ҳɹ�����ַ���λ��
*
* RETURNS
*	0: ʧ��
*	����: string idx + 1
*
* LOCAL AFFECTED
*
*************************************************************************/
unsigned AcTrieSearch(AC_HDL *pHdl, const char **ppStr);

/*************************************************************************
* DESCRIPTION
*	����pattern�ַ�������ACģ��
*
* PARAMETERS
*	ppPattern: ��ƥ����ַ�������,NULL��β
*
* RETURNS
*	NULL: ʧ��
*	����: ae handle
*
* LOCAL AFFECTED
*
*************************************************************************/
AC_HDL *GetAcTrie(const char * const *ppPattern);

/*************************************************************************
* DESCRIPTION
*	�ͷ�acģ��
*
* PARAMETERS
*	Hdl: ac handle
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
void PutAcTrie(AC_HDL *pHdl);

#endif