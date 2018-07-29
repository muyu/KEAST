/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	memspace.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/9/17
* Description: �ڴ������
**********************************************************************************/
#include <string.h>
#include "string_res.h"
#include <module.h>
#include "os.h"
#include <memspace.h>


#ifdef SST64
#define ADDR_OVERFLOW(Addr)	(((TSSIZE)(Addr) >> ADDR_BITS) != (TSSIZE)-1)
#else
#define ADDR_OVERFLOW(Addr)	0
#endif
#define VMA_SZ(pVma)		((pVma)->Ex.EAddr - (pVma)->Ex.SAddr)
#define VMA_CANDROP(pVma)	(!((pVma)->Ref || ((pVma)->pBuf && ((pVma)->Ex.Flag&VMA_MHOLD)))) /* �Ƿ����dropӳ�� */

typedef struct _VMA_INT
{
/* Ex.Flag�ڲ��꣬���ܳ���VMA_RBITS!!! ��VMA_MEXT��־��һ����VMA_MHOLD��־, û��VMA_MHOLD��־��һ��û��VMA_MEXT��־ */
#define VMA_MHOLD	(1 << 0) /* �޸ñ�־: ����drop, ֮��������½���ӳ��, �иñ�־: �ڲ�����drop */
#define VMA_MEXT	(1 << 1) /* �޸ñ�־: �ڲ�ӳ��(map/0�ڴ�), ��Ҫ�ͷ�, �иñ�־: �ⲿӳ��/��ӳ��, �����ͷ� */
#define VMA_MLOCK	(1 << 2) /* ��ס, ������merge */

	struct _VMA_HDL Ex; /* export info */
	struct _VMA_INT *pNext;
	const char *pBuf; /* ӳ���buffer */
	TSIZE FOff; /* �ļ�ƫ����, 0��ʾ0�ڴ�/�ⲿӳ��/��ӳ�� */
	unsigned Ref; /* ������ü��� */
} VMA_INT;

#define FLV_BITS			(7 + ADDR_BITS - 32) /* 1�����λ�� */
#define FLV_SHIFT			(ADDR_BITS - FLV_BITS)
#define FLV_SIZE			(1 << FLV_BITS)
#ifdef SST64
#define FLV_IDX(Addr)		(((Addr) >> FLV_SHIFT)&(FLV_SIZE - 1))
#else
#define FLV_IDX(Addr)		((Addr) >> FLV_SHIFT)
#endif
#define SLV_BITS			(ADDR_BITS - FLV_BITS - PAGE_BITS) /* 2�����λ�� */
#define SLV_SHIFT			PAGE_BITS
#define SLV_SIZE			(1 << SLV_BITS)
#define SLV_IDX(Addr)		(((Addr) >> SLV_SHIFT)&(SLV_SIZE - 1))

#define SLV_FLAG			(1 << 0)
#define IS_SLV(pSlvV)		((uintptr_t)(pSlvV)&SLV_FLAG)
#define SET_SLVF(ppSlvM)	((VMA_INT *)((uintptr_t)(ppSlvM)|SLV_FLAG))
#define GET_SLV(pSlvV)		((VMA_INT **)((uintptr_t)(pSlvV)^SLV_FLAG))

static struct
{
	const VMA_INT *pFlvM[FLV_SIZE];
	VMA_INT *pList/* list����С�� */, *pCache; /* vma���� */
	FP_HDL *pFpHdl; /* coredump��� */
} MmSpace;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�������1��SAddr <= Addr��vma���
*
* PARAMETERS
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: vma���
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static VMA_INT *FindVma(TADDR Addr)
{
	VMA_INT *pVma = NULL, *pEnd = NULL, *pPos;

	if (MmSpace.pCache) {
		if (MmSpace.pCache->Ex.SAddr <= Addr) {
			for (pPos = MmSpace.pCache; pPos; pPos = (pVma = pPos)->pNext) {
				if (pPos->Ex.SAddr > Addr)
					break;
			}
			return (MmSpace.pCache = pVma);
		}
		pEnd = MmSpace.pCache;
	}
	for (pPos = MmSpace.pList; pPos != pEnd; pPos = (pVma = pPos)->pNext) {
		if (pPos->Ex.SAddr > Addr)
			break;
	}
	if (pVma)
		MmSpace.pCache = pVma;
	return pVma;
}

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ����vma���
*
* PARAMETERS
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: mem���
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static VMA_INT *Addr2Vma(TADDR Addr)
{
	const TSIZE f = FLV_IDX(Addr);
	const VMA_INT * const pSlvV = MmSpace.pFlvM[f];
	
	return (IS_SLV(pSlvV) ? GET_SLV(pSlvV)[SLV_IDX(Addr)] : (VMA_INT *)pSlvV);
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�vmaӳ���buffer
*
* PARAMETERS
*	pVma: vma���
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FreeVmaMap(VMA_INT *pVma)
{
	if (pVma->pBuf && !(pVma->Ex.Flag&VMA_MEXT)) {
		if (pVma->FOff)
			FpUnmap(MmSpace.pFpHdl, pVma->pBuf, VMA_SZ(pVma));
		else
			PutZeroBuf(pVma->pBuf, VMA_SZ(pVma));
		pVma->pBuf = NULL;
	}
}

/*************************************************************************
* DESCRIPTION
*	�����ͷ���ӳ��������ڴ�
*
* PARAMETERS
*	Size: Ԥ���ͷŵĴ�С(�ֽ�)
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int TryDropMap(TSIZE Size)
{
	VMA_INT *pVma;

	for (pVma = MmSpace.pList; pVma; pVma = pVma->pNext) {
		if (pVma->pBuf && VMA_CANDROP(pVma)) {
			FreeVmaMap(pVma);
			if (VMA_SZ(pVma) >= Size)
				break;
		}
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�vma���
*
* PARAMETERS
*	pVma: vma���
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FreeVma(VMA_INT *pVma)
{
#ifdef _DEBUG
	const VMA_INT *p;
	TADDR Addr;
	
	if (pVma->Ref)
		DBGERR("δ�ͷ�%d: "TADDRFMT"-"TADDRFMT" %s\n", pVma->Ref, pVma->Ex.SAddr, pVma->Ex.EAddr, pVma->Ex.pName);
	for (Addr = pVma->Ex.SAddr; Addr < pVma->Ex.EAddr; Addr += PAGE_SIZE) {
		p = Addr2Vma(Addr);
		if (p != pVma)
			DBGERR("ӳ��ʧ��: "TADDRFMT", %s, %p\n", Addr, pVma->Ex.pName, p);
	}
#endif
	if (pVma == MmSpace.pCache)
		MmSpace.pCache = NULL;
	FreeVmaMap(pVma);
	if (pVma->Ex.Flag&VMA_NAME)
		Free((void *)pVma->Ex.pName);
	Free(pVma);
}

/*************************************************************************
* DESCRIPTION
*	������ַ��Χ��vma�����ӳ��
*	ע��: ���Ḳ�Ǿɵ�ӳ��!!!
*
* PARAMETERS
*	pVma: ��ӳ���vma���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SetVmaMapping(const VMA_INT *pVma)
{
#if SLV_BITS < 2
#error "SLV_SIZE should be power of 4"
#endif
#define GET_SLV_PTR(ppSlvM, Idx) \
	do { \
		pSlvV = MmSpace.pFlvM[Idx]; \
		if (IS_SLV(pSlvV)) { \
			ppSlvM = (const VMA_INT **)GET_SLV(pSlvV); \
		} else { \
			if ((ppSlvM = Calloc(SLV_SIZE * sizeof(*ppSlvM))) == NULL) { \
				SSTWARN(RS_OOM); \
				return 1; \
			} \
			MmSpace.pFlvM[Idx] = SET_SLVF(ppSlvM); \
			if (pSlvV) { \
				i = 0; /* SLV_SIZE����4��n�η� */ \
				do { ppSlvM[i] = pSlvV; ppSlvM[i + 1] = pSlvV; ppSlvM[i + 2] = pSlvV; ppSlvM[i + 3] = pSlvV; i += 4; } while (i < SLV_SIZE); \
			} \
		} \
	} while (0)
	const VMA_INT **ppSlvM, *pSlvV;
	size_t Sf, Ef, Ss, Es, i;

	Ef = (size_t)FLV_IDX(pVma->Ex.EAddr); /* CreateMemRegion()�ᱣ֤��ַ�������!!! */
	Es = SLV_IDX(pVma->Ex.EAddr);
	Sf = (size_t)FLV_IDX(pVma->Ex.SAddr); /* CreateMemRegion()�ᱣ֤��ַ�������!!! */
	Ss = SLV_IDX(pVma->Ex.SAddr);
	if (Ss) { /* ��ʼ��ַ����1��ҳ����� */
		GET_SLV_PTR(ppSlvM, Sf);
		if (Sf == Ef) { /* ����2��ҳ���� */
			for (; Ss < Es; Ss++) { /* ���SLV */
				ppSlvM[Ss] = pVma;
			}
			return 0;
		}
		for (; Ss < SLV_SIZE; Ss++) { /* ���SLV */
			ppSlvM[Ss] = pVma;
		}
		Sf++;
	}
	for (; Sf < Ef; Sf++) { /* ���FLV */
		pSlvV = MmSpace.pFlvM[Sf];
		MmSpace.pFlvM[Sf] = pVma;
		if (IS_SLV(pSlvV))
			Free(GET_SLV(pSlvV));
	}
	if (Es) { /* ������ַ����1��ҳ����� */
		GET_SLV_PTR(ppSlvM, Ef);
		for (i = 0; i < Es; i++) { /* ���SLV */
			ppSlvM[i] = pVma;
		}
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	����Ƿ���Ժϲ�vma
*
* PARAMETERS
*	SAddr/EAddr: [��ʼ��ַ, ������ַ)
*
* RETURNS
*	0: ���ܺϲ�
*	1: ���Ժϲ�
*
* LOCAL AFFECTED
*
*************************************************************************/
static int CanMergeVma(TADDR SAddr, TADDR EAddr)
{
	VMA_INT *pVma;
	
	pVma = Addr2Vma(SAddr);
	if (pVma->Ex.SAddr != SAddr && pVma->Ref)
		return 0;
	for (; pVma->Ex.EAddr < EAddr; pVma = pVma->pNext) {
		if (pVma->pNext->Ex.Flag&VMA_MLOCK)
			return 0;
	}
	return (pVma->Ex.EAddr == EAddr || !pVma->Ref);
}

/*************************************************************************
* DESCRIPTION
*	����vma, ����vma���ܱ�����!!!
*
* PARAMETERS
*	Addr: ���ѵ�ַ
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SplitVma(TADDR Addr)
{
	VMA_INT *pSVma, *pVma;
	TSIZE Size1, Size2;
	void *pBuf;
	
	pSVma = Addr2Vma(Addr);
	if (!pSVma || pSVma->Ex.SAddr == Addr)
		return 0;
	if (!(pSVma->Ex.Flag&VMA_MHOLD))
		FreeVmaMap(pSVma);
	pVma = Malloc(sizeof(*pVma));
	if (!pVma)
		goto _OOM1;
	*pVma = *pSVma;
	pSVma->pNext = pVma;
	pVma->Ex.SAddr = pSVma->Ex.EAddr = Addr;
	if ((pVma->Ex.Flag&VMA_NAME) && (pVma->Ex.pName = strdup(pVma->Ex.pName)) == NULL)
		pVma->Ex.Flag &= ~VMA_NAME;
	Size1 = Addr - pSVma->Ex.SAddr;
	if (pVma->FOff) {
		pVma->FOff += Size1;
	} else if (pVma->Ex.Flag&VMA_MEXT) {
		if (pVma->pBuf)
			pVma->pBuf += Size1;
	} else if (pVma->Ex.Flag&VMA_MHOLD) {
		Size2 = pVma->Ex.EAddr - Addr;
		pVma->pBuf = GetZeroBuf(Size2);
		pBuf = GetZeroBuf(Size1);
		if (!pVma->pBuf || !pBuf) // TODO: ʧ�ܺ������!!!
			goto _OOM2;
		memcpy((void *)pVma->pBuf, pSVma->pBuf + Size1, (size_t)Size2);
		memcpy(pBuf, pSVma->pBuf, (size_t)Size1);
		PutZeroBuf(pSVma->pBuf, Size1 + Size2);
		pSVma->pBuf = pBuf;
	}
	return SetVmaMapping(pVma); // TODO: ʧ�ܺ������!!!
_OOM2:
	if (pBuf)
		PutZeroBuf(pBuf, Size1);
_OOM1:
	SSTWARN(RS_OOM);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	�ϲ�vma, �ڴ���������Ҳ��ܱ�����!!!
*
* PARAMETERS
*	pSVma/pEVma: [��ʼvma���, ����vma���]
*	NeedBuf: �Ƿ���Ҫ�ϲ�buffer
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MergeVma(VMA_INT *pSVma, VMA_INT *pEVma, int NeedBuf)
{
	const TSIZE BufSz = pEVma->Ex.EAddr - pSVma->Ex.SAddr;
	VMA_INT * const pEnd = pEVma->pNext;
	char *pBuf = NULL;
	VMA_INT *pVma;
	TSIZE Size;
	TADDR SAddr;

	if (NeedBuf) {
		pBuf = GetZeroBuf(BufSz);
		if (!pBuf)
			goto _OOM;
		for (SAddr = pSVma->Ex.SAddr, pVma = pSVma; pVma != pEnd; pVma = pVma->pNext) {
			Size = VMA_SZ(pVma);
			if (!pVma->pBuf) {
				if (!pVma->FOff) {
					if (pVma->Ex.Flag&VMA_MEXT)
						DBGERR("û��ӳ���vma: %s "TADDRFMT"\n", pVma->Ex.pName, pVma->Ex.SAddr);
					continue;
				}
				pVma->pBuf = FpMapRo(MmSpace.pFpHdl, pVma->FOff, Size);
				if (!pVma->pBuf)
					goto _OOM2;
			}
			memcpy(pBuf + (pVma->Ex.SAddr - SAddr), pVma->pBuf, (size_t)Size);
		}
		ProtectZeroBuf(pBuf, BufSz);
	}
	FreeVmaMap(pSVma);
	if (pBuf) {
		pSVma->FOff = 0;
		pSVma->pBuf = pBuf;
		pSVma->Ex.Flag = (pSVma->Ex.Flag&~VMA_MEXT)|VMA_MHOLD;
	}
	pSVma->Ex.EAddr = pEVma->Ex.EAddr;
	pVma = pSVma->pNext;
	pSVma->pNext = pEnd; /* �������� */
	pEVma->pNext = NULL;
	while (pVma) { /* �ͷ�������� */
		pEVma = pVma;
		pVma = pVma->pNext;
		FreeVma(pEVma);
	}
	return SetVmaMapping(pSVma); // TODO: ���ʧ��, �������!!!
_OOM2:
	PutZeroBuf(pBuf, BufSz);
_OOM:
	SSTWARN(RS_OOM);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	���µ�vma���ӵ��ڴ����
*
* PARAMETERS
*	Addr/EAddr: [��ʼ��ַ, ������ַ)
*	Attr: ����
*	FOff: �ļ�ƫ������Ϊ0ʱ, pBuf���벻ΪNULL!
*	pBuf: buffer, ΪNULLʱ, FOff���벻Ϊ0!
*	pHead: �ص�vma���
*	pCmpFunc: �ص��ԱȺ���
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	2: �ص�
*
* GLOBAL AFFECTED
*
*************************************************************************/
static int VmaOverlay(TADDR SAddr, TADDR EAddr, unsigned Attr, TSIZE FOff, const void *pBuf, VMA_INT *pHead
	, int (*pCmpFunc)(TADDR Addr, TSIZE Size, const void *pBuf1, const void *pBuf2, void *pArg), void *pArg)
{
	VMA_INT New, *pList, *pTail = pHead;
	TADDR Addr;
	TSIZE Size;
	int rt = 1;

	memset(&New, 0, sizeof(New));
	New.Ex.SAddr = SAddr;
	New.Ex.EAddr = EAddr;
	New.Ex.Flag = Attr;
	New.FOff = FOff;
	New.pBuf = pBuf;
	/* �Ƚ� */
	Addr = (pTail->Ex.SAddr < SAddr ? SAddr : pTail->Ex.SAddr);
	while (1) {
		if (pTail->Ref || (pTail->Ex.Flag&VMA_MLOCK)) {
			DBGERR("��������/��ס: "TADDRFMT"-"TADDRFMT"\n", pTail->Ex.SAddr, pTail->Ex.EAddr);
			goto _FAIL;
		}
		Size = (EAddr > pTail->Ex.EAddr ? pTail->Ex.EAddr : EAddr) - Addr;
		if (pCmpFunc(Addr, Size, Vma2Mem(MOD_HDL(&New), Addr, Size), Vma2Mem(MOD_HDL(pTail), Addr, Size), pArg))
			goto _FAIL;
		if (!pTail->pNext || pTail->pNext->Ex.SAddr >= EAddr)
			break;
		pTail = pTail->pNext;
		Addr = pTail->Ex.SAddr;
	}
	/* ������ϵ, ������һ�� */
	if (pHead->Ex.SAddr <= SAddr && pHead->Ex.EAddr >= EAddr && VMA_ATTR(MOD_HDL(pHead)) == (Attr&(VMA_RW|VMA_X))) {
		rt = 2;
		goto _FAIL;
	}
	/* ���� */
	if (pHead->Ex.SAddr < SAddr) {
		Addr = pTail->Ex.EAddr - 1;
		if (SplitVma(SAddr))
			goto _FAIL;
		pHead = Addr2Vma(SAddr);
		pTail = Addr2Vma(Addr); /* ��Ҫ����pTail */
	}
	if (pTail->Ex.EAddr > EAddr) {
		if (SplitVma(EAddr))
			goto _FAIL;
		pTail = Addr2Vma(EAddr - 1);
	}
	/* ��ȹ�ϵ */
	if (pHead->Ex.SAddr == SAddr && pHead->Ex.EAddr == EAddr) {
		pHead->Ex.Flag = (pHead->Ex.Flag&~(VMA_RW|VMA_X))|(New.Ex.Flag&(VMA_RW|VMA_X));
		rt = 2;
		goto _FAIL;
	}
	/* �滻 */
	pList = pHead->pNext;
	FreeVmaMap(pHead);
	if (pHead->Ex.Flag&VMA_NAME)
		Free((void *)pHead->Ex.pName);
	New.pNext = pTail = pTail->pNext;
	*pHead = New;
	/* �ͷ� */
	while (pList != pTail) {
		New.pNext = pList;
		pList = pList->pNext;
		FreeVma(New.pNext);
	}
	return SetVmaMapping(pHead); // TODO: ���ʧ��, �������!!!
_FAIL:
	FreeVmaMap(&New);
	return rt;
}


/*************************************************************************
*============================ Memory section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡbuffer
*	ע��: pVmaHdl���ͷź�buffer��������!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	Addr/Size: ��ַ/��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *Vma2Mem(VMA_HDL *pVmaHdl, TADDR Addr, TSIZE Size)
{
	VMA_INT * const pVma = MOD_INT(VMA, pVmaHdl);
	const TADDR End = Addr + Size;
	const void *pBuf = NULL;

	if (Addr >= pVma->Ex.SAddr && End <= pVma->Ex.EAddr && Addr < End) {
		if (!pVma->pBuf) {
			if (pVma->Ex.Flag&VMA_MEXT) /* �ⲿӳ��/��ӳ�� */
				goto _OUT;
			pVma->pBuf = FpMapRo(MmSpace.pFpHdl, pVma->FOff, VMA_SZ(pVma));
			if (!pVma->pBuf) {
				SSTWARN(RS_OOM);
				goto _OUT;
			}
		}
		pBuf = pVma->pBuf + (Addr - pVma->Ex.SAddr);
	} else if (Addr) {
		DBGERR("vma2memʧ��: "TADDRFMT"-"TADDRFMT" %s <- "TADDRFMT"(%d)\n", pVmaHdl->SAddr, pVmaHdl->EAddr, pVmaHdl->pName, Addr, Size);
	}
_OUT:
	return pBuf;
}

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡ���ݲ������buffer
*
* PARAMETERS
*	Addr: ��ַ
*	pBuf/Size: ������buffer/��С(�ֽ�)
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DumpMem(TADDR Addr, void *pBuf, TSIZE Size)
{
	const void *p = NULL;
	VMA_HDL *pVmaHdl;
	TSIZE MaxSz;

	do {
		pVmaHdl = GetVmaHdl(Addr);
		if (!pVmaHdl)
			return 1;
		p = Vma2Mem(pVmaHdl, Addr, 1);
		if (p) {
			MaxSz = pVmaHdl->EAddr - Addr;
			if (MaxSz > Size)
				MaxSz = Size;
			memcpy(pBuf, p, (size_t)MaxSz);
			pBuf = (char *)pBuf + MaxSz;
			Addr += MaxSz;
			Size -= MaxSz;
		}
		PutVmaHdl(pVmaHdl);
	} while (p && Size);
	return !p;
}

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡ�ַ���
*
* PARAMETERS
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: string, ����ʱ��Ҫ�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *GetString(TADDR Addr)
{
	char *pStr = NULL;
	const char *pBuf;
	VMA_HDL *pVmaHdl;
	size_t Len;

	pVmaHdl = GetVmaHdl(Addr);
	if (!pVmaHdl)
		return pStr;
	pBuf = Vma2Mem(pVmaHdl, Addr, 1);
	if (pBuf) {
		Len = strnlen(pBuf, (size_t)(pVmaHdl->EAddr - Addr));
		pStr = Malloc(Len + 1);
		if (pStr) {
			memcpy(pStr, pBuf, Len);
			pStr[Len] = '\0';
		}
	}
	PutVmaHdl(pVmaHdl);
	return pStr;
}

/*************************************************************************
* DESCRIPTION
*	����vma�������, ���Ǿ�����!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	pName: ����
*	Flag: ֻ����VMA_NAME/VMA_MMASK/VMA_TMASK!!!
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void VmaSetAttr(VMA_HDL *pVmaHdl, const char *pName, unsigned Flag)
{
	VMA_INT * const pVma = MOD_INT(VMA, pVmaHdl);

	if (Flag&~(VMA_NAME|VMA_MMASK|VMA_TMASK))
		DBGERR("flag�쳣: %x\n", Flag);
	//if (VMA_TYPE(pVmaHdl) != VMA_NONE && VMA_TYPE(pVmaHdl) != VMA_FILE && VMA_TYPE(pVmaHdl) != (Flag&VMA_TMASK))
	//	DBGERR("flag�쳣: %s, %x, %x\n", pVmaHdl->pName, VMA_TYPE(pVmaHdl) >> VMA_TSHIFT, (Flag&VMA_TMASK) >> VMA_TSHIFT);
	if (Flag&VMA_NAME) {
		pName = strdup(pName);
		if (!pName) {
			SSTWARN(RS_OOM);
			return;
		}
	}
	if (pVma->Ex.Flag&VMA_NAME)
		Free((void *)pVma->Ex.pName);
	pVma->Ex.Flag  = (pVma->Ex.Flag&~(VMA_NAME|VMA_MMASK|VMA_TMASK))|Flag;
	pVma->Ex.pName = pName;
}

/*************************************************************************
* DESCRIPTION
*	��vma�������1������ͻ�ȡbuffer����, ���Ǿɾ���ͻ�ȡbuffer����!!!
*	���غ������ܵ���TryMergeVma()!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	pHdl: ���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void VmaRegHdl(VMA_HDL *pVmaHdl, const void *pHdl)
{
	VMA_INT * const pVma = MOD_INT(VMA, pVmaHdl);
	
	pVma->Ex.pHdl = pHdl;
}

/*************************************************************************
* DESCRIPTION
*	��vma�������1����ַ, ���Ǿɵ�ַ!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	Addr: �ϸ�vma��ʼ��ַ
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void VmaLink(VMA_HDL *pVmaHdl, TADDR Addr)
{
	VMA_INT * const pVma = MOD_INT(VMA, pVmaHdl);
	
	pVma->Ex.LinkAddr = Addr;
	pVma->Ex.Flag |= VMA_MLOCK;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡvma���
*
* PARAMETERS
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: vma���
*
* GLOBAL AFFECTED
*
*************************************************************************/
VMA_HDL *GetVmaHdl(TADDR Addr)
{
	VMA_INT *pVma;

	if (!ADDR_OVERFLOW(Addr) && (pVma = Addr2Vma(Addr)) != NULL) {
		pVma->Ref++;
		return MOD_HDL(pVma);
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�vma���
*
* PARAMETERS
*	pVmaHdl: vma���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutVmaHdl(VMA_HDL *pVmaHdl)
{
	VMA_INT * const pVma = MOD_INT(VMA, pVmaHdl);
	
	pVma->Ref--;
#ifdef _DEBUG
	if (VMA_CANDROP(pVma))
		FreeVmaMap(pVma);
#endif
}

/*************************************************************************
* DESCRIPTION
*	���Ժϲ�ָ����Χ�ڵ�vma(���ͱ���ΪVMA_NONE)�����ûص�����
*
* PARAMETERS
*	SAddr/EAddr: [��ʼ��ַ, ������ַ)
*	pFunc: �ص�����
*		pVmaHdl: vma���, �뿪�ص����������ͷ�!!!
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int TryMergeVma(TADDR SAddr, TADDR EAddr, int (*pFunc)(VMA_HDL *pVmaHdl, void *pArg), void *pArg)
{
#define INVALID_OFF		 1 /* ����PAGE��������ּ��� */
#define VMA_CANMERGE(pVma)  (!((pVma)->Ref || (!(pVma)->pBuf && ((pVma)->Ex.Flag&VMA_MEXT)))) /* ���ϲ���buf���ⲿӳ���vma */
	VMA_INT *pVma, *pTmp;
	unsigned Flag;
	TSIZE Off;
	int rt;

	SAddr &= PAGE_MASK;
	pVma = Addr2Vma(SAddr);
	if (!pVma) {
		pVma = FindVma(SAddr);
		pVma = (pVma ? (pVma->Ex.EAddr <= SAddr ? pVma->pNext : pVma) : MmSpace.pList);
		if (!pVma)
			return 1;
		SAddr = pVma->Ex.SAddr;
	}
	EAddr = (EAddr + PAGE_SIZE - 1)&PAGE_MASK; /* ���� */
	pVma = Addr2Vma(EAddr);
	if (!pVma) {
		pVma = FindVma(EAddr);
		if (!pVma)
			return 1;
		EAddr = pVma->Ex.EAddr;
	}
	if (SAddr >= EAddr || ADDR_OVERFLOW(SAddr) || !CanMergeVma(SAddr, EAddr)) {
		DBGERR("�ϲ�["TADDRFMT"-"TADDRFMT")ʧ��\n", SAddr, EAddr);
		return 1;
	}
	if (SplitVma(SAddr) || SplitVma(EAddr))
		return 1;
	pVma = Addr2Vma(SAddr);
	do {
		if (VMA_CANMERGE(pVma) && pVma->Ex.EAddr != EAddr) { /* ���Ժϲ� */
			Off = (pVma->FOff ? pVma->FOff + VMA_SZ(pVma) : 0);
			Flag = VMA_ATTR(MOD_HDL(pVma));
			pTmp = pVma;
			do {
				if (pTmp->Ex.EAddr != pTmp->pNext->Ex.SAddr || VMA_ATTR(MOD_HDL(pTmp->pNext)) != Flag || !VMA_CANMERGE(pTmp->pNext)) /* �޷������ϲ� */
					break;
				pTmp = pTmp->pNext;
				if (Off == INVALID_OFF) /* ��������ӳ�� */
					continue;
				if (Off)
					Off = (pTmp->FOff == Off ? (Off + VMA_SZ(pTmp)) : INVALID_OFF);
				else if (pTmp->FOff)
					Off = INVALID_OFF;
			} while (pTmp->Ex.EAddr != EAddr);
			if (pVma != pTmp && MergeVma(pVma, pTmp, Off == INVALID_OFF)) /* �ϲ� */
				return 1;
		}
		pVma->Ref++;
		rt = pFunc(MOD_HDL(pVma), pArg);
		pVma->Ref--;
		if (rt)
			return rt;
		pVma = pVma->pNext;
	} while (pVma && pVma->Ex.SAddr < EAddr);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	ָ����Χ�ڵ�ַ��С�������vma
*
* PARAMETERS
*	SAddr/EAddr: ��Χ: [��ʼ��ַ, ������ַ)
*	pFunc: �ص�����
*		SAddr/EAddr: [��ʼ��ַ, ������ַ)
*		Flag: ��־, VMA_*
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachVma(TADDR SAddr, TADDR EAddr, int (*pFunc)(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg), void *pArg)
{
	VMA_INT *pVma = NULL;
	TADDR NAddr;
	int rt;

	if (SAddr) {
		pVma = Addr2Vma(SAddr);
		if (!pVma && (pVma = FindVma(SAddr)) != NULL && pVma->Ex.EAddr <= SAddr)
			pVma = pVma->pNext;
	}
	if (!pVma) {
		pVma = MmSpace.pList;
		if (!pVma)
			return 0;
	}
	if (pVma->Ex.SAddr >= EAddr) /* ��ʼ��ַ >= ������ַ */
		return 0;
	do {
		NAddr = (pVma->pNext ? pVma->pNext->Ex.SAddr : pVma->Ex.EAddr);
		rt = pFunc(pVma->Ex.SAddr, pVma->Ex.EAddr, pVma->Ex.Flag, pArg);
	} while (!rt && (pVma = Addr2Vma(NAddr)) != NULL && pVma->Ex.SAddr < EAddr);
	return rt;
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�����ڴ��׼��
*	ʹ�÷���: PrepareMemSpace() -> CreateMemRegion() -> DestoryMemSpace()
*
* PARAMETERS
*	pFpHdl: fp���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PrepareMemSpace(const void *pFpHdl)
{
	MmSpace.pFpHdl = DupFpHdl(pFpHdl);
	RegTryUnmapFunc(TryDropMap);
}

/*************************************************************************
* DESCRIPTION
 *	����1���ڴ�������coredumpΪ׼�� vmlinux�� cordump������memspace�ϸ���compare
*
* PARAMETERS
 *	Addr/Size: MAP��ʼ��ַ(VA)��MAP��С(�ֽ�), ����PAGE����!!!
 *	Attr: MAP��д����(VMA_R/VMA_W/VMA_X)
 *	FOff: �ļ�ƫ���������elf�ļ�
*	pBuf: buffer, ��ΪNULLʱ, FOffΪ0!!!
 *	pCmpFunc: �ص��ԱȺ���(vmlinux �� coredump �ص�����Ա�)
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	2: �ص�
*
* GLOBAL AFFECTED
*
*************************************************************************/
int CreateMemRegion(TADDR Addr, TSIZE Size, unsigned Attr, TSIZE FOff, const void *pBuf
	, int (*pCmpFunc)(TADDR Addr, TSIZE Size, const void *pBuf1, const void *pBuf2, void *pArg), void *pArg)
{
	const TADDR EAddr = Addr + Size;
	VMA_INT *p, *pVma;

	if (((Addr|EAddr)&~PAGE_MASK) || Addr >= EAddr || ADDR_OVERFLOW(Addr))
		goto _ERR1;
	if (!FOff)
		Attr |= (VMA_MEXT|VMA_MHOLD);
	/* �������� */
	pVma = FindVma(Addr);/* �������1��SAddr <= Addr��vma��� , �����ھͲ����ص��� ������Ҫ����Ƿ��ص� */
	p = (pVma ? (pVma->Ex.EAddr <= Addr ? pVma->pNext : pVma) : MmSpace.pList);/* �ҵ���pVma Eaddr <=Addr ,p=pVma->pNext�ȽϺ���һ��(NULL����SAddr>Addr) */
	if (p && p->Ex.SAddr < EAddr) /* p=pVma->pNext�ȽϺ���һ��(Addr<p->Ex.SAddr<EAddr)�ص� */
		return VmaOverlay(Addr, EAddr, Attr, FOff, pBuf, p, pCmpFunc, pArg);
	p = Calloc(sizeof(*p));
	if (!p)
		goto _ERR2;
	p->Ex.SAddr = Addr;
	p->Ex.EAddr = EAddr;
	p->FOff = FOff;
	p->pBuf = pBuf;
	p->Ex.Flag = Attr;
	if (pVma) { /* p ����pVma��pVma->pNext֮��  */
		p->pNext = pVma->pNext;
		pVma->pNext = p;
	} else {
		p->pNext = MmSpace.pList;
		MmSpace.pList = p; /* MmSpace.pListָ����С�� */
	}
	if (SetVmaMapping(p))
		p = NULL; /* DestoryMemSpace()���ͷ�!!! */
	return !p;
_ERR2:
	SSTWARN(RS_OOM);
	return 1;
_ERR1:
	SSTERR(RS_EADDR, MmSpace.pFpHdl->pPath, "PT_LOAD", Addr);
	return 0; /* �������ִ���! */
}

/*************************************************************************
* DESCRIPTION
*	ע�������ڴ��
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
void DestoryMemSpace(void)
{
	VMA_INT *pVma = MmSpace.pList;
	const VMA_INT *pSlvV;
	size_t i;

	if (pVma) {
		/* �ͷ�vma */
		do {
			MmSpace.pList = MmSpace.pList->pNext;
			if (MmSpace.pList && pVma->Ex.EAddr > MmSpace.pList->Ex.SAddr)
				DBGERR("�ڴ���쳣: "TADDRFMT" > "TADDRFMT"\n", pVma->Ex.EAddr, MmSpace.pList->Ex.SAddr);
			FreeVma(pVma);
		} while ((pVma = MmSpace.pList) != NULL);
		/* �ͷ�ӳ�� */
		for (i = 0; i < FLV_SIZE; i++) {
			pSlvV = MmSpace.pFlvM[i];
			if (IS_SLV(pSlvV))
				Free(GET_SLV(pSlvV));
		}
	}
	if (MmSpace.pFpHdl)
		PutFpHdl(MmSpace.pFpHdl);
	memset(&MmSpace, 0, sizeof(MmSpace));
}
