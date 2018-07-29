/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	slub.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: slub模块解析
**********************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <module.h>
#include "string_res.h"
#include <mrdump.h>
#include "page.h"
#include "slub.h"

#define SLUB_RED_INACTIVE	0xBB
#define SLUB_RED_ACTIVE		0xCC

#define POISON_INUSE		0x5A /* for use-uninitialised poisoning */
#define POISON_FREE			0x6B /* for use-after-free poisoning */
#define POISON_END			0xA5 /* end-byte of poisoning */

#define TRACK_ADDRS_COUNT	8
typedef const struct _SLUB_TRACK
{
	TADDR Addr;	/* Called from address */
	TUINT Addrs[TRACK_ADDRS_COUNT];	/* Called from address */
	TINT Cpu, Pid;
	TSIZE When;
} SLUB_TRACK;

typedef const struct _KMEM_CACHE_CPU
{
	TADDR FreeListAddr;
	TSIZE Tid;
	TADDR PageAddr, PartialAddr;
} KMEM_CACHE_CPU;

typedef const struct _KMEM_CACHE
{
	TADDR PcSlabAddr;
	/* Internal SLUB flags */
#define __OBJECT_POISON		0x80000000UL /* Poison object */
#define __CMPXCHG_DOUBLE	0x40000000UL /* Use cmpxchg_double */
	TSIZE Flags, MinPartial;
	TINT Size, ObjSize, Off; /* obj包含meta大小/obj没包含meta大小/空闲指针偏移 */
	TINT CpuPartial;
	TSIZE Order, MaxOrder, MinOerder;
	TUINT AllocFlags, RefCnt;
	TADDR CtorFunAddr;
	TINT Inuse, Align, Reserved;
	TADDR NameAddr;
	LIST_HEAD List;
} KMEM_CACHE;

typedef struct _SLUB_ARG
{
	int (*pFunc)(TADDR ObjAddr, int IsFree, void *pArg);
	void *pArg;
	TUINT Size, TotPSz;
} SLUB_ARG;

typedef enum _SLUB_NODE_ID
{
#define SLUB_NODEOFF_MAP \
	_NL(SLUB_N_PARTIAL,		"partial") \
	/* 这里添加新的成员 */ \
	_NL(SLUB_N_FULL,		"full") \

#define _NL(Enum, str) Enum,
	SLUB_NODEOFF_MAP
#undef _NL
} SLUB_NODE_ID;

static struct
{
	TUINT NodeSz;
	int NodeOff;
	int NOff[SLUB_N_FULL + 1];
} SlubInfs;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	检查指定的memory是否都是指定的值
*
* PARAMETERS
*	pSrc: 内存起始地址
*	Size: 大小
*	Val: 值
*
* RETURNS
*	0: 存在不是指定的值
*	1: 都是指定的值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MemCheck(const void *pSrc, unsigned Size, unsigned char Val)
{
	const unsigned char *p = pSrc;
	const unsigned char *q = p + Size;
	const unsigned *s;
	unsigned val, i;

	for (i = (4 - (uintptr_t)p)&3; i > 0; i--) {
		if (*p++ != Val) return 0;
	}
	for (i = (uintptr_t)q&3; i > 0; i--) {
		if (*--q != Val) return 0;
	}
	val = Val|(Val << 8)|(Val << 16)|(Val << 24);
	for (s = (unsigned *)p; s < (unsigned *)q; ) {
		if (*s++ != val) return 0;
		if (s >= (unsigned *)q) break; if (*s++ != val) return 0;
		if (s >= (unsigned *)q) break; if (*s++ != val) return 0;
		if (s >= (unsigned *)q) break; if (*s++ != val) return 0;
	}
	return 1;
}


static int ForEachSlubPage(TADDR PageAddr, void *pArg)
{
	SLUB_ARG *pSlubArg = pArg;
	TADDR Addr, EAddr;

	for (Addr = PageAddr2VAddr(PageAddr), EAddr = Addr + pSlubArg->TotPSz; Addr < EAddr; Addr += pSlubArg->Size) {
		pSlubArg->pFunc(Addr, 0, pSlubArg->pArg);
	}
	return 0;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取slub信息
*
* PARAMETERS
*	pSlubInf: 返回slub信息
*	TskAddr: kmem_cache地址
*
* RETURNS
*	NULL: 失败
*	其他: slub信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
SLUB_INF *GetSlubInf(struct _SLUB_INF *pSlubInf, TADDR SlubAddr)
{
	VMA_HDL *pVmaHdl;
	KMEM_CACHE *s;

	pVmaHdl = GetVmaHdl(SlubAddr);
	if (!pVmaHdl)
		return NULL;
	s = Vma2Mem(pVmaHdl, SlubAddr, sizeof(*s));
	if (!s)
		goto _OUT;
	memset(pSlubInf, 0, sizeof(*pSlubInf));
	pSlubInf->Flags = s->Flags;
	pSlubInf->ObjSz = s->ObjSize;
	pSlubInf->InUse = s->Inuse;
	pSlubInf->Off	= s->Off;
	DumpMem(s->NameAddr, pSlubInf->Name, sizeof(pSlubInf->Name) - 1);
_OUT:
	PutVmaHdl(pVmaHdl);
	return pSlubInf;
}

/*************************************************************************
* DESCRIPTION
*	获取slub obj信息
*
* PARAMETERS
*	pSlubInf: slub信息
*	pSObjInf: 返回slub obj信息
*	SObjAddr: slub obj地址
*
* RETURNS
*	NULL: 失败
*	其他: slub obj信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
SOBJ_INF *GetSlubObjInf(SLUB_INF pSlubInf, struct _SOBJ_INF *pSObjInf, TADDR SObjAddr)
{
	(void)pSlubInf, (void)pSObjInf, (void)SObjAddr;
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	扫面slub的页面
*
* PARAMETERS
*	pSlubHdl: slub句柄
*	pPage: 需要分析的页面
*	pFunc: obj输出函数
*		pPC: 前一个obj结构体，可能为NULL
*		pCC: 当前obj结构体
*		pArg: arg
*	pArg: 传给pFunc()的参数
*
* RETURNS
*	0: 成功
*	1: 链表异常
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachSlubObj(TADDR SlubAddr, int (*pFunc)(TADDR ObjAddr, int IsFree, void *pArg), void *pArg)
{
	const TUINT CoreCnt = GetKExpInf()->CoreCnt;
	VMA_HDL *pVmaHdl;
	struct _KMEM_CACHE_CPU c;
	KMEM_CACHE *s;
	SLUB_ARG SlubArg;
	TUINT CpuId;
	TADDR Addr;
	int rt = 1;

	pVmaHdl = GetVmaHdl(SlubAddr);
	if (!pVmaHdl)
		return rt;
	s = Vma2Mem(pVmaHdl, SlubAddr, sizeof(*s));
	if (!s)
		goto _OUT;
	SlubArg.pFunc = pFunc;
	SlubArg.pArg = pArg;
	SlubArg.Size = s->Size;
	SlubArg.TotPSz = PAGE_SIZE << (s->Order >> 16);
	for (CpuId = 0; CpuId < CoreCnt; CpuId++) {
		if (DumpMem(PerCpuAddr(s->PcSlabAddr, CpuId), &c, sizeof(c)))
			continue;
		for (Addr = c.PageAddr; Addr; ) {
			rt = ForEachSlubPage(Addr, &SlubArg);
			if (rt || DumpMem(Addr + OFFSET_OF(PAGE_HEAD, u3.Slub.NextAddr), &Addr, sizeof(Addr)))
				break;
		}
		for (Addr = c.PartialAddr; Addr; ) {
			rt = ForEachSlubPage(Addr, &SlubArg);
			if (rt || DumpMem(Addr + OFFSET_OF(PAGE_HEAD, u3.Slub.NextAddr), &Addr, sizeof(Addr)))
				break;
		}
	}
	/* 遍历kmem_cache_node */
	ForEachList(OFFVAL_ADDR(s, SlubInfs.NodeOff) + SlubInfs.NOff[SLUB_N_PARTIAL], OFFSET_OF(PAGE_HEAD, u3.Lru), ForEachSlubPage, &SlubArg);
	ForEachList(OFFVAL_ADDR(s, SlubInfs.NodeOff) + SlubInfs.NOff[SLUB_N_FULL], OFFSET_OF(PAGE_HEAD, u3.Lru), ForEachSlubPage, &SlubArg);
_OUT:
	PutVmaHdl(pVmaHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	遍历slub
*
* PARAMETERS
*	pFunc: 回调函数
*		SlubAddr: slub地址
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 链表异常
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachSlubAddr(int (*pFunc)(TADDR SlubAddr, void *pArg), void *pArg)
{
	TADDR SlabHeadAddr;

	SlabHeadAddr = SymName2Addr(pVmSHdl, NULL, "slab_caches", TGLBVAR);
	if (!SlabHeadAddr)
		return 1;
	return ForEachList(SlabHeadAddr, OFFSET_OF(KMEM_CACHE, List), pFunc, pArg);
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化slub系统模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: slub系统关闭
*	1: slub系统可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SlubInit(void)
{
	static const char *pNames[] = {"node"};
	static const char *pNNames[] =
	{
#define _NL(Enum, str) str,
		SLUB_NODEOFF_MAP
#undef _NL
	};
	struct _DBG_HDL fDbgHdl, DbgHdl;

	if (!SymFile2DbgHdl(pVmSHdl, &fDbgHdl, "slub.c")) {
		if (!DbgGetCDbgHdl(&fDbgHdl, &DbgHdl, "kmem_cache"))
			DbgGetChildOffs(&DbgHdl, &SlubInfs.NodeOff, pNames, ARY_CNT(pNames));
		if (!DbgGetCDbgHdl(&fDbgHdl, &DbgHdl, "kmem_cache_node")) {
			SlubInfs.NodeSz = (TUINT)DbgGetByteSz(&DbgHdl);
			DbgGetChildOffs(&DbgHdl, SlubInfs.NOff, pNNames, ARY_CNT(pNNames));
		}
	}
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	注销slub系统模块
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
void SlubDeInit(void)
{

}
