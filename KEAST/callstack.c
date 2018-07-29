/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	callstack.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: ����ջ��ԭ
**********************************************************************************/
#include <stdarg.h>
#include <string.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include <armdis.h>
#include <kernel/task.h>
#include <callstack.h>


#define REG_PARAMASK	(((TSIZE)1 << REG_PNUM) - 1)

typedef struct _BT_FRAME
{
	TSIZE RegAry[REG_NUM], RegMask;

#define BTF_EXT  (1 << 0) /* ��չ֡ */
#define BTF_EXCP (1 << 1) /* �쳣֡ */
	unsigned Flag;
	struct _BT_FRAME *pNext;
} BT_FRAME;

typedef struct _CSF_ARG
{
	CS_FRAME *pLastCsF;
	const BT_FRAME *pBtF;
	BT_FRAME *pCallerBtF;
	int FullInf, InlineCnt;
} CSF_ARG;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�ͷ�����ջ֡
*
* PARAMETERS
*	pBtFHead: ջ֡����ͷ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FreeBtFrames(BT_FRAME *pBtFHead)
{
	BT_FRAME *pBtF;

	while ((pBtF = pBtFHead) != NULL) {
		pBtFHead = pBtFHead->pNext;
		Free(pBtF);
	}
}

/*************************************************************************
* DESCRIPTION
*	����1��ջ֡����������
*
* PARAMETERS
*	ppBtFHead: ջ֡����ͷ
*	pRegAry/RegMask: �Ĵ�����/�Ĵ�����Чλͼ
*	Flag: ��־
*
* RETURNS
*	NULL: ʧ��/OOM
*	����: ջ֡
*
* LOCAL AFFECTED
*
*************************************************************************/
static BT_FRAME *AllocBtFrame(BT_FRAME **ppBtFHead, const TSIZE *pRegAry, TSIZE RegMask, unsigned Flag)
{
	BT_FRAME *pBtF;

	pBtF = Malloc(sizeof(*pBtF));
	if (pBtF) {
		memcpy(pBtF->RegAry, pRegAry, sizeof(pBtF->RegAry));
		pBtF->RegMask = RegMask;
		pBtF->Flag = Flag;
		pBtF->pNext = *ppBtFHead;
		*ppBtFHead = pBtF;
	}
	return pBtF;
}

/*************************************************************************
* DESCRIPTION
*	ͨ��FP��ԭ1��ջ֡
*
* PARAMETERS
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_FP/REG_SP������Ч!!!), �ᱻ����!!!
*	IsLeaf: �Ƿ���Ҷ�Ӻ���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FpUnwind(TSIZE *pRegAry, TSIZE *pRegMask, int IsLeaf)
{
	const TADDR OldFp = pRegAry[REG_FP];
#ifdef SST64
	TSIZE Ary[2], StackBase;
	struct _PT_REGS PtRegs;

	if (DumpMem(OldFp, Ary, sizeof(Ary)))
		return 1;
	/* �ж��Ƿ���Ҷ�Ӻ��� */
	if (IsLeaf && Ary[1] != pRegAry[REG_LR] && pRegAry[REG_PC] - pRegAry[REG_LR] + 0x2000 > 0x4000/* PC/LR����ͬһ���� */) {
		pRegAry[REG_PC] = pRegAry[REG_LR];
		goto _OUT;
	}
	/* ���� */
	pRegAry[REG_FP] = Ary[0];
	StackBase = THDSTK_BASE(pRegAry[REG_FP]);
	if (THDSTK_BASE(OldFp) != StackBase && StackBase == THDSTK_BASE(Ary[1])) { /* �Ƿ���irq_stack�� */
		if (pRegAry[19] == pRegAry[REG_SP] && !DumpMem(pRegAry[REG_SP], &PtRegs, sizeof(PtRegs))) { /* ���˳�irq stack */
			pRegAry[REG_FP] = PtRegs.RegAry[REG_FP];
		} else {
			pRegAry[REG_SP] = Ary[1];
		}
	} else {
		if (OldFp >= pRegAry[REG_FP] && pRegAry[REG_FP]) /* �����FP */
			return 1;
		pRegAry[REG_PC] = Ary[1];
		pRegAry[REG_SP] = (OldFp + (REG_NUM + 3) * sizeof(TSIZE) <= Ary[0] ? Ary[0] - (REG_NUM + 3) * sizeof(TSIZE)/* sizeof(pt_regs) */ : Ary[0]);
	}
_OUT:
#else
	unsigned short Mask;
	VMA_HDL *pVmaHdl;
	const TSIZE *p;
	TSIZE Inst[2];
	int i, j;
	
	pVmaHdl = GetVmaHdl(OldFp);
	if (!pVmaHdl || (p = Vma2Mem(pVmaHdl, OldFp, sizeof(TSIZE))) == NULL) {
		if (pVmaHdl)
			PutVmaHdl(pVmaHdl);
		return 1;
	}
	/* �ж��Ƿ���Ҷ�Ӻ��� */
	if (IsLeaf && pRegAry[REG_PC] - p[0] + 0x2000 > 0x4000/* PC/pc����ͬһ���� */ && pRegAry[REG_LR] - p[0] < 0x2000 /* LR/pc��ͬһ���� */)
		goto _OUT;
	/* ��ջȡ���Ĵ�����Ϣ */
	Mask = 0xD800;
	if (!DumpMem(p[0] - 3 * sizeof(TADDR), Inst, 2 * sizeof(TSIZE)) && (Inst[1] >> 16) == 0xE92D) { /* ��ȡѹջָ�� */
		Mask = Inst[1]&0xFFFF;
		if ((Inst[0] >> 4) == 0xE92D000) /* PUSHָ�� */
			Mask |= Inst[0]&0xF;
	}
	*pRegMask |= Mask;
	for (j = 0, i = 15; Mask; Mask <<= 1, i--) {
		if (Mask&(1 << 15))
			pRegAry[i] = p[j--];
	}
	/* ���� */
	pRegAry[REG_SP] = pRegAry[12];
_OUT:
	pRegAry[REG_PC] = pRegAry[REG_LR];
	PutVmaHdl(pVmaHdl);
#endif
	*pRegMask &= ~TMPREG_MASK;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	��ԭ1���쳣ջ֡
*
* PARAMETERS
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_FP/REG_SP������Ч!!!), �ᱻ����!!!
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ExcpUnwind(TSIZE *pRegAry, TSIZE *pRegMask)
{
	PT_REGS *pPtRegs;
	VMA_HDL *pVmaHdl;
	int rt = 1;

	if (pRegAry[REG_SP] + sizeof(*pPtRegs) > pRegAry[REG_FP] || (pVmaHdl = GetVmaHdl(pRegAry[REG_SP])) == NULL)
		return 1;
	pPtRegs = Vma2Mem(pVmaHdl, pRegAry[REG_SP], sizeof(*pPtRegs));
	if (pPtRegs && pPtRegs->RegAry[REG_FP] == pRegAry[REG_FP] && pPtRegs->RegAry[REG_SP] >= pRegAry[REG_SP] + sizeof(*pPtRegs)) { /* ���쳣֡ */
		memcpy(pRegAry, pPtRegs->RegAry, sizeof(pPtRegs->RegAry));
#ifndef SST64
		pRegAry[REG_PC] |= ((pPtRegs->Cpsr >> 5)&1); /* ���THUMB bit */
#endif
		*pRegMask = REG_ALLMASK;
		rt = 0;
	}
	PutVmaHdl(pVmaHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	��ȡ����ջ
*
* PARAMETERS
*	ppBtFHead: ����ջ֡����ͷ
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_SP������Ч!!!), �ᱻ����!!!
*
* RETURNS
*	0: �ɹ�/���
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int GetBacktrace(BT_FRAME **ppBtFHead, TSIZE *pRegAry, TSIZE *pRegMask)
{
	int rt = 1, IsRbPc = !!*ppBtFHead;
	VMA_HDL *pVmaHdl = NULL;

	do {
		/* ���PC */
		pVmaHdl = GetVmaHdl(pRegAry[REG_PC]);
		if (!pVmaHdl || !(pVmaHdl->Flag&VMA_X)) { /* �������κ�1���ڴ��������ִ��Ȩ�� */
			rt = !!AllocBtFrame(ppBtFHead, pRegAry, *pRegMask, 0);
			goto _OUT;
		}
		/* �ع�PC */
		if (IsRbPc)
			pRegAry[REG_PC] -= 4;
		/* ����ջ֡ */
		if (!AllocBtFrame(ppBtFHead, pRegAry, *pRegMask, 0))
			goto _OUT;
		/* ��ԭ��1֡ */
		if (VMA_TYPE(pVmaHdl) == VMA_ELF) {
			SymForceLoad(pVmaHdl->pHdl);
			if (SymUnwind(pVmaHdl->pHdl, pRegAry, pRegMask) && FpUnwind(pRegAry, pRegMask, !IsRbPc))
				goto _OUT;
		} else if (FpUnwind(pRegAry, pRegMask, !IsRbPc)) {
			goto _OUT;
		}
		IsRbPc = 1;
		if (!ExcpUnwind(pRegAry, pRegMask)) {
			IsRbPc = 0;
			(*ppBtFHead)->Flag = BTF_EXCP;
		}
	} while (PutVmaHdl(pVmaHdl), pVmaHdl = NULL, pRegAry[REG_FP]);
	rt = !AllocBtFrame(ppBtFHead, pRegAry, *pRegMask, 0);
_OUT:
	if (pVmaHdl)
		PutVmaHdl(pVmaHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	����Ƿ��Ƿ��ص�ַ
*
* PARAMETERS
*	RtAddr: LRֵ
*
* RETURNS
*	0: ����
*	1: ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int IsReturnAddr(TADDR RtAddr)
{
	VMA_HDL *pVmaHdl;
	const void *pInst;
	TUINT Inst;
	int rt = 0;

	if ((RtAddr&3)/* ����4�ֽڶ��� */ || (pVmaHdl = GetVmaHdl(RtAddr)) == NULL)
		return rt;
	if (!(pVmaHdl->Flag&VMA_X) || VMA_TYPE(pVmaHdl) != VMA_ELF)
		goto _OUT;
	pInst = Vma2Mem(pVmaHdl, RtAddr - sizeof(Inst), sizeof(Inst));
	if (!pInst)
		goto _OUT;
	Inst = OFFVAL(TUINT, pInst, 0);
#ifdef SST64
	if ((Inst&0xFC000000) == 0x94000000 || (Inst&0xFFFFFC1F) == 0xD63F0000) /* BL #26/BLR Rn */
		rt = 1;
#else
	if ((Inst&0xFE000000) == 0xFA000000 || (Inst&0x0F000000) == 0x0B000000
	  || (Inst&0x0FFFFFF0) == 0x012FFF30 || (Inst&0x0E70F000) == 0x0410F000) /* A32: BLX #25/BL #24/BLX Rm/LDR PC, [Rm, #off] */
		rt = 1;
#endif
_OUT:
	PutVmaHdl(pVmaHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	���Ի�ԭ����ջ״̬
*
* PARAMETERS
*	ppBtFHead: ����ջ֡����ͷ
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_SP������Ч!!!), �ᱻ����!!!
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int TryRewindReg(BT_FRAME **ppBtFHead, TSIZE *pRegAry, TSIZE *pRegMask)
{
	int HasExeAttr, rt = 1;
	const TADDR *pVal;
	VMA_HDL *pVmaHdl;
	TADDR Sp, SpEnd;
	BT_FRAME *pBtF;

	if (*ppBtFHead && !(*ppBtFHead)->pNext /* ��1֡ */
	  && (*pRegMask&((TSIZE)1 << REG_LR)) && !(pRegAry[REG_LR]&3) && (pVmaHdl = GetVmaHdl(pRegAry[REG_LR])) != NULL) { /* Ҷ�Ӻ��� */
		HasExeAttr = (pVmaHdl->Flag&VMA_X);
		PutVmaHdl(pVmaHdl);
		if (HasExeAttr) {
			*pRegMask &= ~TMPREG_MASK;
			pRegAry[REG_PC] = pRegAry[REG_LR];
			return 0;
		}
	}
	for (pBtF = *ppBtFHead; pBtF; pBtF = pBtF->pNext) { /* ����Ƿ����Sp���ƣ�������ѭ�� */
		if (!(pBtF->Flag&BTF_EXT))
			continue;
		if (pRegAry[REG_SP] < pBtF->RegAry[REG_SP])
			return 1;
		break;
	}
	SpEnd = THDSTK_BASE(pRegAry[REG_SP]) + THREAD_SIZE - sizeof(PT_REGS) - 2 * sizeof(TSIZE);
	Sp = (pRegAry[REG_SP] + 2 * sizeof(Sp) - 1)&~(TSIZE)(2 * sizeof(Sp) - 1); /* ջ���� */
	pVmaHdl = GetVmaHdl(Sp);
	if (!pVmaHdl)
		return 1;
#ifdef SST64
	do {
		pVal = Vma2Mem(pVmaHdl, Sp, 2 * sizeof(*pVal)); /* PUSH FP,LR */
		Sp += 2 * sizeof(*pVal);
		if (!pVal || Sp >= SpEnd)
			goto _OUT;
	} while ((pVal[0]&(2 * sizeof(Sp) - 1)) || pVal[0] <= Sp || !IsReturnAddr(pVal[1]));
	pRegAry[REG_FP] = pRegAry[REG_SP] = pVal[0];
	pRegAry[REG_PC] = pVal[1];
#else
	do {
		pVal = Vma2Mem(pVmaHdl, Sp, 4 * sizeof(*pVal)); /* PUSH FP,SP,LR,PC */
		Sp += 2 * sizeof(*pVal);
		if (!pVal || Sp >= SpEnd)
			goto _OUT;
	} while (pVal[1] != Sp + 2 * sizeof(*pVal) || !IsReturnAddr(pVal[2]));
	pRegAry[REG_FP] = pVal[0];
	pRegAry[REG_SP] = pVal[1];
	pRegAry[REG_PC] = pVal[2];
#endif
	*pRegMask = ((TSIZE)1 << REG_FP)|((TSIZE)1 << REG_SP)|((TSIZE)1 << REG_PC);
	AllocBtFrame(ppBtFHead, pRegAry, 0, BTF_EXT);
	rt = 0;
_OUT:
	PutVmaHdl(pVmaHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	��亯������
*
* PARAMETERS
*	pParaName: ����������, ����ΪNULL
*	Idx: ��������, ��0��ʼ
*	Type: ������������
*	Val: ��������ֵ
*	pArg: ����
*
* RETURNS
*	0: ����
*	2: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FillCsPara(const char *pParaName, int Idx, BASE_TYPE Type, BASE_VAL Val, void *pArg)
{
#ifdef SST64
#define CST_NUM			 CS_I64
#else
#define CST_NUM			 CS_INT
#endif
	static int AllocCnt;
	struct _CS_FRAME *pCsF = *(void **)pArg;
	struct _CS_PARA *pPara = (void *)pCsF->Para;

	if (!pCsF->ArgCnt)
		AllocCnt = 0;
	if (AllocCnt <= Idx) {
		AllocCnt = Idx + 4;
		pCsF = Realloc(pCsF, sizeof(*pCsF) + AllocCnt * sizeof(pPara[0]));
		if (!pCsF)
			return 2;
		*(void **)pArg = pCsF;
		pPara = (void *)pCsF->Para;
	}
	do {
		pPara[pCsF->ArgCnt++].Type = CS_NOVAL;
	} while (pCsF->ArgCnt <= (unsigned)Idx);
	pPara[Idx].pName = pParaName;
	pPara[Idx].Type = Type;
	pPara[Idx].Val = Val;
	if (Type == CS_ADDR && (TSSIZE)Val.p > -32768 && (TSSIZE)Val.p < 32768)
		pPara[Idx].Type = CST_NUM;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	����������Ϣ
*
* PARAMETERS
*	pCsF: ջ��Ϣ
*	pSymHdl: sym���, ����ΪNULL
*	pDbgHdl: ����dbg���
*	pBtF: ջ֡
*	pCallerBtF: ������ջ֡, ���ڲ�����ԭ, ���ܻᱻ�ƻ�!!!
*
* RETURNS
*	ջ��Ϣ
*
* LOCAL AFFECTED
*
*************************************************************************/
static CS_FRAME *BuildCsPara(struct _CS_FRAME *pCsF, SYM_HDL *pSymHdl, DBG_HDL *pDbgHdl, const BT_FRAME *pBtF, BT_FRAME *pCallerBtF)
{
	const TSIZE CParaMask2 = pCallerBtF->RegMask&REG_PARAMASK;
	TSIZE CParaMask = 0;
	BASE_VAL Val = {0};
	int i;

	if (!(pCallerBtF->Flag&BTF_EXT))
		CParaMask = RegDerivation(pCsF->pNext->FAddr, pCallerBtF->RegAry, pCallerBtF->RegMask^CParaMask2/* �����Ĵ���ͨ�����Ȼ��� */, REG_PARAMASK)&REG_PARAMASK;
	if (pSymHdl && SymGetFuncPara(pSymHdl, pDbgHdl, FillCsPara, pBtF->RegAry, pBtF->RegMask, pCallerBtF->RegAry, CParaMask, CParaMask2, &pCsF) != 1)
		return pCsF;
	for (CParaMask |= CParaMask2, i = 0; CParaMask; i++) { /* �Ĵ������� */
		if (!(CParaMask&((TSIZE)1 << i)))
			continue;
		CParaMask &= ~((TSIZE)1 << i);
		Val.p = pCallerBtF->RegAry[i];
		if (FillCsPara(NULL, i, CS_ADDR, Val, &pCsF))
			break;
	}
	return pCsF;
}

/*************************************************************************
* DESCRIPTION
*	����sym�������1֡ջ��Ϣ
*
* PARAMETERS
*	pSymHdl: sym���
*	FAddr: �����׵�ַ
*	pFName: ������
*	pDbgHdl: ����dbg���
*	pArg: ����
*
* RETURNS
*	0: ����
*	2: OOM
*
* LOCAL AFFECTED
*
*************************************************************************/
static int BuildSymCsFrame(SYM_HDL *pSymHdl, TADDR FAddr, const char *pFName, DBG_HDL *pDbgHdl, void *pArg)
{
	CSF_ARG * const pCsfArg = pArg;
	struct _CS_FRAME *pCsF, * const pLastCsF = (void *)pCsfArg->pLastCsF;
	const BT_FRAME * const pBtF = pCsfArg->pBtF;

	pCsF = Calloc(sizeof(*pCsF));
	if (!pCsF)
		return 2;
	pCsfArg->pLastCsF = pCsF;
	if (!pCsfArg->InlineCnt)
		pCsF->Flag = FLAG_TRANS(pBtF->Flag, BTF_EXCP, CS_EXCP);
	pCsF->pNext = pLastCsF;
	pCsF->FAddr = FAddr;
	pCsF->Off = (TUINT)(pBtF->RegAry[REG_PC] - FAddr);
	pCsF->pMName = pSymHdl->pName;
	pCsF->pFName = pFName;
	if (!pCsfArg->FullInf)
		return 0;
	if (pCsfArg->InlineCnt++) {
		pCsF->Flag |= CS_INLINE;
		pCsF->pPath = pLastCsF->pPath;
		pCsF->LineNo = pLastCsF->LineNo;
		pLastCsF->pPath = DbgGetLineInf(pDbgHdl, &pLastCsF->LineNo);
	} else {
		pCsF->pPath = SymAddr2Line(pSymHdl, &pCsF->LineNo, pBtF->RegAry[REG_PC]);
		if (pCsF->Flag&CS_EXCP)
			return 0;
		if (pCsfArg->pCallerBtF) {
			pCsfArg->pLastCsF = BuildCsPara(pCsF, pSymHdl, pDbgHdl, pBtF, pCsfArg->pCallerBtF);
			return 0;
		}
	}
	SymGetFuncPara(pSymHdl, pDbgHdl, FillCsPara, pBtF->RegAry, pBtF->RegMask, NULL, 0, 0, &pCsF);
	pCsfArg->pLastCsF = pCsF;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	����1֡ջ��Ϣ
*
* PARAMETERS
*	pLastCsF: ǰһջ��Ϣ
*	pBtF: ջ֡
*	pCallerBtF: ������ջ֡(����ΪNULL), ���ڲ�����ԭ, ���ܻᱻ�ƻ�!!!
*	FullInf: ���ȫ����Ϣ, ����û���ļ���/�к�, ����
*
* RETURNS
*	ջ��Ϣ
*
* LOCAL AFFECTED
*
*************************************************************************/
static CS_FRAME *BuildCsFrame(CS_FRAME *pLastCsF, const BT_FRAME *pBtF, BT_FRAME *pCallerBtF, SYM_HDL *pSymHdl, int FullInf)
{
	struct _CS_FRAME *pCsF;
	VMA_HDL *pVmaHdl;
	const char *pStr;

	pCsF = Calloc(sizeof(*pCsF));
	if (!pCsF)
		return pLastCsF;
	pCsF->pNext = pLastCsF;
	pCsF->pMName = "......";
	if (pBtF->Flag&BTF_EXT)
		return pCsF;
	pCsF->Flag = FLAG_TRANS(pBtF->Flag, BTF_EXCP, CS_EXCP);
	pCsF->FAddr = pBtF->RegAry[REG_PC];
	if (pSymHdl) {
		pCsF->pMName = pSymHdl->pName;
		pStr = SymAddr2Name(pSymHdl, &pCsF->FAddr, pBtF->RegAry[REG_PC], TALLFUN);
		if (pStr && pStr != pUndFuncName)
			pCsF->pFName = pStr;
		pCsF->Off = (TUINT)(pBtF->RegAry[REG_PC] - pCsF->FAddr);
		if (FullInf)
			pCsF->pPath = SymAddr2Line(pSymHdl, &pCsF->LineNo, pBtF->RegAry[REG_PC]);
	} else if ((pVmaHdl = GetVmaHdl(pBtF->RegAry[REG_PC])) != NULL){
		pStr = (pVmaHdl->pName ? strdup(pVmaHdl->pName) : NULL);
		if (pStr) {
			pCsF->pMName = pStr;
			pCsF->Flag |= CS_MNAME;
		}
		PutVmaHdl(pVmaHdl);
	}
	return (FullInf && pCallerBtF && !(pCsF->Flag&CS_EXCP) ? BuildCsPara(pCsF, NULL, NULL, pBtF, pCallerBtF) : pCsF);
}

/*************************************************************************
* DESCRIPTION
*	��ȡ��ʽ���ַ�������, �������Ϣ!!!
*
* PARAMETERS
*	fp: �ļ����
*	pFmt: ��ʽ���ַ���
*
* RETURNS
*	����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int NullPrint(FILE *fp, const char *pFmt, ...)
{
	int rt;
	va_list ap;

	(void)fp;
	va_start(ap, pFmt);
	rt = vsnprintf(NULL, 0, pFmt, ap);
	va_end(ap);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	��ʾ1��ջ֡
*
* PARAMETERS
*	pDispFunc: ��ʾ�ص�����
*	pMWidth: �����ڴ���������󳤶�
*	pFWidth: ���غ�������󳤶�
*	pCsF: ջ֡
*	fp: �ļ����
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispCsFrame(int (*pDispFunc)(FILE *fp, const char *pFmt, ...), int *pMWidth, int *pFWidth, CS_FRAME *pCsF, void *fp)
{
	const char * const pParaStr = GetCStr(RS_PARA);
	CS_PARA * const pPara = pCsF->Para;
	int Size, Flag;
	unsigned i;

	Size = pDispFunc(fp, "\t%-*s%c ", *pMWidth, pCsF->pMName, pCsF->Flag&CS_EXCP ? '>' : ' ') - 3;
	if (Size > *pMWidth)
		*pMWidth = Size;
	if (!pCsF->FAddr) { /* ��չ֡ */
		pDispFunc(fp, "......\n");
		return;
	}
	Size = (pCsF->pFName ? pDispFunc(fp, "%s(", pCsF->pFName) : pDispFunc(fp, TADDRFMT"(", pCsF->FAddr));
	if (!pPara)
		goto _NO_PARA;
	for (Flag = 0, i = 0; i < pCsF->ArgCnt; i++) {
		if (pPara[i].Type == CS_NOVAL)
			continue;
		if (Flag)
			Size += pDispFunc(fp, ", ");
		Flag = 1;
		if (pPara[i].pName)
			Size += pDispFunc(fp, "%s=", pPara[i].pName);
		else
			Size += pDispFunc(fp, "%s%d=", pParaStr, i + 1);
		switch (pPara[i].Type) {
		case CS_BOOL:   Size += pDispFunc(fp, pPara[i].Val.n ? "true" : "false"); break;
		case CS_INT:	Size += pDispFunc(fp, "%d", pPara[i].Val.i); break;
		case CS_UINT:   Size += pDispFunc(fp, "%u", pPara[i].Val.n); break;
		case CS_I64:	Size += pDispFunc(fp, "%lld", pPara[i].Val.j); break;
		case CS_FLOAT:  Size += pDispFunc(fp, "%f", pPara[i].Val.f); break;
		case CS_DOUBLE: Size += pDispFunc(fp, "%f", pPara[i].Val.d); break;
		case CS_STRING: Size += pDispFunc(fp, "%s", pPara[i].Val.pStr); break;
		default: Size += ShowSymVal(pDispFunc, fp, pPara[i].Val.p);
		}
	}
_NO_PARA:
	pDispFunc(fp, ")");
	if (pCsF->Off)
		Size += pDispFunc(fp, " + %d", pCsF->Off);
	if (Size > *pFWidth)
		*pFWidth = Size;
	if (pCsF->pPath)
		pDispFunc(fp, "%*c <%s:%d>\n", *pFWidth - Size + 1, ' ', pCsF->pPath, pCsF->LineNo);
	else
		pDispFunc(fp, "\n");
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʾ����ջ
*
* PARAMETERS
*	fp: �ļ����
*	pCsHdl: cs���
*	pHeadStr: ���ͷ��Ϣ
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispCallStack(FILE *fp, CS_HDL *pCsHdl, const char *pHeadStr)
{
	int MWidth, FWidth, HasFlag = 0;
	CS_FRAME *pCsF;
	
	for (MWidth = FWidth = 0, pCsF = pCsHdl; pCsF; pCsF = pCsF->pNext) { /* ��ȡ��� */
		if (pCsF->Flag&CS_EXCP)
			HasFlag = 1;
		DispCsFrame(NullPrint, &MWidth, &FWidth, pCsF, NULL);
	}
	if (HasFlag)
		MWidth++;
	LOGC(fp, pHeadStr);
	for (pCsF = pCsHdl; pCsF; pCsF = pCsF->pNext) {
		DispCsFrame(fprintf, &MWidth, &FWidth, pCsF, fp);
	}
	LOG(fp, RS_CS_END);
}

/*************************************************************************
* DESCRIPTION
*	��ȡ���ص���ջ, ע��: pRegAry[REG_SP],pRegAry[REG_PC]����������ȷ��ֵ!!!
*
* PARAMETERS
*	pRegAry: �Ĵ�����(REG_NUM���Ĵ���)
*	Flag: ��Ϣ����
*		bit0: ��ʾ�ļ���/�к�, ����
*		bit1: ��ʾinline����
*
* RETURNS
*	NULL: ʧ��
*	����: ����ջ������ʱ�����PutCsHdl()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
CS_HDL *GetNativeCsHdl(const TSIZE *pRegAry, int Flag)
{
	TSIZE RegAry[REG_NUM], RegMask = REG_ALLMASK;
	BT_FRAME *pBtFHead = NULL, *pBtF, *pCallerBtF;
	CS_FRAME *pCsF = NULL;
	SYM_HDL *pSymHdl;
	CSF_ARG CsfArg;
	int IsInline;

	memcpy(RegAry, pRegAry, sizeof(RegAry));
	while (GetBacktrace(&pBtFHead, RegAry, &RegMask) && !TryRewindReg(&pBtFHead, RegAry, &RegMask));
	/* ת����ʽ */
	IsInline = Flag&2;
	CsfArg.FullInf = Flag&1;
	for (pBtF = pBtFHead, pCallerBtF = NULL; pBtF; pCallerBtF = pBtF, pBtF = pBtF->pNext) {
		pSymHdl = NULL;
		if (!(pBtF->Flag&BTF_EXT) && (pSymHdl = GetSymHdlByAddr(pBtF->RegAry[REG_PC])) != NULL && IsInline) {
			CsfArg.InlineCnt = 0;
			CsfArg.pLastCsF = pCsF;
			CsfArg.pBtF = pBtF;
			CsfArg.pCallerBtF = pCallerBtF;
			if (SymAddr2Func(pSymHdl, pBtF->RegAry[REG_PC], BuildSymCsFrame, &CsfArg) != 1) {
				pCsF = CsfArg.pLastCsF;
				continue;
			}
		}
		pCsF = BuildCsFrame(pCsF, pBtF, pCallerBtF, pSymHdl, CsfArg.FullInf);
	}
	FreeBtFrames(pBtFHead);
	return pCsF;
}

/*************************************************************************
* DESCRIPTION
*	����LR���齨�����ص���ջ
*
* PARAMETERS
*	pBts/Cnt: LR����/����
*
* RETURNS
*	NULL: ʧ��
*	����: ����ջ������ʱ�����PutCsHdl()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
CS_HDL *Bt2NativeCsHdl(const TADDR *pBts, TUINT Cnt)
{
	CS_FRAME *pCsF = NULL;
	SYM_HDL *pSymHdl;
	CSF_ARG CsfArg;
	BT_FRAME BtF;
	
	memset(&BtF, 0, sizeof(BtF));
	BtF.RegMask = ((TSIZE)1 << REG_PC);
	CsfArg.FullInf = 1;
	while (Cnt-- > 0) {
		BtF.RegAry[REG_PC] = pBts[Cnt] - 4/* LRǰ1��ָ���ַ, ����֧��thumb */;
		pSymHdl = GetSymHdlByAddr(BtF.RegAry[REG_PC]);
		if (pSymHdl) {
			CsfArg.InlineCnt = 0;
			CsfArg.pLastCsF = pCsF;
			CsfArg.pBtF = &BtF;
			CsfArg.pCallerBtF = NULL;
			if (SymAddr2Func(pSymHdl, BtF.RegAry[REG_PC], BuildSymCsFrame, &CsfArg) != 1) {
				pCsF = CsfArg.pLastCsF;
				continue;
			}
		}
		pCsF = BuildCsFrame(pCsF, &BtF, NULL, pSymHdl, 1);
	}
	return pCsF;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�cs���
*
* PARAMETERS
*	pCsHdl: cs���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutCsHdl(CS_HDL *pCsHdl)
{
	struct _CS_FRAME *pCsF;

	do {
		pCsF = (void *)pCsHdl;
		pCsHdl = pCsHdl->pNext;
		if (pCsF->Flag&CS_MNAME)
			Free((char *)pCsF->pMName);
		Free((void *)pCsF->pPath);
		Free(pCsF);
	} while (pCsHdl);
}

/*************************************************************************
* DESCRIPTION
*	��ȡ��1֡�쳣֡�ļĴ�����Ϣ, ����mrdump����ջ!
*
* PARAMETERS
*	pRegAry: ���ؼĴ�����(REG_NUM���Ĵ���)
*
* RETURNS
*	�Ĵ�����Чλͼ, 0��ʾʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE GetFirstExcpCntx(TSIZE *pRegAry)
{
	BT_FRAME *pBtFHead = NULL, *pBtF, *pTmp = NULL, *pExcp = NULL;
	TSIZE RegMask = REG_ALLMASK;
	TSIZE RegAry[REG_NUM];

	memcpy(RegAry, pRegAry, sizeof(RegAry));
	GetBacktrace(&pBtFHead, RegAry, &RegMask);
	for (pBtF = pBtFHead; pBtF; pTmp = pBtF, pBtF = pBtF->pNext) {
		if (pBtF->Flag&BTF_EXCP)
			pExcp = pTmp;
	}
	if (pExcp) {
		memcpy(pRegAry, pExcp->RegAry, sizeof(pExcp->RegAry));
		RegMask = pExcp->RegMask;
	} else {
		RegMask = 0;
	}
	FreeBtFrames(pBtFHead);
	return RegMask;
}
