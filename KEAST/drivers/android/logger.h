/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	logger.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: log��ȡ
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
*	ȷ���Ƿ����log
*
* PARAMETERS
*	Type: log����
*
* RETURNS
*	0: ������
*	1: ����
*
* LOCAL AFFECTED
*
*************************************************************************/
int ExistLogInf(LOGGER_TYPE Type);

/*************************************************************************
* DESCRIPTION
*	ɨ��android log
*
* PARAMETERS
*	Type: log����,������KERNEL_LOG!!!
*	pFunc: �����ص�����ָ��
*		pEntry: һ��log��Ϣ
*		pMsg: log buffer
*		pArg: �������Ĳ���
*	pArg: ����pFunc�Ĳ���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	����: pFunc����ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ParseDroidLog(LOGGER_TYPE Type, int (*pFunc)(LoggerEntry *pEntry, char *pMsg, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	ɨ��kernel log
*
* PARAMETERS
*	pFunc: �����ص�����ָ��
*		pBuff: һ��log��Ϣ
*		pArg: �������Ĳ���
*	pArg: ����pFunc�Ĳ���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	����: pFunc����ֵ
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
*	��ʼ��loggerģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: loggerģ��ر�
*	1: loggerģ�����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int LoggerInit(void);

/*************************************************************************
* DESCRIPTION
*	ע��loggerģ��
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
void LoggerDeInit(void);

#endif