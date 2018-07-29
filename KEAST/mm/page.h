/*********************************************************************************
* Copyright(C),2017-2017,MediaTek Inc.
* FileName:	page.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2017/5/3
* Description: page内存管理系统
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
*	根据page结构体地址转换为va
*
* PARAMETERS
*	PageAddr: page结构体地址
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
*	初始化page模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: page模块关闭
*	1: page模块可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PageInit(void);

/*************************************************************************
* DESCRIPTION
*	注销page模块
*
* PARAMETERS
*	无
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PageDeInit(void);

#endif