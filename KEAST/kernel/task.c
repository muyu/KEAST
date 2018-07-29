/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	task.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: task����
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <module.h>
#include "string_res.h"
#include <mrdump.h>
#include <callstack.h>
#include "task.h"

#define STACK_END_MAGIC	0x57AC6E9D
#define PREEMPT_ACTIVE	0x40000000 /* ��־�ں���ռ���� */

typedef enum _TSK_ID
{
#define TSKOFF_MAP \
	_NL(TSK_STATE,		"state") \
	_NL(TSK_FLAGS,		"flags") \
	_NL(TSK_ON_CPU, 	"on_cpu") \
	_NL(TSK_ON_RQ,		"on_rq") \
	_NL(TSK_PRIO,		"prio") \
	_NL(TSK_POLICY, 	"policy") \
	_NL(TSK_TASKS,		"tasks") \
	_NL(TSK_MM, 		"mm") \
	_NL(TSK_PID,		"pid") \
	_NL(TSK_RPARENT,	"real_parent") \
	_NL(TSK_THDNODE,	"thread_node") \
	_NL(TSK_SIGNAL, 	"signal") \
	/* ��������µĳ�Ա */ \
	_NL(TSK_COMM,		"comm")

#ifdef SST64
#define TSKOFF_MAP2 \
	_NL(TSK_THD,		"thread") \
	
#else
#define TSKOFF_MAP2
#endif

#define _NL(Enum, str) Enum,
	TSKOFF_MAP2
	TSKOFF_MAP
#undef _NL
} TSK_ID;

typedef enum _THD_ID
{
#define THDOFF_MAP \
	_NL(THD_TASK,		"task") \
	_NL(THD_PMTCNT, 	"preempt_count") \
	/* ��������µĳ�Ա */ \
	_NL(THD_CPU,		"cpu")

#ifdef SST64
#define THDOFF_MAP2
#else
#define THDOFF_MAP2 \
	_NL(THD_REG,		"cpu_context")
#endif

#define _NL(Enum, str) Enum,
	THDOFF_MAP2
	THDOFF_MAP
#undef _NL
} THD_ID;

static struct
{
	unsigned long long CurTime; /* ��λ: ns */
	TADDR RunQAddr;
#ifdef SST64
	TADDR PCpuIrqStkAddr;
	int ThdInTsk;
#endif
	TUINT TskSz, ThdSz;
	int TskOff[TSK_COMM + 1];
	int ThdOff[THD_CPU + 1];
	int WaitOff[3], CurrOff;
	int HasTskDbgHdl;
	struct _DBG_HDL TskDbgHdl;
} TskInfs /* taskinit �г�ʼ��task �ṹ��offset info */;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	����sp��ַ��ȡthread_info��ַ
*
* PARAMETERS
*	Sp: sp��ַ
*	CpuId: cpu id
*
* RETURNS
*	0: ʧ��
*	����: thread_info��ַ
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR Sp2ThdInfAddr(TADDR Sp, TUINT CpuId)
{
#ifdef SST64
	TADDR IrqStkAddr;

	if (TskInfs.PCpuIrqStkAddr) {
		IrqStkAddr = PerCpuAddr(TskInfs.PCpuIrqStkAddr, CpuId);
		if (Sp >= IrqStkAddr && Sp < IrqStkAddr + THREAD_SIZE) {
			Sp = 0;
			DumpMem(IrqStkAddr + THREAD_SIZE - sizeof(Sp) * 3, &Sp, sizeof(Sp));
		}
	}
#endif
	(void)CpuId; /* �ñ��������� */
	return THDSTK_BASE(Sp);
}

/*************************************************************************
* DESCRIPTION
*	������CPU���еļĴ�����Ϣ
*
* PARAMETERS
*	pCpuCntx: ������Ϣ
*	CpuId: cpu id
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void UpdateOnCpuReg(CORE_CNTX *pCpuCntx, TUINT CpuId)
{
	KEXP_INF * const pKExpInf = GetKExpInf();
	struct _PT_REGS PtRegs;
	const char *pFName;
	int NeedUnwind = 0;
	SYM_HDL *pSymHdl;

	/* ���������� */
	pSymHdl = GetSymHdlByAddr(pCpuCntx->RegAry[REG_PC]);
	if (!pSymHdl || (pFName = SymAddr2Name(pSymHdl, NULL, pCpuCntx->RegAry[REG_PC], TGLBFUN)) == NULL)
		return;
	if (pKExpInf->CpuId == CpuId) {
		if (pKExpInf->mType == MRD_T_MRDUMP && !(strcmp(pFName, "aee_kdump_reboot") && strcmp(pFName, "mrdump_save_current_backtrace")))
			NeedUnwind = 1;
	} else {
		if (!(strcmp(pFName, "handle_IPI") && strcmp(pFName, "mrdump_stop_noncore_cpu") && strcmp(pFName, "mrdump_save_current_backtrace")
		  && strcmp(pFName, "aee_smp_send_stop") && strcmp(pFName, "aee_fiq_ipi_cpu_stop")))
			NeedUnwind = 1;
	}
	if (NeedUnwind) {
		memcpy(PtRegs.RegAry, pCpuCntx->RegAry, sizeof(PtRegs.RegAry));
		PtRegs.Cpsr = pCpuCntx->Cpsr;
		if (GetFirstExcpCntx(PtRegs.RegAry)) {
			UpdateCoreCntx(CpuId, PtRegs.RegAry, PtRegs.Cpsr&~(TSIZE)0xA0/* ���IRQ bit����ֹ�ϲ��� */, 0, 0, NULL);
			pSymHdl = GetSymHdlByAddr(pCpuCntx->RegAry[REG_PC]); /* ���������� */
			if (!pSymHdl || (pFName = SymAddr2Name(pSymHdl, NULL, pCpuCntx->RegAry[REG_PC], TGLBFUN)) == NULL)
				return;
		}
	}
	if (!strcmp(pFName, "aee_wdt_atf_info")) { /* X22ָ��regs */
#ifdef SST64
		if (pCpuCntx->RegAry[22] > pCpuCntx->RegAry[REG_SP] && !DumpMem(pCpuCntx->RegAry[22], &PtRegs, sizeof(PtRegs))) /* M0�汾��������! */
#else
		if (!DumpMem(pCpuCntx->RegAry[REG_FP] + 4, &PtRegs, sizeof(PtRegs)))
#endif
			UpdateCoreCntx(CpuId, PtRegs.RegAry, PtRegs.Cpsr, 0, 0, NULL);
	}
#ifndef SST64
	if (!strcmp(pFName, "aee_wdt_irq_info")) {
		TADDR Addr;
		int i;

		for (i = 0; i < 4 * 6; i += 4) { /* ��FP��ָջ����R5/R6(������regsָ��) */
			if (!DumpMem(pCpuCntx->RegAry[REG_FP] - 4 * sizeof(PtRegs.Cpsr) - i, &Addr, sizeof(Addr))
			  && !((Addr + sizeof(PtRegs) + 8)&0xFFF)
			  && !DumpMem(Addr, &PtRegs, sizeof(PtRegs))) {
				UpdateCoreCntx(CpuId, PtRegs.RegAry, PtRegs.Cpsr, 0, 0, NULL);
				break;
			}
		}
	}
#endif
}

/*************************************************************************
* DESCRIPTION
*	������CPU���е�task��Ϣ
*
* PARAMETERS
*	pCpuCntx: ������Ϣ
*	CpuId: cpu id
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void UpdateOnCpuTsk(CORE_CNTX *pCpuCntx, TUINT CpuId)
{
	char Comm[TASK_COMM_LEN + 1];
	const char *pBuf;
	VMA_HDL *pVmaHdl;
	TADDR Addr;

	/* ���½�����/pid */
	if (TskInfs.TskOff[TSK_COMM] <= 0 || TskInfs.TskOff[TSK_PID] <= 0)
		return;
	if (pCpuCntx->TskAddr)
		Addr = pCpuCntx->TskAddr;
	else if (TskInfs.ThdOff[THD_TASK] <= 0 /* ��thread_infoȡ��task */
		|| DumpMem(Sp2ThdInfAddr(pCpuCntx->RegAry[REG_SP], CpuId) + TskInfs.ThdOff[THD_TASK], &Addr, sizeof(Addr))) {
		Addr = PerCpuAddr(TskInfs.RunQAddr, CpuId); /* ͨ��rqȡ��curr */
		if (DumpMem(Addr + TskInfs.CurrOff, &Addr, sizeof(Addr)))
			return;
	}
	pVmaHdl = GetVmaHdl(Addr);
	if (!pVmaHdl)
		return;
	pBuf = Vma2Mem(pVmaHdl, Addr, TskInfs.TskSz);
	if (pBuf) {
		Comm[TASK_COMM_LEN] = '\0';
		strncpy(Comm, pBuf + TskInfs.TskOff[TSK_COMM], TASK_COMM_LEN);
		UpdateCoreCntx(CpuId, NULL, 0, Addr, OFFVAL(TINT, pBuf, TskInfs.TskOff[TSK_PID]), Comm);
	}
	PutVmaHdl(pVmaHdl);
}

/*************************************************************************
* DESCRIPTION
*	��ȡthread������Ϣ
*
* PARAMETERS
*	pTskHdl: ����task������Ϣ
*	ThdAddr: thread_info��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: thread������Ϣ
*
* LOCAL AFFECTED
*
*************************************************************************/
static TSK_INF *GetThdInf(struct _TSK_INF *pTskInf, TADDR ThdAddr)
{
	CORE_CNTX *pCpuCntx;
	const char *pBuf;
	VMA_HDL *pVmaHdl;

	pVmaHdl = GetVmaHdl(ThdAddr);
	if (!pVmaHdl)
		return NULL;
	pBuf = Vma2Mem(pVmaHdl, ThdAddr, TskInfs.ThdSz);
	if (!pBuf)
		goto _OUT;
#ifdef SST64
	if (!TskInfs.ThdInTsk)
#endif
	pTskInf->ExtFlag |= (OFFVAL(TUINT, pBuf, TskInfs.ThdOff[THD_CPU]) << TSK_CPUIDX);
#ifndef SST64
	memcpy(&pTskInf->RegAry[4], pBuf + TskInfs.ThdOff[THD_REG], sizeof(pTskInf->RegAry[0]) * 8);
	pTskInf->RegAry[REG_SP] = OFFVAL_SIZE(pBuf, TskInfs.ThdOff[THD_REG] + sizeof(pTskInf->RegAry[0]) * 8);
	pTskInf->RegAry[REG_PC] = OFFVAL_SIZE(pBuf, TskInfs.ThdOff[THD_REG] + sizeof(pTskInf->RegAry[0]) * 9);
#endif
	/* �����������ܵ��̵߳ļĴ��� */
	if (pTskInf->ExtFlag&TSK_ONCPU) {
		pCpuCntx = GetCoreCntx(pTskInf->ExtFlag >> TSK_CPUIDX);
		if (pCpuCntx)
			memcpy(pTskInf->RegAry, pCpuCntx->RegAry, sizeof(pCpuCntx->RegAry));
	}
_OUT:
	PutVmaHdl(pVmaHdl);
	return pTskInf;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡtask������Ϣ
*
* PARAMETERS
*	pTskInf: ����task������Ϣ
*	TskAddr: task_struct��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: task������Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSK_INF *GetTskInf(struct _TSK_INF *pTskInf, TADDR TskAddr)
{
	unsigned long long StartTime;
	const char *pBuf;
	VMA_HDL *pVmaHdl;

	pVmaHdl = GetVmaHdl(TskAddr);
	if (!pVmaHdl)
		return NULL;
	pBuf = Vma2Mem(pVmaHdl, TskAddr, TskInfs.TskSz);
	if (!pBuf) {
		PutVmaHdl(pVmaHdl);
		return NULL;
	}
	memset(pTskInf, 0, sizeof(*pTskInf));
	pTskInf->State = OFFVAL_SIZE(pBuf, TskInfs.TskOff[TSK_STATE]);
	pTskInf->StkAddr = OFFVAL_SIZE(pBuf, TskInfs.TskOff[TSK_STATE] + sizeof(TSIZE));
	pTskInf->Flags = OFFVAL(TUINT, pBuf, TskInfs.TskOff[TSK_FLAGS]);
	strncpy(pTskInf->Comm, pBuf + TskInfs.TskOff[TSK_COMM], TASK_COMM_LEN);
	pTskInf->Policy = OFFVAL(TUINT, pBuf, TskInfs.TskOff[TSK_POLICY]);
	pTskInf->Prio = (pTskInf->Policy == SCHED_FIFO || pTskInf->Policy == SCHED_RR ? 
		OFFVAL(TUINT, pBuf, TskInfs.TskOff[TSK_PRIO] + sizeof(TUINT) * 3) : OFFVAL(TUINT, pBuf, TskInfs.TskOff[TSK_PRIO] + sizeof(TUINT) * 2));
	pTskInf->CorePrio = OFFVAL(TUINT, pBuf, TskInfs.TskOff[TSK_PRIO]);
	if (OFFVAL(TINT, pBuf, TskInfs.TskOff[TSK_ON_CPU]))
		pTskInf->ExtFlag |= TSK_ONCPU;
	if (OFFVAL(TINT, pBuf, TskInfs.TskOff[TSK_ON_RQ]))
		pTskInf->ExtFlag |= TSK_ONRQ;
	pTskInf->Pid = OFFVAL(TINT, pBuf, TskInfs.TskOff[TSK_PID]);
	pTskInf->Ppid = OFFVAL(TINT, pBuf, TskInfs.TskOff[TSK_PID] + sizeof(TINT));
	if (TskInfs.WaitOff[0] > 0) {
		if (!pTskInf->State)
			StartTime = OFFVAL_U8(pBuf, TskInfs.WaitOff[0] + (pTskInf->ExtFlag&TSK_ONCPU ? 0 : sizeof(TSIZE)));
		else if (pTskInf->State&TASK_UNINTERRUPTIBLE)
			StartTime = OFFVAL_U8(pBuf, TskInfs.WaitOff[2]);
		else
			StartTime = OFFVAL_U8(pBuf, TskInfs.WaitOff[1]);
		if (TskInfs.CurTime - 1 > StartTime - 1)
			pTskInf->WaitTime = (TUINT)((TskInfs.CurTime - StartTime) / 1000000000ULL);
	}
#ifdef SST64
	if (TskInfs.ThdInTsk)
		pTskInf->ExtFlag |= (OFFVAL(TUINT, pBuf, TskInfs.ThdOff[THD_CPU]) << TSK_CPUIDX);
	memcpy(&pTskInf->RegAry[19], pBuf + TskInfs.TskOff[TSK_THD], sizeof(TSIZE) * 11);
	pTskInf->RegAry[REG_SP] = OFFVAL_SIZE(pBuf, TskInfs.TskOff[TSK_THD] + sizeof(TSIZE) * 11);
	pTskInf->RegAry[REG_PC] = OFFVAL_SIZE(pBuf, TskInfs.TskOff[TSK_THD] + sizeof(TSIZE) * 12);
#endif
	GetThdInf(pTskInf, pTskInf->StkAddr);
	PutVmaHdl(pVmaHdl);
	return pTskInf;
}

/*************************************************************************
* DESCRIPTION
*	�������̵�task_struct
*
* PARAMETERS
*	pFunc: �ص�����
*		TskAddr: task_struct��ַ
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: �����쳣
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachProcAddr(int (*pFunc)(TADDR TskAddr, void *pArg), void *pArg)
{
	TADDR InitTskAddr;

	InitTskAddr = SymName2Addr(pVmSHdl, NULL, "init_task", TGLBVAR);
	if (!InitTskAddr)
		return 1;
	return ForEachList(InitTskAddr + TskInfs.TskOff[TSK_TASKS], TskInfs.TskOff[TSK_TASKS], pFunc, pArg);
}

/*************************************************************************
* DESCRIPTION
*	���������������̵߳�task_struct
*
* PARAMETERS
*	TskAddr: ����task_struct��ַ
*	pFunc: �ص�����
*		TskAddr: task_struct��ַ
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: �����쳣
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachThdAddr(TADDR TskAddr, int (*pFunc)(TADDR TskAddr, void *pArg), void *pArg)
{
/* signal_struct���thread_head��ƫ���� */
#ifdef SST64
#define SIG_THDHEAD_OFF sizeof(TINT) * 4
#else
#define SIG_THDHEAD_OFF	sizeof(TINT) * 3
#endif
	TADDR SigAddr;

	if (DumpMem(TskAddr + TskInfs.TskOff[TSK_SIGNAL], &SigAddr, sizeof(SigAddr)))
		return 1;
	return ForEachList(SigAddr + SIG_THDHEAD_OFF, TskInfs.TskOff[TSK_THDNODE], pFunc, pArg);
}

/*************************************************************************
* DESCRIPTION
*	���SP�Ƿ����
*
* PARAMETERS
*	Sp: stack pointer
*	CpuId: cpu id
*
* RETURNS
*	0: ��������Ϣ����
*	1: ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
int StackIsOverflow(TSIZE Sp, TUINT CpuId)
{
	const TADDR ThdIAddr = Sp2ThdInfAddr(Sp, CpuId);
	TSIZE StkEndMagic;

#ifdef SST64
	if (TskInfs.ThdInTsk)
		return (DumpMem(ThdIAddr, &StkEndMagic, sizeof(StkEndMagic)) ? 0 : StkEndMagic != STACK_END_MAGIC);
#endif
	if (ThdIAddr + TskInfs.ThdSz >= Sp)
		return 1;
	if (TskInfs.ThdSz && !DumpMem(ThdIAddr + TskInfs.ThdSz, &StkEndMagic, sizeof(StkEndMagic)))
		return StkEndMagic != STACK_END_MAGIC;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	��ȡpreempt_disable_ip
*
* PARAMETERS
*	TskAddr: ����task_struct��ַ
*
* RETURNS
*	0: ����/û�п�CONFIG_DEBUG_PREEMPT
*	����: preempt_disable_ipֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR GetTskPreemptDisIp(TADDR TskAddr)
{
	struct _DBG_HDL DbgHdl;
	TADDR DisIp;
	int IpOff;

	if (!TskInfs.HasTskDbgHdl || DbgGetCDbgHdl(&TskInfs.TskDbgHdl, &DbgHdl, "preempt_disable_ip"))
		return 0;
	IpOff = (int)DbgGetOffset(&DbgHdl);
	return (IpOff < 0 || DumpMem(TskAddr + IpOff, &DisIp, sizeof(DisIp)) ? 0 : DisIp);
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��taskģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: taskģ��ر�
*	1: taskģ�����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int TaskInit(void)
{
	static const char *pTskNames[] =
	{
#define _NL(Enum, str) str,
		TSKOFF_MAP2
		TSKOFF_MAP
#undef _NL
	};
	static const char *pThdNames[] =
	{
#define _NL(Enum, str) str,
		THDOFF_MAP2
		THDOFF_MAP
#undef _NL
	};
	struct _DBG_HDL fDbgHdl, DbgHdl, tDbgHdl, t2DbgHdl;
	KEXP_INF * const pKExpInf = GetKExpInf();
	CORE_CNTX *pCpuCntx;
	TSIZE Off;
	TUINT i;

	if (pKExpInf->mType == MRD_T_NONE)
		return 1;
	TskInfs.CurTime = pKExpInf->Sec * 1000000000ULL + pKExpInf->USec * 1000ULL;
#ifdef SST64
	TskInfs.PCpuIrqStkAddr = SymName2Addr(pVmSHdl, NULL, "irq_stack", TGLBVAR);
#endif
	if (!SymFile2DbgHdl(pVmSHdl, &fDbgHdl, "init_task.c")) {
		if (!DbgGetCDbgHdl(&fDbgHdl, &TskInfs.TskDbgHdl, "task_struct")) {
			TskInfs.HasTskDbgHdl = 1;
			TskInfs.TskSz = (TUINT)DbgGetByteSz(&TskInfs.TskDbgHdl);
			if (DbgGetChildOffs(&TskInfs.TskDbgHdl, TskInfs.TskOff, pTskNames, ARY_CNT(pTskNames)))
				SSTWARN(RS_VAR_CHANGE, "task_struct");
			if (!DbgGetCDbgHdl(&TskInfs.TskDbgHdl, &tDbgHdl, "sched_info")) {
				Off = DbgGetOffset(&tDbgHdl);
				if (!DbgGetTDbgHdl(&tDbgHdl, &tDbgHdl) && !DbgGetCDbgHdl(&tDbgHdl, &t2DbgHdl, "last_arrival"))
					TskInfs.WaitOff[0] = (int)(Off + DbgGetOffset(&t2DbgHdl));
			}
			if (!DbgGetCDbgHdl(&TskInfs.TskDbgHdl, &tDbgHdl, "se")) {
				Off = DbgGetOffset(&tDbgHdl);
				if (!DbgGetTDbgHdl(&tDbgHdl, &tDbgHdl) && !DbgGetCDbgHdl(&tDbgHdl, &tDbgHdl, "statistics")) {
					Off += DbgGetOffset(&tDbgHdl);
					if (!DbgGetTDbgHdl(&tDbgHdl, &tDbgHdl)) {
						if (!DbgGetCDbgHdl(&tDbgHdl, &t2DbgHdl, "sleep_start"))
							TskInfs.WaitOff[1] = (int)(Off + DbgGetOffset(&t2DbgHdl));
						if (!DbgGetCDbgHdl(&tDbgHdl, &t2DbgHdl, "block_start"))
							TskInfs.WaitOff[2] = (int)(Off + DbgGetOffset(&t2DbgHdl));
					}
				}
			}
#ifdef SST64
			if (!DbgGetCDbgHdl(&TskInfs.TskDbgHdl, &tDbgHdl, "cpu")) {
				TskInfs.ThdOff[THD_CPU] = (int)DbgGetOffset(&tDbgHdl);
				TskInfs.ThdInTsk = 1;
			}
#endif
		}
		if (!DbgGetCDbgHdl(&fDbgHdl, &DbgHdl, "thread_info")) {
			TskInfs.ThdSz = (TUINT)DbgGetByteSz(&DbgHdl);
#ifdef SST64
			if (TskInfs.ThdInTsk && !DbgGetCDbgHdl(&DbgHdl, &tDbgHdl, pThdNames[THD_PMTCNT])) {
				TskInfs.ThdOff[THD_PMTCNT] = (int)DbgGetOffset(&tDbgHdl);
			} else
#endif
			if (DbgGetChildOffs(&DbgHdl, TskInfs.ThdOff, pThdNames, ARY_CNT(pThdNames)))
				SSTWARN(RS_VAR_CHANGE, "thread_info");
		}
	}
	if (!SymFile2DbgHdl(pVmSHdl, &fDbgHdl, "sched/core.c")) {
		TskInfs.RunQAddr = SymName2Addr(pVmSHdl, NULL, "runqueues", TGLBVAR);
		if (DbgGetCDbgHdl(&fDbgHdl, &DbgHdl, "rq") || DbgGetCDbgHdl(&DbgHdl, &tDbgHdl, "curr"))
			SSTWARN(RS_VAR_CHANGE, "rq");
		else
			TskInfs.CurrOff = (int)DbgGetOffset(&tDbgHdl);
	}
	for (i = pKExpInf->CoreCnt; i-- > 0; ) {
		pCpuCntx = GetCoreCntx(i);
		if (!pCpuCntx)
			continue;
		UpdateOnCpuReg(pCpuCntx, i);
		UpdateOnCpuTsk(pCpuCntx, i);
	}
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	ע��taskģ��
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
void TaskDeInit(void)
{
	
}
