/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	task_tracker.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: task解析
**********************************************************************************/
#ifndef _TASK_TRACKER_H
#define _TASK_TRACKER_H

#include <stdio.h>


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	显示抢断相关信息
*
* PARAMETERS
*	fp: 文件句柄
*	TskAddr: 进程task_struct地址
*	pProcName: 进程名
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void ShowPreemptInf(FILE *fp, TADDR TskAddr, const char *pProcName);

/*************************************************************************
* DESCRIPTION
*	分析task模块
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
void TaskTrack(FILE *fp);

#endif
