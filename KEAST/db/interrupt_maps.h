/*********************************************************************************
* Copyright(C),2018-2018,MediaTek Inc.
* FileName:	interrupt_maps.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2018/4/25
* Description: INTERRUPT_NUM_NAME_MAPS信息提取
**********************************************************************************/
#ifndef _INTERRUPT_MAPS_H
#define _INTERRUPT_MAPS_H


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据irq index获取irq的名称
*
* PARAMETERS
*	Irq: irq index
*	pStr: 返回irq名称
*	Sz: pStr指向内存的大小
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int IrqIdx2Name(TUINT Irq, char *pStr, size_t Sz);

#endif
