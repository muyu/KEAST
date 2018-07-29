/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	armdis.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/11/13
* Description: arm�����
**********************************************************************************/
#ifndef _ARMDIS_H
#define _ARMDIS_H

#include <stdio.h>
#include <ttypes.h>

typedef enum _EX_TYPE
{
	EX_FAIL,	 /* ʧ�� */
	EX_NORMAL,   /* ����, �������̻�HW���� */
	EX_INST_EA,  /* �����PC��ַ(��ַ��Ч/δ����/��ִ��Ȩ��) */
	EX_INST_UND, /* δ����ָ�� */
	EX_DATA_UNA, /* ��ַδ���� */
	EX_DATA_EAR, /* �ڴ���ĵ�ַ��(��ַ��Ч/�޶�Ȩ��) */
	EX_DATA_EAW, /* �ڴ���ĵ�ַд(��ַ��Ч/��дȨ��) */
} EX_TYPE;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʾ�������쳣��ط����ָ��
*
* PARAMETERS
*	fp: �ļ����
*	FAddr/Size: ������ʼ��ַ
*	pRegAry/RegMask: �Ĵ�����/�Ĵ�����Чλͼ
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispFuncAsm(FILE *fp, TADDR FAddr, const TSIZE *pRegAry, TSIZE RegMask);

/*************************************************************************
* DESCRIPTION
*	��ȡ�쳣��Ϣ
*
* PARAMETERS
*	pVal: ���ش�ȡ�����ݻ�ָ��(ΪEX_NORMALʱ)
*	pFaultAddr: �����쳣��ַ
*	pRegAry/Cpsr: �쳣ʱ�̵ļĴ�����Ϣ
*
* RETURNS
*	�쳣����
*
* LOCAL AFFECTED
*
*************************************************************************/
EX_TYPE GetExInf(TADDR *pFaultAddr, TSIZE *pVal, const TSIZE *pRegAry, TSIZE Cpsr);

/*************************************************************************
* DESCRIPTION
*	�Ĵ��������Ƶ��������Ƶ���������
*
* PARAMETERS
*	FAddr: ������ַ
*	pRegAry: �Ĵ�����Ϣ���ᱻ����
*	RegMask: �Ĵ�����Чλͼ
*	TarMask: Ԥ�ڴﵽ����Чλͼ
*
* RETURNS
*	���º����Чλͼ
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE RegDerivation(TADDR FAddr, TSIZE *pRegAry, TSIZE RegMask, TSIZE TarMask);

#endif
