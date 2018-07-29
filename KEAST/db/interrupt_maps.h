/*********************************************************************************
* Copyright(C),2018-2018,MediaTek Inc.
* FileName:	interrupt_maps.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2018/4/25
* Description: INTERRUPT_NUM_NAME_MAPS��Ϣ��ȡ
**********************************************************************************/
#ifndef _INTERRUPT_MAPS_H
#define _INTERRUPT_MAPS_H


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	����irq index��ȡirq������
*
* PARAMETERS
*	Irq: irq index
*	pStr: ����irq����
*	Sz: pStrָ���ڴ�Ĵ�С
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int IrqIdx2Name(TUINT Irq, char *pStr, size_t Sz);

#endif
