/*********************************************************************************
* Copyright(C),2017-2017,MediaTek Inc.
* FileName:	page.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2017/5/3
* Description: page�ڴ����ϵͳ
**********************************************************************************/
#ifndef _PAGE_H
#define _PAGE_H


typedef const struct _PAGE_HEAD
{
	TSIZE Flags;
	TADDR MappingAddr;
	union
	{
		TSIZE Idx;
		TADDR FreeListAddr;
	} u1;
	union
	{
		TSIZE Cnts;
		struct
		{
			TINT MapCnt; /* SlubInf */
			TINT RefCnt;
		} s1;
	} u2;
	union
	{
		TADDR Lru[2];
		struct
		{
			TADDR NextAddr;
#ifdef SST64
			TINT
#else
			unsigned short
#endif
				Pages, PObjs;
		} Slub;
		struct
		{
			TADDR Head;
#ifdef SST64
			TINT
#else
			unsigned short
#endif
				Dtor, Order;
		} Cmpd;
	} u3;
} PAGE_HEAD;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	����page�ṹ���ַת��Ϊva
*
* PARAMETERS
*	PageAddr: page�ṹ���ַ
*
* RETURNS
*	va
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR PageAddr2VAddr(TADDR PageAddr);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��pageģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: pageģ��ر�
*	1: pageģ�����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PageInit(void);

/*************************************************************************
* DESCRIPTION
*	ע��pageģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PageDeInit(void);

#endif