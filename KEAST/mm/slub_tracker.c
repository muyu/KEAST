/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	slub_tracker.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: slub模块解析
**********************************************************************************/
#include <stdio.h>
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include <kernel/kernel.h>
#include "slub.h"

static int cntxxx;

static int DispSlubEx(TADDR SlubAddr, void *pArg)
{
	struct _SLUB_INF SlubInf;
	FILE *fp = pArg;

	if (!GetSlubInf(&SlubInf, SlubAddr)) {
		printf("xxx\n");
		return 0;
	}
	LOGC(fp, "%s %d, %dKB\n", SlubInf.Name, SlubInf.ObjSz, cntxxx * SlubInf.ObjSz / 1024);
	cntxxx = 0;
	return 0;
}
static int DispSlubObj(TADDR ObjAddr, int IsFree, void *pArg)
{
	cntxxx++;
	return 0;
}

static int DispSlub(TADDR SlubAddr, void *pArg)
{
	int rt;

	rt = ForEachSlubObj(SlubAddr, DispSlubObj, pArg);
	DispSlubEx(SlubAddr, pArg);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	分析slub系统
*
* PARAMETERS
*	fp: 文件句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SlubTrack(FILE *fp)
{
	//ForEachSlubAddr(DispSlub, fp);
}

