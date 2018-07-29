/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	dwframe.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/22
* Description: .debug_frame����(֧��dwarf2~4)
**********************************************************************************/
#include <string.h>
#include "string_res.h"
#include "os.h"
#include <module.h>
#include <memspace.h>
#include "dwarf_int.h"
#include "dwframe.h"


typedef struct _DWF_INT
{
	FP_HDL *pFpHdl;
	const void *pBase;
	TSIZE Size;
} DWF_INT;

typedef enum _DWF_RULE
{
	DWF_UNDEF,
	DWF_SAME,
	DWF_SPOFF,
	DWF_REG,
	DWF_EXPR,
} DWF_RULE;

typedef struct _DWF_TREG
{
	union
	{
		TSIZE Off;
		TUINT Idx;
		const void *pInst;
	} u;
	DWF_RULE Rule;
} DWF_TREG;

typedef struct _DWF_FUNC
{
	DWF_TREG RegAry[REG_NUM];
	TSIZE SpOff;
} DWF_FUNC;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���Ҷ�Ӧ��ַ��FDE�����������Ϣ
*
* PARAMETERS
*	pFde: ����FDE�ṹ��
*	pBase/pEnd: .debug_frame[��ʼ��ַ, ������ַ)
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��/�Ҳ���
*	����: CIE buffer
*
* LOCAL AFFECTED
*
*************************************************************************/
static const void *LinearFindFde(FDE_INF *pFde, const void *pBase, const void *pEnd, TADDR Addr)
{
	const struct
	{
		TUINT Len;
		TINT CieOff; /* .debug_frame: -1, .eh_frame: 0��ʾ����CIE */
		TADDR FAddr; /* ����ͷ��ַ */
		TSIZE Size; /* ������С(�ֽ�) */
		/* ʣ�ಿ��
		char Insts[];
		*/
	} *pFde32, *pNext;

	for (pFde32 = pBase; (void *)pFde32 < pEnd && pFde32->Len; pFde32 = pNext) {
#ifdef SST64
		if (pFde32->Len != ~(TUINT)0) {
#endif
			pNext = (void *)((char *)pFde32 + pFde32->Len + sizeof(pFde32->Len));
			if (pFde32->CieOff != -1/* ����CIE */ && Addr - 1 >= pFde32->FAddr - 1 && Addr < pFde32->FAddr + pFde32->Size) {
				pFde->FAddr  = (TADDR)pFde32->FAddr;
				pFde->Size   = (TSIZE)pFde32->Size;
				pFde->pSInst = (void *)(pFde32 + 1);
				pFde->pEInst = (void *)pNext;
				return (char *)pBase + pFde32->CieOff;
			}
#ifdef SST64
		} else {
			const struct
			{
				TSIZE Len;
				TSSIZE CieOff; /* .debug_frame: -1, .eh_frame: 0��ʾ����CIE */
				TADDR FAddr; /* ����ͷ��ַ */
				TSIZE Size; /* ������С(�ֽ�) */
			} *pFde64 = (void *)((char *)pFde32 + sizeof(pFde32->Len));

			pNext = (void *)((char *)pFde64 + pFde64->Len + sizeof(pFde64->Len));
			if (pFde64->CieOff != -1/* ����CIE */ && Addr - 1 >= pFde64->FAddr - 1 && Addr < pFde64->FAddr + pFde64->Size) {
				pFde->FAddr  = pFde64->FAddr;
				pFde->Size   = pFde64->Size;
				pFde->pSInst = (void *)(pFde64 + 1);
				pFde->pEInst = (void *)pNext;
				return (char *)pBase + pFde64->CieOff;
			}
		}
#endif
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	���Ҷ�Ӧ��ַ��FDE
*
* PARAMETERS
*	pDwf: dwf���
*	pFde: ����FDE�ṹ��
*	Addr: ��ַ
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��/�Ҳ���
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DwSearchFde(DWF_INT *pDwf, FDE_INF *pFde, TADDR Addr)
{
	const void *pCie;

	pFde->Cie.pName = pDwf->pFpHdl->pPath;
	pFde->Cie.pSeg  = ".debug_frame";
	pCie = LinearFindFde(pFde, pDwf->pBase, (char *)pDwf->pBase + pDwf->Size, Addr);
	return pCie ? ExtractCie(&pFde->Cie, pCie, NULL) : 1;
}

/*************************************************************************
* DESCRIPTION
*	��ȡĿ���ַ
*
* PARAMETERS
*	pCie: CIE�ṹ��
*	pAddr: ����Ŀ���ַ
*	p: buffer
*
* RETURNS
*	��ȡ���buffer
*
* LOCAL AFFECTED
*
*************************************************************************/
static const void *DwGetTAddr(const CIE_INF *pCie, TADDR *pAddr, const void *p)
{
	(void)pCie;
	*pAddr = *(TADDR *)p;
	return (TADDR *)p + 1;
}

/*************************************************************************
* DESCRIPTION
*	����CFIָ��
*
* PARAMETERS
*	pFde: FDE�ṹ��
*	pRFunc: ���ؼĴ�������
*	pOrgRFunc: ��ʼ�Ĵ�������
*	Pc: ��������ַ��THUMB bit��PC, 0: ����CIE��CFIָ��, ����: ����FDE��CFIָ��
*	pGetTAddrFunc: ��ȡĿ���ַ�ص�����, ���ض�ȡ���buffer
*		pCie: CIE�ṹ��
*		pAddr: ����Ŀ���ַ
*		p: buffer
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ParseCfi(const FDE_INF *pFde, DWF_FUNC *pRFunc, DWF_FUNC *pOrgRFunc, TADDR Pc, const void *(*pGetTAddrFunc)(const CIE_INF *pCie, TADDR *pAddr, const void *p))
{
#define TREG_UNDEF(Idx)	   do { if ((Idx) < REG_NUM) { pTReg[Idx].Rule = DWF_UNDEF; } } while (0)
#define TREG_SAME(Idx)		do { if ((Idx) < REG_NUM) { pTReg[Idx].Rule = DWF_SAME; } } while (0)
#define TREG_SPOFF(Idx, Rel)  do { if ((Idx) < REG_NUM) { pTReg[Idx].Rule = DWF_SPOFF; pTReg[Idx].u.Off = Rel; } } while (0)
#define TREG_REG(idx, tIdx)   do { if ((idx) < REG_NUM) { pTReg[idx].Rule = DWF_REG; if ((tIdx) >= REG_NUM) goto _ERR1; pTReg[idx].u.Idx = (TUINT)(tIdx); } } while (0)
#define TSP_REG(tIdx, Rel) do { pTReg[REG_SP].Rule = DWF_REG; if ((tIdx) >= REG_NUM) goto _ERR1; pTReg[REG_SP].u.Idx = (TUINT)(tIdx); pRFunc->SpOff = Rel; } while (0)
#define TREG_EXPR(Idx, Ptr) do { if ((Idx) < REG_NUM) { pTReg[Idx].Rule = DWF_EXPR; pTReg[Idx].u.pInst = Ptr; } } while (0)
#define DWF_OP_MASK 0xC0
	typedef enum _DWF_OP
	{
		DW_CFA_nop,
		DW_CFA_set_loc,
		DW_CFA_advance_loc1,
		DW_CFA_advance_loc2,
		DW_CFA_advance_loc4,
		DW_CFA_offset_extended,
		DW_CFA_restore_extended,
		DW_CFA_undefined,
		DW_CFA_same_value,
		DW_CFA_register,
		DW_CFA_remember_state,
		DW_CFA_restore_state,
		DW_CFA_def_cfa,
		DW_CFA_def_cfa_register,
		DW_CFA_def_cfa_offset,
		DW_CFA_def_cfa_expression,
		DW_CFA_expression,
		DW_CFA_offset_extended_sf,
		DW_CFA_def_cfa_sf,
		DW_CFA_def_cfa_offset_sf,
		DW_CFA_val_offset,
		DW_CFA_val_offset_sf,
		DW_CFA_val_expression,
		DW_CFA_lo_user		  = 0x1C,
		DW_CFA_GNU_window_save  = 0x2D,
		DW_CFA_GNU_args_size	= 0x2E,
		DW_CFA_GNU_negative_offset_extended = 0x2F,
		DW_CFA_hi_user		  = 0x3c,
		DW_CFA_advance_loc = 0x40,
		DW_CFA_offset	  = 0x80,
		DW_CFA_restore	 = 0xc0,
	} DWF_OP;
	typedef struct _DWF_FUNC_STK
	{
		DWF_FUNC RFunc;
		struct _DWF_FUNC_STK *pNext;
	} DWF_FUNC_STK;
	DWF_TREG * const pTReg = pRFunc->RegAry;
	const TSSIZE Da = pFde->Cie.DataAlign;
	const TSIZE Ca = pFde->Cie.CodeAlign;
	DWF_FUNC_STK *pFStk = NULL, *ptFStk;
	const unsigned char *p, *q;
	unsigned char Op;
	TSIZE Idx, Val = 0;
	TSSIZE sVal;
	TADDR CurPc;
	int rt;

	if (Pc) {
		CurPc = pFde->FAddr;
		p = pFde->pSInst;
		q = pFde->pEInst;
	} else {
		CurPc = 0;
		Pc  = ~(TADDR)0;
		p = pFde->Cie.pSInst;
		q = pFde->Cie.pEInst;
	}
	for (rt = 1; p < q && CurPc <= Pc; ) {
		Op = *p++;
		if (Op&DWF_OP_MASK) {
			Val = Op&~DWF_OP_MASK;
			Op &= DWF_OP_MASK;
		}
		switch (Op) {
		/* ����CurPc */
		case DW_CFA_advance_loc: CurPc += Val * Ca; break;
		case DW_CFA_set_loc: p = pGetTAddrFunc(&pFde->Cie, &Val, p); CurPc = Val; break;
		case DW_CFA_advance_loc1: CurPc += *(unsigned char  *)p * Ca; p += 1; break;
		case DW_CFA_advance_loc2: CurPc += *(unsigned short *)p * Ca; p += 2; break;
		case DW_CFA_advance_loc4: CurPc += *(unsigned	   *)p * Ca; p += 4; break;
		/* SP��� */
		case DW_CFA_def_cfa:	p = Getleb128(&Idx, p); p = Getleb128(&Val,   p); TSP_REG(Idx, Val);	   break;
		case DW_CFA_def_cfa_sf: p = Getleb128(&Idx, p); p = Getsleb128(&sVal, p); TSP_REG(Idx, sVal * Da); break;
		case DW_CFA_def_cfa_register:  p = Getleb128(&Idx, p); TREG_REG(REG_SP, Idx); break;
		case DW_CFA_def_cfa_offset:	p = Getleb128(&Val, p);   pRFunc->SpOff = Val;	   break;
		case DW_CFA_def_cfa_offset_sf: p = Getsleb128(&sVal, p); pRFunc->SpOff = sVal * Da; break;
		case DW_CFA_def_cfa_expression: TREG_EXPR(REG_SP, p); p = Getleb128(&Val, p); p += Val; break;
		/* ������� */
		case DW_CFA_offset: p = Getleb128(&Idx, p); TREG_SPOFF(Val, Idx * Da); break;
		case DW_CFA_offset_extended:	p = Getleb128(&Idx, p); p = Getleb128(&Val, p);   TREG_SPOFF(Idx, Val * Da); break;
		case DW_CFA_offset_extended_sf: p = Getleb128(&Idx, p); p = Getsleb128(&sVal, p); TREG_SPOFF(Idx, sVal * Da); break;
		case DW_CFA_GNU_negative_offset_extended: p = Getleb128(&Idx, p); p = Getleb128(&Val, p); TREG_SPOFF(Idx, -(TSSIZE)(Val * Da)); break;
		case DW_CFA_restore: if (Val < REG_NUM) pTReg[Val] = pOrgRFunc->RegAry[Val]; break;
		case DW_CFA_restore_extended: p = Getleb128(&Idx, p); if (Idx < REG_NUM) pTReg[Idx] = pOrgRFunc->RegAry[Idx]; break;
		case DW_CFA_undefined:  p = Getleb128(&Idx, p); TREG_UNDEF(Idx); break;
		case DW_CFA_same_value: p = Getleb128(&Idx, p); TREG_SAME(Idx); break;
		case DW_CFA_register:   p = Getleb128(&Idx, p); p = Getleb128(&Val, p); TREG_REG(Idx, Val); break;
		case DW_CFA_expression: p = Getleb128(&Idx, p); TREG_EXPR(Idx, p); p = Getleb128(&Val, p); p += Val; break;
		case DW_CFA_GNU_args_size: p = Getleb128(&Val, p); break;
		case DW_CFA_nop: break;
		case DW_CFA_remember_state:
			ptFStk = Malloc(sizeof(*ptFStk));
			if (!ptFStk)
				goto _ERR2;
			memcpy(&ptFStk->RFunc, pRFunc, sizeof(*pRFunc));
			ptFStk->pNext = pFStk;
			pFStk = ptFStk;
			break;
		case DW_CFA_restore_state:
			if (!pFStk)
				goto _ERR3;
			ptFStk = pFStk;
			pFStk = ptFStk->pNext;
			memcpy(pRFunc, &ptFStk->RFunc, sizeof(*pRFunc));
			Free(ptFStk);
			break;
		default:
			SSTWARN(RS_UNKNOWN_DWOP, pFde->Cie.pName, pFde->Cie.pSeg, Op);
			goto _OUT;
		}
	}
	rt = 0;
_OUT:
	while ((ptFStk = pFStk) != NULL) {
		pFStk = ptFStk->pNext;
		Free(ptFStk);
	}
	return rt;
_ERR1:
	SSTWARN(RS_INVALID_IDX, pFde->Cie.pName, pFde->Cie.pSeg, GetCStr(RS_REGISTER), Idx);
	goto _OUT;
_ERR2:
	SSTWARN(RS_OOM);
	goto _OUT;
_ERR3:
	SSTWARN(RS_DWFOP_UNDERFLOW, pFde->Cie.pName, pFde->Cie.pSeg);
	goto _OUT;
}

/*************************************************************************
* DESCRIPTION
*	���мĴ�������
*
* PARAMETERS
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ, �ᱻ����!!!
*	pRFunc: �Ĵ���������
*	pFde: FDE�ṹ��
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int RunRegFunc(TSIZE *pRegAry, TSIZE *pRegMask, DWF_FUNC *pRFunc, const FDE_INF *pFde)
{
	TSIZE RegAry[REG_NUM], RegMask = *pRegMask, Size;
	VMA_HDL *pSpVmaHdl;
	const TSIZE *pVal;
	const void *pInst;
	DWF_TREG *pTReg;
	unsigned i;
	TADDR Sp;
	int rt;

	memcpy(RegAry, pRegAry, sizeof(RegAry));
	pTReg = &pRFunc->RegAry[REG_SP];
	switch (pTReg->Rule) { /* �����ȼ���sp, �����Ĵ�����Ҫ�ο�sp */
	case DWF_SAME: break;
	case DWF_REG:
		if (!(RegMask&((TSIZE)1 << pTReg->u.Idx)))
			return 1;
		RegAry[REG_SP] = RegAry[pTReg->u.Idx] + pRFunc->SpOff;
		break;
	case DWF_EXPR:
		pInst = Getleb128(&Size, pTReg->u.pInst);
		if (DwEvalExpr(pInst, Size, &RegAry[REG_SP], RegAry, RegMask, pFde->Cie.pSeg, pFde->Cie.pName))
			return 1;
		break;
	default: SSTWARN(RS_INVALID_DWFSP_RULE, pFde->Cie.pName, pFde->Cie.pSeg); return 1;
	}
	pTReg->Rule = DWF_SAME; /* �����ظ����� */
	Sp = RegAry[REG_SP];
	pSpVmaHdl = GetVmaHdl(Sp);
	if (!pSpVmaHdl)
		return 1;
	for (rt = 1, i = 0; i < ARY_CNT(pRFunc->RegAry); i++) {
		pTReg = &pRFunc->RegAry[i];
		switch (pTReg->Rule) {
		case DWF_UNDEF: RegMask &= ~((TSIZE)1 << i); break;
		case DWF_SPOFF:
			pVal = Vma2Mem(pSpVmaHdl, Sp + pTReg->u.Off, sizeof(*pVal));
			if (!pVal)
				goto _OUT;
			RegAry[i] = pVal[0];
			RegMask |= ((TSIZE)1 << i);
			break;
		case DWF_REG:
			if (!(RegMask&((TSIZE)1 << pTReg->u.Idx)))
				goto _OUT;
			RegAry[i] = RegAry[pTReg->u.Idx];
			RegMask |= ((TSIZE)1 < i);
			break;
		case DWF_EXPR:
			pInst = Getleb128(&Size, pTReg->u.pInst);
			if (DwEvalExpr(pInst, Size, &RegAry[i], RegAry, RegMask, pFde->Cie.pSeg, pFde->Cie.pName))
				goto _OUT;
			RegMask |= ((TSIZE)1 < i);
			break;
		default:;
		}
	}
	if (!(RegMask&((TSIZE)1 << pFde->Cie.RtIdx)))
		goto _OUT;
	*pRegMask = (RegMask^((TSIZE)1 << pFde->Cie.RtIdx))|((TSIZE)1 << REG_PC);
	RegAry[REG_PC] = RegAry[pFde->Cie.RtIdx];
	memcpy(pRegAry, RegAry, sizeof(RegAry));
	rt = 0;
_OUT:
	PutVmaHdl(pSpVmaHdl);
	return rt;
}


/*************************************************************************
*=========================== internal section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡCIE��Ϣ
*
* PARAMETERS
*	pCie: ����CIE�ṹ��
*	p: CIE buffer
*	pEhAugFunc: .eh_frame��չ�ַ��������ص�����
*		pCie: ����CIE�ṹ��
*		pAug: ��չ�ַ���
*		p: CIE buffer
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ExtractCie(CIE_INF *pCie, const void *p, int (*pEhAugFunc)(CIE_INF *pCie, const char *pAug, const void *p))
{
#define DW_CIE_VER  1 /* dwarf2 */
#define DW_CIE_VER3 3 /* dwarf3 */
#define DW_CIE_VER4 4 /* dwarf4 */
	unsigned char Ver;
	const char *pAug;
	TSIZE AugSz, RtIdx;
	TSIZE Len;
	
	Len = *(unsigned *)p;
#ifdef SST64
	if (Len != 0xFFFFFFFF) {
#endif
		pCie->pEInst = (unsigned char *)p + Len + 4;
		p = (int *)p + 2;
#ifdef SST64
	} else {
		Len = *(TSIZE *)((int *)p + 1); /* ��չ���� */
		pCie->pEInst = (unsigned char *)p + Len + 12;
		p = (int *)p + 5;
	}
#endif
	Ver = *(unsigned char *)p;
	if (Ver != DW_CIE_VER4 && Ver != DW_CIE_VER3 && Ver != DW_CIE_VER)
		goto _ERR1;
	pAug = (char *)p + 1;
	p = Getleb128(&pCie->CodeAlign, (char *)p + strlen(pAug) + (Ver == DW_CIE_VER4 ? 4 : 2)/* dwarf4 bypass addr size/segment size */);
	p = Getsleb128(&pCie->DataAlign, p);
	if (Ver == DW_CIE_VER) {
		pCie->RtIdx = *(unsigned char *)p;
		p = (char *)p + 1;
	} else {
		p = Getleb128(&RtIdx, p);
		if (RtIdx >= REG_NUM)
			goto _ERR3;
		pCie->RtIdx = (TUINT)RtIdx;
	}
	pCie->pSInst = p;
	if (*pAug == 'z' && pEhAugFunc) { /* .eh_frame */
		p = Getleb128(&AugSz, p);
		pCie->pSInst = (unsigned char *)p + AugSz;
		pCie->HasAug = 1;
		if (pEhAugFunc(pCie, pAug + 1, p))
			return 1;
	} else if (*pAug) {
		goto _ERR2;
	}
	return 0;
_ERR3:
	SSTWARN(RS_INVALID_IDX, pCie->pName, pCie->pSeg, GetCStr(RS_REGISTER), RtIdx);
	return 1;
_ERR2:
	SSTWARN(RS_GNU_AUG, pCie->pName, pCie->pSeg, *pAug);
	return 1;
_ERR1:
	SSTWARN(RS_SEG_VERSION, pCie->pName, pCie->pSeg, Ver);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	����FDE��ԭ1��ջ֡
*
* PARAMETERS
*	pFde: FDE�ṹ��
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_SP/REG_PC������Ч!!!), �ᱻ����!!!
*	Pc: ��������ַ��THUMB bit��PC
*	pGetTAddrFunc: ��ȡĿ���ַ�ص�����, ���ض�ȡ���buffer
*		pCie: CIE�ṹ��
*		pAddr: ����Ŀ���ַ
*		p: buffer
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int FdeUnwind(const FDE_INF *pFde, TSIZE *pRegAry, TSIZE *pRegMask, TSIZE Pc, const void *(*pGetTAddrFunc)(const CIE_INF *pCie, TADDR *pAddr, const void *p))
{
#ifdef SST64
#define REG_SAVE_START  18
#else
#define REG_SAVE_START  4
#endif
	DWF_FUNC OrgRFunc, RFunc;
	unsigned i;

	for (i = 0; i < ARY_CNT(RFunc.RegAry); i++) { /* ��ʼ������ */
		RFunc.RegAry[i].Rule = DWF_UNDEF;
	}
	for (i = REG_SAVE_START; i <= REG_FP; i++) {
		RFunc.RegAry[i].Rule = DWF_SAME;
	}
	RFunc.RegAry[pFde->Cie.RtIdx].Rule = DWF_SAME; /* �����1֡ΪҶ�Ӻ���, ��LR��Ч */
	if (ParseCfi(pFde, &RFunc, NULL, 0, pGetTAddrFunc))
		return 1;
	memcpy(&OrgRFunc, &RFunc, sizeof(RFunc));
	if (ParseCfi(pFde, &RFunc, &OrgRFunc, Pc, pGetTAddrFunc))
		return 1;
	return RunRegFunc(pRegAry, pRegMask, &RFunc, pFde);
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ԭ1��ջ֡
*
* PARAMETERS
*	pDwfHdl: dwf���
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_SP/REG_PC������Ч!!!), �ᱻ����!!!
*	Pc: ��������ַ��THUMB bit��PC
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DwfUnwind(DWF_HDL *pDwfHdl, TSIZE *pRegAry, TSIZE *pRegMask, TSIZE Pc)
{
	DWF_INT * const pDwf = (DWF_INT *)pDwfHdl;
	FDE_INF Fde;

	return DwSearchFde(pDwf, &Fde, Pc) ? 1 : FdeUnwind(&Fde, pRegAry, pRegMask, Pc, DwGetTAddr);
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡdwf���
*
* PARAMETERS
*	pFpHdl: fp���
*	Off/Size: .debug_frameƫ�Ƶ�ַ/��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: dwf���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWF_HDL *GetDwfHdl(const void *pFpHdl, TSIZE Off, TSIZE Size)
{
	DWF_INT *pDwf;

	pDwf = Calloc(sizeof(*pDwf));
	if (!pDwf)
		goto _ERR1;
	pDwf->pFpHdl = DupFpHdl(pFpHdl);
	pDwf->Size   = Size;
	pDwf->pBase  = FpMapRo(pFpHdl, Off, Size);
	if (!pDwf->pBase)
		goto _ERR2;
	return (DWF_HDL *)pDwf;
_ERR2:
	PutFpHdl(pFpHdl);
	Free(pDwf);
	SSTWARN(RS_FILE_MAP, ((FP_HDL *)pFpHdl)->pPath, ".debug_frame");
	return NULL;
_ERR1:
	SSTWARN(RS_OOM);
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�dwf���
*
* PARAMETERS
*	pDwfHdl: dwf���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwfHdl(DWF_HDL *pDwfHdl)
{
	DWF_INT * const pDwf = (DWF_INT *)pDwfHdl;

	FpUnmap(pDwf->pFpHdl, pDwf->pBase, pDwf->Size);
	PutFpHdl(pDwf->pFpHdl);
	Free(pDwf);
}
