/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	mrdump.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: mrdump解析及虚拟地址转换
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
/* 根据指令(pInst所指)取出对应的值 */
#define A32LTORG(Off)   (*(unsigned *)((A32CODE(Off)&0xFFF) + (char *)pInst + (Off) + 8))

#define IS_A64ADRP(Off, RegNo)  ((A64CODE(Off)&0x9F00001F) == (0x90000000|(RegNo)))
/* 根据指令(pInst所指)判断是否是文字池装载指令: LDR R(RegNo), [PC, #immed] */
#define IS_A32LTORG(Off, RegNo) ((A32CODE(Off) >> 12) == (0xE59F0|(RegNo)))


/*************************************************************************
*============================== Memory section ==============================
*************************************************************************/
extern unsigned MCntInstSize; /* 插入的mcount指令字节数(-pg) */
#define FUN_PROLOGUE(FAddr)	 ((FAddr) + MCntInstSize + 3 * 4) /* 跳过函数头，无额外压栈版本 */
#define FUN_PROLOGUE1(FAddr)	((FAddr) + MCntInstSize + 4 * 4) /* 跳过函数头，额外压栈版本 */


/*************************************************************************
*============================= commom section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取KE相关信息
*
* PARAMETERS
*	无
*
* RETURNS
*	KE相关信息
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
	const char *pExcpUtc; /* android异常时间点, 可能为NULL */
	TUINT CoreCnt, CpuId; /* 异常CPU id */
	TUINT Sec, USec; /* kernel异常时间点 */
	TUINT FiqStep;
	TUINT KickBit, ValidBit; /* 喂狗位图，在线CPU位图 */
	TADDR MmuBase; /* 物理地址 */
	TADDR MemMapAddr, ModStart; /* 虚拟地址 */
} KEXP_INF;
KEXP_INF *GetKExpInf(void);

/*************************************************************************
* DESCRIPTION
*	获取对应CPU核的当前的上下文信息
*
* PARAMETERS
*	CpuId: CPU id
*
* RETURNS
*	对应上下文信息
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
	int InSec; /* 是否在TEE */
} CORE_CNTX;
CORE_CNTX *GetCoreCntx(TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	设置TEE标记给指定的CPU
*
* PARAMETERS
*	CpuId: CPU id
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SetCoreInSec(TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	更新指定的CPU信息
*
* PARAMETERS
*	CpuId: CPU id
*	pRegAry/Cpsr: CPU寄存器，为NULL则不更新
*	TskAddr: task_struct地址，为0则不更新
*	Pid: 进程标识符
*	pProcName: 进程名, 为NULL则不更新ProcName和Pid
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateCoreCntx(TUINT CpuId, const TSIZE *pRegAry, TSIZE Cpsr, TADDR TskAddr, TUINT Pid, const char *pProcName);

/*************************************************************************
* DESCRIPTION
*	更新HWT信息
*
* PARAMETERS
*	KickBit: 喂狗CPU位图
*	ValidBit: 在线CPU位图
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateHwtInf(TUINT KickBit, TUINT ValidBit);

/*************************************************************************
* DESCRIPTION
*	更新异常kernel时间
*
* PARAMETERS
*	Sec/USec: 秒/微秒, 都为0则不更新!!!
*	eType: 异常类型
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateExcpTime(TUINT Sec, TUINT USec, EXP_TYPE eType);

/*************************************************************************
* DESCRIPTION
*	更新异常utc时间
*
* PARAMETERS
*	pExcpUtc: 异常时间点
*
* RETURNS
*	无
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
*	初始化mrdump模块
*
* PARAMETERS
*	fp: 异常信息输出文件句柄
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PrepareMrdump(FILE *fp);

/*************************************************************************
* DESCRIPTION
*	注销mrdump模块
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
void DestoryMrdump(void);

#endif
