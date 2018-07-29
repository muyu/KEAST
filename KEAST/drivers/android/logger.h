/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	logger.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: log提取
**********************************************************************************/
#ifndef _LOGGER_H
#define _LOGGER_H

typedef enum _LOGGER_TYPE
{
	MAIN_LOG,
	SYSTEM_LOG,
	EVENT_LOG,
	RADIO_LOG,
	KERNEL_LOG,
	MAX_LOGGER_CNT,
} LOGGER_TYPE;

typedef struct _LoggerEntry
{
	unsigned short Len, HdrSize;
	unsigned Pid, Tid;
	unsigned Sec, Nsec;
	unsigned Euid;
} LoggerEntry;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	确认是否存在log
*
* PARAMETERS
*	Type: log类型
*
* RETURNS
*	0: 不存在
*	1: 存在
*
* LOCAL AFFECTED
*
*************************************************************************/
int ExistLogInf(LOGGER_TYPE Type);

/*************************************************************************
* DESCRIPTION
*	扫描android log
*
* PARAMETERS
*	Type: log类型,不能是KERNEL_LOG!!!
*	pFunc: 分析回调函数指针
*		pEntry: 一条log信息
*		pMsg: log buffer
*		pArg: 传进来的参数
*	pArg: 传给pFunc的参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: pFunc返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ParseDroidLog(LOGGER_TYPE Type, int (*pFunc)(LoggerEntry *pEntry, char *pMsg, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	扫描kernel log
*
* PARAMETERS
*	pFunc: 分析回调函数指针
*		pBuff: 一条log信息
*		pArg: 传进来的参数
*	pArg: 传给pFunc的参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: pFunc返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ParseKernelLog(int (*pFunc)(char *pBuff, void *pArg), void *pArg);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化logger模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: logger模块关闭
*	1: logger模块可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int LoggerInit(void);

/*************************************************************************
* DESCRIPTION
*	注销logger模块
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
void LoggerDeInit(void);

#endif