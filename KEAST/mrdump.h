/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	mrdump.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: mrdump�����������ַת��
**********************************************************************************/
#ifndef _MRDUMP_H
#define _MRDUMP_H

#include "os.h"
#include <stdio.h>
#include <ttypes.h>
#include <memspace.h>
#include <symfile.h>


#define A64CODE(Off)	(*(unsigned *)((char *)pInst + (Off)))
#define A32CODE(Off)	A64CODE(Off)
#define U32DATA(Off)	A64CODE(Off)

#define A64ADRP(Off)	((((Addr + (Off) + 8) >> 12) << 12) + ((((TSSIZE)(((A64CODE(Off) >> 3)&~3)|((A64CODE(Off) >> 29)&3))) << 43) >> 31))
/* ����ָ��(pInst��ָ)ȡ����Ӧ��ֵ */
#define A32LTORG(Off)   (*(unsigned *)((A32CODE(Off)&0xFFF) + (char *)pInst + (Off) + 8))

#define IS_A64ADRP(Off, RegNo)  ((A64CODE(Off)&0x9F00001F) == (0x90000000|(RegNo)))
/* ����ָ��(pInst��ָ)�ж��Ƿ������ֳ�װ��ָ��: LDR R(RegNo), [PC, #immed] */
#define IS_A32LTORG(Off, RegNo) ((A32CODE(Off) >> 12) == (0xE59F0|(RegNo)))


/*************************************************************************
*============================== Memory section ==============================
*************************************************************************/
extern unsigned MCntInstSize; /* �����mcountָ���ֽ���(-pg) */
#define FUN_PROLOGUE(FAddr)	 ((FAddr) + MCntInstSize + 3 * 4) /* ��������ͷ���޶���ѹջ�汾 */
#define FUN_PROLOGUE1(FAddr)	((FAddr) + MCntInstSize + 4 * 4) /* ��������ͷ������ѹջ�汾 */


/*************************************************************************
*============================= commom section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡKE�����Ϣ
*
* PARAMETERS
*	��
*
* RETURNS
*	KE�����Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef enum _EXP_TYPE
{
	EXP_NONE,
	EXP_NO_EXP,
	EXP_SEGV,
	EXP_DIE,
	EXP_PANIC,
	EXP_HWT,
	EXP_HW_REBOOT,
	EXP_BUG,
	EXP_ATF_CRASH,
	EXP_THERMAL_RB,
	EXP_SPM_RB,
} EXP_TYPE;
typedef const struct _KEXP_INF
{
	EXP_TYPE eType;
	enum
	{
		MRD_T_MIDUMP,
		MRD_T_MRDUMP,
		MRD_T_NONE,
	} mType;
	const char *pCoreFile;
	const char *pExcpUtc; /* android�쳣ʱ���, ����ΪNULL */
	TUINT CoreCnt, CpuId; /* �쳣CPU id */
	TUINT Sec, USec; /* kernel�쳣ʱ��� */
	TUINT FiqStep;
	TUINT KickBit, ValidBit; /* ι��λͼ������CPUλͼ */
	TADDR MmuBase; /* �����ַ */
	TADDR MemMapAddr, ModStart; /* �����ַ */
} KEXP_INF;
KEXP_INF *GetKExpInf(void);

/*************************************************************************
* DESCRIPTION
*	��ȡ��ӦCPU�˵ĵ�ǰ����������Ϣ
*
* PARAMETERS
*	CpuId: CPU id
*
* RETURNS
*	��Ӧ��������Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef const struct _CORE_CNTX
{
#define TASK_COMM_LEN   16
	char ProcName[TASK_COMM_LEN + 1];
	TUINT Pid;
#ifdef SST64
#define DEF_CPSR 0x05 /* EL1h */
#else
#define DEF_CPSR 0x13 /* SVC */
#endif
	TSIZE RegAry[REG_NUM], Cpsr, Tcr;
	TADDR TskAddr;
	TUINT Sctlr;
	int InSec; /* �Ƿ���TEE */
} CORE_CNTX;
CORE_CNTX *GetCoreCntx(TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	����TEE��Ǹ�ָ����CPU
*
* PARAMETERS
*	CpuId: CPU id
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SetCoreInSec(TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	����ָ����CPU��Ϣ
*
* PARAMETERS
*	CpuId: CPU id
*	pRegAry/Cpsr: CPU�Ĵ�����ΪNULL�򲻸���
*	TskAddr: task_struct��ַ��Ϊ0�򲻸���
*	Pid: ���̱�ʶ��
*	pProcName: ������, ΪNULL�򲻸���ProcName��Pid
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateCoreCntx(TUINT CpuId, const TSIZE *pRegAry, TSIZE Cpsr, TADDR TskAddr, TUINT Pid, const char *pProcName);

/*************************************************************************
* DESCRIPTION
*	����HWT��Ϣ
*
* PARAMETERS
*	KickBit: ι��CPUλͼ
*	ValidBit: ����CPUλͼ
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateHwtInf(TUINT KickBit, TUINT ValidBit);

/*************************************************************************
* DESCRIPTION
*	�����쳣kernelʱ��
*
* PARAMETERS
*	Sec/USec: ��/΢��, ��Ϊ0�򲻸���!!!
*	eType: �쳣����
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateExcpTime(TUINT Sec, TUINT USec, EXP_TYPE eType);

/*************************************************************************
* DESCRIPTION
*	�����쳣utcʱ��
*
* PARAMETERS
*	pExcpUtc: �쳣ʱ���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateExcpUtc(const char *pExcpUtc);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��mrdumpģ��
*
* PARAMETERS
*	fp: �쳣��Ϣ����ļ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PrepareMrdump(FILE *fp);

/*************************************************************************
* DESCRIPTION
*	ע��mrdumpģ��
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
void DestoryMrdump(void);

#endif
