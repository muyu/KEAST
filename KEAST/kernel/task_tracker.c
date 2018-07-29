/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	task_tracker.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: task解析
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include <callstack.h>
#include <armdis.h>
#include <db/rb_reason.h>
#include "task.h"
#include "task_tracker.h"


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	显示一个进程的基本信息
*
* PARAMETERS
*	fp: 文件句柄
*	pCpuCntx: 进程信息
*	CpuId: 所在cpu id
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispProcessEx(FILE *fp, CORE_CNTX *pCpuCntx, int CpuId)
{
#ifdef SST64
#define SP_INVALID(Sp) ((Sp) <= 0xFFFFFF8000000000)
	static const char *pARM64Modes[] = {"EL0t", "ERR", "ERR", "ERR", "EL1t", "EL1h", "ERR", "ERR", "EL2t", "EL2h", "ERR", "ERR", "EL3t", "EL3h", "ERR", "ERR"};
#else
#define SP_INVALID(Sp) ((Sp) <= 0xC0400000 || (Sp) >= 0xF0000000)
#endif
	static const char *pARM32Modes[] = {"USR", "FIQ", "IRQ", "SVC", "ERR", "ERR", "MON", "ABT", "ERR", "ERR", "HYP", "UND", "ERR", "ERR", "ERR", "SYS"};
	struct _TSK_INF TskInf;
	const char *pMode;

	LOGC(fp, "%s, pid: %d", pCpuCntx->ProcName, pCpuCntx->Pid);
	if (GetTskInf(&TskInf, pCpuCntx->TskAddr) && TskInf.WaitTime)
		LOG(fp, RS_EXEC_T, TskInf.WaitTime);
	if (pCpuCntx->Cpsr&0x80)
		LOG(fp, RS_CPU_DIS_INT);
	if ((pCpuCntx->Cpsr&0x1F) != DEF_CPSR || pCpuCntx->InSec) {
		pMode = pARM32Modes[pCpuCntx->Cpsr&0xF];
#ifdef SST64
		if (!(pCpuCntx->Cpsr&0x10))
			pMode = pARM64Modes[pCpuCntx->Cpsr&0xF];
#endif
		LOGC(fp, ", %s%s\n", pMode, pCpuCntx->InSec ? " TEE" : "");
	} else {
		LOGC(fp, "\n");
		if (SP_INVALID(pCpuCntx->RegAry[REG_SP])) { /* 判断是否在内核?? */
			LOG(fp, RS_REG_EXP);
			LOG(fp, RS_SP_INCORRECT, pCpuCntx->RegAry[REG_SP]);
		} else if ((CpuId || pCpuCntx->Pid) && StackIsOverflow(pCpuCntx->RegAry[REG_SP], CpuId)) {
			LOG(fp, RS_REG_EXP);
			LOG(fp, RS_STK_OVERFLOW);
		}
	}
}

/*************************************************************************
* DESCRIPTION
*	显示一个进程的信息
*
* PARAMETERS
*	fp: 文件句柄
*	pCpuCntx: 进程信息
*	CpuId: 所在cpu id
*	NeedDisas: 是否需要反汇编
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispProcess(FILE *fp, CORE_CNTX *pCpuCntx, int CpuId, int NeedDisas)
{
	CS_HDL *pCsHdl, *pCsF;

	LOGC(fp, "   CPU%d: ", CpuId);
	pCsHdl = GetNativeCsHdl(pCpuCntx->RegAry, 3);
	if (pCsHdl) {
		for (pCsF = pCsHdl; pCsF && (pCsF->Flag&CS_INLINE); pCsF = pCsF->pNext) {} /* 跳过inline帧 */
		if (!pCsF->pFName
		  || (strcmp(pCsF->pFName, "__go_to_wfi") && strcmp(pCsF->pFName, "go_to_rgidle")
			&& strcmp(pCsF->pFName, "arch_cpu_idle") && strcmp(pCsF->pFName, "cpuidle_enter_state"))) {
			DispProcessEx(fp, pCpuCntx, CpuId);
			DispCallStack(fp, pCsHdl, GetCStr(RS_CS));
			if (NeedDisas)
				DispFuncAsm(fp, pCsHdl->FAddr, pCpuCntx->RegAry, REG_ALLMASK);
		} else {
			LOG(fp, RS_CPU_IDLE);
		}
		PutCsHdl(pCsHdl);
	} else {
		DispProcessEx(fp, pCpuCntx, CpuId);
	}
	LOGC(fp, "\n");
}

/*************************************************************************
* DESCRIPTION
*	输出线程信息到文件
*
* PARAMETERS
*	TskAddr: task地址
*	pArg: 文件句柄
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowThread(TADDR TskAddr, void *pArg)
{
	FILE * const fp = pArg;
	struct _TSK_INF TskInf;
	CS_HDL *pCsHdl;
	TUINT Idx;

	if (!GetTskInf(&TskInf, TskAddr)) {
		LOGC(fp, "ERROR: fail to get task("TADDRFMT") info.\n", TskAddr);
		return 0;
	}
	Idx = Fhs((TUINT)TskInf.State);
	LOGC(fp, "ppid=%d pid=%d %c cpu=%d prio=%d wait=%ds %s\n"
		, TskInf.Ppid, TskInf.Pid, Idx < sizeof(TSK_STATE2STR) - 1 ? TSK_STATE2STR[Idx] : '?'
		, TskInf.ExtFlag >> TSK_CPUIDX, TskInf.Prio, TskInf.WaitTime, TskInf.Comm);
	pCsHdl = GetNativeCsHdl(TskInf.RegAry, 3);
	if (pCsHdl) {
		DispCallStack(fp, pCsHdl, GetCStr(RS_CS));
		PutCsHdl(pCsHdl);
	}
	LOGC(fp, "\n");
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	输出进程信息到文件
*
* PARAMETERS
*	TskAddr: task地址
*	pArg: 文件句柄
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowProcess(TADDR TskAddr, void *pArg)
{
	return ForEachThdAddr(TskAddr, ShowThread, pArg);
}


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
void ShowPreemptInf(FILE *fp, TADDR TskAddr, const char *pProcName)
{
	const char *pFName;
	SYM_HDL *pSymHdl;
	char FunName[20];
	TADDR Addr;

	LOG(fp, RS_SCHED2BUG_1, pProcName);
	Addr = GetTskPreemptDisIp(TskAddr);
	if (!Addr) {
		LOG(fp, RS_SCHED2BUG_2);
		return;
	}
	pSymHdl = GetSymHdlByAddr(Addr);
	if (!pSymHdl || (pFName = SymAddr2Name(pSymHdl, NULL, Addr, TGLBFUN)) == NULL) {
		FunName[sizeof(FunName) - 1] = '\0';
		snprintf(FunName, sizeof(FunName) - 1, TADDRFMT, Addr);
		pFName = FunName;
	}
	LOG(fp, RS_SCHED2BUG_3, pFName);
}

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
void TaskTrack(FILE *fp)
{
	KEXP_INF * const pKExpInf = GetKExpInf();
	TUINT i, CpuBitMap, tBitMap;
	CORE_CNTX *pCpuCntx;

	SSTINFO(RS_OUT_CPUS);
	LOG(fp, RS_CPUS_INFO);
	switch (pKExpInf->eType) {
	case EXP_HWT:
		CpuBitMap = pKExpInf->ValidBit&~pKExpInf->KickBit;
		for (tBitMap = i = 0; CpuBitMap != tBitMap; i++) {
			if (!(CpuBitMap&(1 << i)))
				continue;
			if (!tBitMap)
				LOG(fp, RS_NOT_KICK);
			tBitMap |= (1 << i);
			if ((pCpuCntx = GetCoreCntx(i)) != NULL) {
				DispProcess(fp, pCpuCntx, i, 0);
			} else if (PerCpuRbInf(i)->HpState == CPU_DOWN) {
				LOGC(fp, "   CPU%d: ", i);
				LOG(fp, RS_CPU_OFF);
			}
		}
		break;
	case EXP_HW_REBOOT: case EXP_ATF_CRASH: case EXP_THERMAL_RB:
		CpuBitMap = 0;
		break;
	default:
		pCpuCntx = GetCoreCntx(pKExpInf->CpuId);
		LOG(fp, RS_CRASH_CPU);
		if (pCpuCntx)
			DispProcess(fp, pCpuCntx, pKExpInf->CpuId, 1);
		CpuBitMap = 1 << pKExpInf->CpuId;
		break;
	}
	for (tBitMap = 1, i = 0; i < pKExpInf->CoreCnt; i++) {
		if ((CpuBitMap&(1 << i)) || (pCpuCntx = GetCoreCntx(i)) == NULL)
			continue;
		if (tBitMap) {
			tBitMap = 0;
			LOG(fp, RS_OTHER_CPU);
		}
		DispProcess(fp, pCpuCntx, i, 0);
	}
	/* 显示其他线程 */
	if (IS_ALLTHREADS() && GetKExpInf()->mType == MRD_T_MRDUMP && (pVmSHdl->Flag&SYM_LCLSYM)) {
		LOG(fp, RS_ALL_THREAD);
		ForEachProcAddr(ShowProcess, fp);
	}
}
