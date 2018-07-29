/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	memspace.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/9/17
* Description: 内存虚拟层
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
#define VMA_CANDROP(pVma)	(!((pVma)->Ref || ((pVma)->pBuf && ((pVma)->Ex.Flag&VMA_MHOLD)))) /* 是否可以drop映射 */

typedef struct _VMA_INT
{
/* Ex.Flag内部宏，不能超过VMA_RBITS!!! 有VMA_MEXT标志就一定有VMA_MHOLD标志, 没有VMA_MHOLD标志就一定没有VMA_MEXT标志 */
#define VMA_MHOLD	(1 << 0) /* 无该标志: 可以drop, 之后可以重新建立映射, 有该标志: 内部不能drop */
#define VMA_MEXT	(1 << 1) /* 无该标志: 内部映射(map/0内存), 需要释放, 有该标志: 外部映射/无映射, 无需释放 */
#define VMA_MLOCK	(1 << 2) /* 锁住, 不让其merge */

	struct _VMA_HDL Ex; /* export info */
	struct _VMA_INT *pNext;
	const char *pBuf; /* 映射的buffer */
	TSIZE FOff; /* 文件偏移量, 0表示0内存/外部映射/无映射 */
	unsigned Ref; /* 句柄引用计数 */
} VMA_INT;

#define FLV_BITS			(7 + ADDR_BITS - 32) /* 1级解决位数 */
#define FLV_SHIFT			(ADDR_BITS - FLV_BITS)
#define FLV_SIZE			(1 << FLV_BITS)
#ifdef SST64
#define FLV_IDX(Addr)		(((Addr) >> FLV_SHIFT)&(FLV_SIZE - 1))
#else
#define FLV_IDX(Addr)		((Addr) >> FLV_SHIFT)
#endif
#define SLV_BITS			(ADDR_BITS - FLV_BITS - PAGE_BITS) /* 2级解决位数 */
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
	VMA_INT *pList/* list中最小？ */, *pCache; /* vma链表 */
	FP_HDL *pFpHdl; /* coredump句柄 */
} MmSpace;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	查找最后1个SAddr <= Addr的vma句柄
*
* PARAMETERS
*	Addr: 地址
*
* RETURNS
*	NULL: 失败
*	其他: vma句柄
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
*	根据地址查找vma句柄
*
* PARAMETERS
*	Addr: 地址
*
* RETURNS
*	NULL: 失败
*	其他: mem句柄
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
*	释放vma映射的buffer
*
* PARAMETERS
*	pVma: vma句柄
*
* RETURNS
*	无
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
*	尝试释放已映射进来的内存
*
* PARAMETERS
*	Size: 预期释放的大小(字节)
*
* RETURNS
*	0: 成功
*	1: 失败
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
*	释放vma句柄
*
* PARAMETERS
*	pVma: vma句柄
*
* RETURNS
*	无
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
		DBGERR("未释放%d: "TADDRFMT"-"TADDRFMT" %s\n", pVma->Ref, pVma->Ex.SAddr, pVma->Ex.EAddr, pVma->Ex.pName);
	for (Addr = pVma->Ex.SAddr; Addr < pVma->Ex.EAddr; Addr += PAGE_SIZE) {
		p = Addr2Vma(Addr);
		if (p != pVma)
			DBGERR("映射失败: "TADDRFMT", %s, %p\n", Addr, pVma->Ex.pName, p);
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
*	建立地址范围到vma句柄的映射
*	注意: 将会覆盖旧的映射!!!
*
* PARAMETERS
*	pVma: 被映射的vma句柄
*
* RETURNS
*	0: 成功
*	1: 失败
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
				i = 0; /* SLV_SIZE必须4的n次方 */ \
				do { ppSlvM[i] = pSlvV; ppSlvM[i + 1] = pSlvV; ppSlvM[i + 2] = pSlvV; ppSlvM[i + 3] = pSlvV; i += 4; } while (i < SLV_SIZE); \
			} \
		} \
	} while (0)
	const VMA_INT **ppSlvM, *pSlvV;
	size_t Sf, Ef, Ss, Es, i;

	Ef = (size_t)FLV_IDX(pVma->Ex.EAddr); /* CreateMemRegion()会保证地址不会溢出!!! */
	Es = SLV_IDX(pVma->Ex.EAddr);
	Sf = (size_t)FLV_IDX(pVma->Ex.SAddr); /* CreateMemRegion()会保证地址不会溢出!!! */
	Ss = SLV_IDX(pVma->Ex.SAddr);
	if (Ss) { /* 起始地址不是1级页表对齐 */
		GET_SLV_PTR(ppSlvM, Sf);
		if (Sf == Ef) { /* 都在2级页表内 */
			for (; Ss < Es; Ss++) { /* 填充SLV */
				ppSlvM[Ss] = pVma;
			}
			return 0;
		}
		for (; Ss < SLV_SIZE; Ss++) { /* 填充SLV */
			ppSlvM[Ss] = pVma;
		}
		Sf++;
	}
	for (; Sf < Ef; Sf++) { /* 填充FLV */
		pSlvV = MmSpace.pFlvM[Sf];
		MmSpace.pFlvM[Sf] = pVma;
		if (IS_SLV(pSlvV))
			Free(GET_SLV(pSlvV));
	}
	if (Es) { /* 结束地址不是1级页表对齐 */
		GET_SLV_PTR(ppSlvM, Ef);
		for (i = 0; i < Es; i++) { /* 填充SLV */
			ppSlvM[i] = pVma;
		}
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	检查是否可以合并vma
*
* PARAMETERS
*	SAddr/EAddr: [起始地址, 结束地址)
*
* RETURNS
*	0: 不能合并
*	1: 可以合并
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
*	分裂vma, 所在vma不能被引用!!!
*
* PARAMETERS
*	Addr: 分裂地址
*
* RETURNS
*	0: 成功
*	1: 失败
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
		if (!pVma->pBuf || !pBuf) // TODO: 失败后果严重!!!
			goto _OOM2;
		memcpy((void *)pVma->pBuf, pSVma->pBuf + Size1, (size_t)Size2);
		memcpy(pBuf, pSVma->pBuf, (size_t)Size1);
		PutZeroBuf(pSVma->pBuf, Size1 + Size2);
		pSVma->pBuf = pBuf;
	}
	return SetVmaMapping(pVma); // TODO: 失败后果严重!!!
_OOM2:
	if (pBuf)
		PutZeroBuf(pBuf, Size1);
_OOM1:
	SSTWARN(RS_OOM);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	合并vma, 内存必须连续且不能被引用!!!
*
* PARAMETERS
*	pSVma/pEVma: [起始vma句柄, 结束vma句柄]
*	NeedBuf: 是否需要合并buffer
*
* RETURNS
*	0: 成功
*	1: 失败
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
						DBGERR("没有映射的vma: %s "TADDRFMT"\n", pVma->Ex.pName, pVma->Ex.SAddr);
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
	pSVma->pNext = pEnd; /* 重新链接 */
	pEVma->pNext = NULL;
	while (pVma) { /* 释放这个链表 */
		pEVma = pVma;
		pVma = pVma->pNext;
		FreeVma(pEVma);
	}
	return SetVmaMapping(pSVma); // TODO: 如果失败, 后果严重!!!
_OOM2:
	PutZeroBuf(pBuf, BufSz);
_OOM:
	SSTWARN(RS_OOM);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	将新的vma叠加到内存层中
*
* PARAMETERS
*	Addr/EAddr: [起始地址, 结束地址)
*	Attr: 属性
*	FOff: 文件偏移量，为0时, pBuf必须不为NULL!
*	pBuf: buffer, 为NULL时, FOff必须不为0!
*	pHead: 重叠vma句柄
*	pCmpFunc: 重叠对比函数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	2: 重叠
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
	/* 比较 */
	Addr = (pTail->Ex.SAddr < SAddr ? SAddr : pTail->Ex.SAddr);
	while (1) {
		if (pTail->Ref || (pTail->Ex.Flag&VMA_MLOCK)) {
			DBGERR("区域被引用/锁住: "TADDRFMT"-"TADDRFMT"\n", pTail->Ex.SAddr, pTail->Ex.EAddr);
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
	/* 包含关系, 且属性一致 */
	if (pHead->Ex.SAddr <= SAddr && pHead->Ex.EAddr >= EAddr && VMA_ATTR(MOD_HDL(pHead)) == (Attr&(VMA_RW|VMA_X))) {
		rt = 2;
		goto _FAIL;
	}
	/* 分裂 */
	if (pHead->Ex.SAddr < SAddr) {
		Addr = pTail->Ex.EAddr - 1;
		if (SplitVma(SAddr))
			goto _FAIL;
		pHead = Addr2Vma(SAddr);
		pTail = Addr2Vma(Addr); /* 需要更新pTail */
	}
	if (pTail->Ex.EAddr > EAddr) {
		if (SplitVma(EAddr))
			goto _FAIL;
		pTail = Addr2Vma(EAddr - 1);
	}
	/* 相等关系 */
	if (pHead->Ex.SAddr == SAddr && pHead->Ex.EAddr == EAddr) {
		pHead->Ex.Flag = (pHead->Ex.Flag&~(VMA_RW|VMA_X))|(New.Ex.Flag&(VMA_RW|VMA_X));
		rt = 2;
		goto _FAIL;
	}
	/* 替换 */
	pList = pHead->pNext;
	FreeVmaMap(pHead);
	if (pHead->Ex.Flag&VMA_NAME)
		Free((void *)pHead->Ex.pName);
	New.pNext = pTail = pTail->pNext;
	*pHead = New;
	/* 释放 */
	while (pList != pTail) {
		New.pNext = pList;
		pList = pList->pNext;
		FreeVma(New.pNext);
	}
	return SetVmaMapping(pHead); // TODO: 如果失败, 后果严重!!!
_FAIL:
	FreeVmaMap(&New);
	return rt;
}


/*************************************************************************
*============================ Memory section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据地址获取buffer
*	注意: pVmaHdl被释放后，buffer将无意义!!!
*
* PARAMETERS
*	pVmaHdl: vma句柄
*	Addr/Size: 地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
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
			if (pVma->Ex.Flag&VMA_MEXT) /* 外部映射/无映射 */
				goto _OUT;
			pVma->pBuf = FpMapRo(MmSpace.pFpHdl, pVma->FOff, VMA_SZ(pVma));
			if (!pVma->pBuf) {
				SSTWARN(RS_OOM);
				goto _OUT;
			}
		}
		pBuf = pVma->pBuf + (Addr - pVma->Ex.SAddr);
	} else if (Addr) {
		DBGERR("vma2mem失败: "TADDRFMT"-"TADDRFMT" %s <- "TADDRFMT"(%d)\n", pVmaHdl->SAddr, pVmaHdl->EAddr, pVmaHdl->pName, Addr, Size);
	}
_OUT:
	return pBuf;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取内容并填充至buffer
*
* PARAMETERS
*	Addr: 地址
*	pBuf/Size: 待填充的buffer/大小(字节)
*
* RETURNS
*	0: 成功
*	1: 失败
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
*	根据地址获取字符串
*
* PARAMETERS
*	Addr: 地址
*
* RETURNS
*	NULL: 失败
*	其他: string, 不用时需要释放!!!
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
*	设置vma句柄属性, 覆盖旧属性!!!
*
* PARAMETERS
*	pVmaHdl: vma句柄
*	pName: 名称
*	Flag: 只能是VMA_NAME/VMA_MMASK/VMA_TMASK!!!
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void VmaSetAttr(VMA_HDL *pVmaHdl, const char *pName, unsigned Flag)
{
	VMA_INT * const pVma = MOD_INT(VMA, pVmaHdl);

	if (Flag&~(VMA_NAME|VMA_MMASK|VMA_TMASK))
		DBGERR("flag异常: %x\n", Flag);
	//if (VMA_TYPE(pVmaHdl) != VMA_NONE && VMA_TYPE(pVmaHdl) != VMA_FILE && VMA_TYPE(pVmaHdl) != (Flag&VMA_TMASK))
	//	DBGERR("flag异常: %s, %x, %x\n", pVmaHdl->pName, VMA_TYPE(pVmaHdl) >> VMA_TSHIFT, (Flag&VMA_TMASK) >> VMA_TSHIFT);
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
*	给vma句柄关联1个句柄和获取buffer函数, 覆盖旧句柄和获取buffer函数!!!
*	加载函数不能调用TryMergeVma()!!!
*
* PARAMETERS
*	pVmaHdl: vma句柄
*	pHdl: 句柄
*
* RETURNS
*	无
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
*	给vma句柄关联1个地址, 覆盖旧地址!!!
*
* PARAMETERS
*	pVmaHdl: vma句柄
*	Addr: 上个vma起始地址
*
* RETURNS
*	无
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
*	根据地址获取vma句柄
*
* PARAMETERS
*	Addr: 地址
*
* RETURNS
*	NULL: 失败
*	其他: vma句柄
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
*	释放vma句柄
*
* PARAMETERS
*	pVmaHdl: vma句柄
*
* RETURNS
*	无
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
*	尝试合并指定范围内的vma(类型必须为VMA_NONE)并调用回调函数
*
* PARAMETERS
*	SAddr/EAddr: [起始地址, 结束地址)
*	pFunc: 回调函数
*		pVmaHdl: vma句柄, 离开回调函数将被释放!!!
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int TryMergeVma(TADDR SAddr, TADDR EAddr, int (*pFunc)(VMA_HDL *pVmaHdl, void *pArg), void *pArg)
{
#define INVALID_OFF		 1 /* 不是PAGE对齐的数字即可 */
#define VMA_CANMERGE(pVma)  (!((pVma)->Ref || (!(pVma)->pBuf && ((pVma)->Ex.Flag&VMA_MEXT)))) /* 不合并无buf的外部映射的vma */
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
	EAddr = (EAddr + PAGE_SIZE - 1)&PAGE_MASK; /* 对齐 */
	pVma = Addr2Vma(EAddr);
	if (!pVma) {
		pVma = FindVma(EAddr);
		if (!pVma)
			return 1;
		EAddr = pVma->Ex.EAddr;
	}
	if (SAddr >= EAddr || ADDR_OVERFLOW(SAddr) || !CanMergeVma(SAddr, EAddr)) {
		DBGERR("合并["TADDRFMT"-"TADDRFMT")失败\n", SAddr, EAddr);
		return 1;
	}
	if (SplitVma(SAddr) || SplitVma(EAddr))
		return 1;
	pVma = Addr2Vma(SAddr);
	do {
		if (VMA_CANMERGE(pVma) && pVma->Ex.EAddr != EAddr) { /* 可以合并 */
			Off = (pVma->FOff ? pVma->FOff + VMA_SZ(pVma) : 0);
			Flag = VMA_ATTR(MOD_HDL(pVma));
			pTmp = pVma;
			do {
				if (pTmp->Ex.EAddr != pTmp->pNext->Ex.SAddr || VMA_ATTR(MOD_HDL(pTmp->pNext)) != Flag || !VMA_CANMERGE(pTmp->pNext)) /* 无法继续合并 */
					break;
				pTmp = pTmp->pNext;
				if (Off == INVALID_OFF) /* 不是连续映射 */
					continue;
				if (Off)
					Off = (pTmp->FOff == Off ? (Off + VMA_SZ(pTmp)) : INVALID_OFF);
				else if (pTmp->FOff)
					Off = INVALID_OFF;
			} while (pTmp->Ex.EAddr != EAddr);
			if (pVma != pTmp && MergeVma(pVma, pTmp, Off == INVALID_OFF)) /* 合并 */
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
*	指定范围内地址从小到大遍历vma
*
* PARAMETERS
*	SAddr/EAddr: 范围: [起始地址, 结束地址)
*	pFunc: 回调函数
*		SAddr/EAddr: [起始地址, 结束地址)
*		Flag: 标志, VMA_*
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
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
	if (pVma->Ex.SAddr >= EAddr) /* 起始地址 >= 结束地址 */
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
*	虚拟内存层准备
*	使用方法: PrepareMemSpace() -> CreateMemRegion() -> DestoryMemSpace()
*
* PARAMETERS
*	pFpHdl: fp句柄
*
* RETURNS
*	无
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
 *	创建1个内存区域，以coredump为准， vmlinux往 cordump建立的memspace上覆盖compare
*
* PARAMETERS
 *	Addr/Size: MAP起始地址(VA)和MAP大小(字节), 必须PAGE对齐!!!
 *	Attr: MAP读写属性(VMA_R/VMA_W/VMA_X)
 *	FOff: 文件偏移量相对于elf文件
*	pBuf: buffer, 不为NULL时, FOff为0!!!
 *	pCmpFunc: 重叠对比函数(vmlinux 和 coredump 重叠区域对比)
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	2: 重叠
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
	/* 插入链表 */
	pVma = FindVma(Addr);/* 查找最后1个SAddr <= Addr的vma句柄 , 不存在就不会重叠， 存在需要检查是否重叠 */
	p = (pVma ? (pVma->Ex.EAddr <= Addr ? pVma->pNext : pVma) : MmSpace.pList);/* 找到的pVma Eaddr <=Addr ,p=pVma->pNext比较后面一个(NULL或者SAddr>Addr) */
	if (p && p->Ex.SAddr < EAddr) /* p=pVma->pNext比较后面一个(Addr<p->Ex.SAddr<EAddr)重叠 */
		return VmaOverlay(Addr, EAddr, Attr, FOff, pBuf, p, pCmpFunc, pArg);
	p = Calloc(sizeof(*p));
	if (!p)
		goto _ERR2;
	p->Ex.SAddr = Addr;
	p->Ex.EAddr = EAddr;
	p->FOff = FOff;
	p->pBuf = pBuf;
	p->Ex.Flag = Attr;
	if (pVma) { /* p 插入pVma和pVma->pNext之间  */
		p->pNext = pVma->pNext;
		pVma->pNext = p;
	} else {
		p->pNext = MmSpace.pList;
		MmSpace.pList = p; /* MmSpace.pList指向最小？ */
	}
	if (SetVmaMapping(p))
		p = NULL; /* DestoryMemSpace()会释放!!! */
	return !p;
_ERR2:
	SSTWARN(RS_OOM);
	return 1;
_ERR1:
	SSTERR(RS_EADDR, MmSpace.pFpHdl->pPath, "PT_LOAD", Addr);
	return 0; /* 忽略这种错误! */
}

/*************************************************************************
* DESCRIPTION
*	注销虚拟内存层
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
void DestoryMemSpace(void)
{
	VMA_INT *pVma = MmSpace.pList;
	const VMA_INT *pSlvV;
	size_t i;

	if (pVma) {
		/* 释放vma */
		do {
			MmSpace.pList = MmSpace.pList->pNext;
			if (MmSpace.pList && pVma->Ex.EAddr > MmSpace.pList->Ex.SAddr)
				DBGERR("内存层异常: "TADDRFMT" > "TADDRFMT"\n", pVma->Ex.EAddr, MmSpace.pList->Ex.SAddr);
			FreeVma(pVma);
		} while ((pVma = MmSpace.pList) != NULL);
		/* 释放映射 */
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
