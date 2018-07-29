/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	slub.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: slub模块解析
**********************************************************************************/
#ifndef _SLUB_H
#define _SLUB_H


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取slub信息
*
* PARAMETERS
*	pSlubInf: 返回slub信息
*	SlubAddr: kmem_cache地址
*
* RETURNS
*	NULL: 失败
*	其他: slub信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef const struct _SLUB_INF
{
#define SLAB_DEBUG_FREE 	0x00000100UL /* DEBUG: Perform (expensive) checks on free */
#define SLAB_RED_ZONE   	0x00000400UL /* DEBUG: Red zone objs in a cache */
#define SLAB_POISON 		0x00000800UL /* DEBUG: Poison objects */
#define SLAB_HWCACHE_ALIGN  0x00002000UL /* Align objs on cache lines */
#define SLAB_CACHE_DMA  	0x00004000UL /* Use GFP_DMA memory */
#define SLAB_STORE_USER 	0x00010000UL /* DEBUG: Store the last owner for bug hunting */
#define SLAB_RECLAIM_ACCOUNT 0x00020000UL /* Objects are reclaimable */
#define SLAB_PANIC  		0x00040000UL /* Panic if kmem_cache_create() fails */
#define SLAB_DESTROY_BY_RCU 0x00080000UL /* Defer freeing slabs to RCU */
#define SLAB_MEM_SPREAD 	0x00100000UL /* Spread some memory over cpuset */
#define SLAB_TRACE  		0x00200000UL /* Trace allocations and frees */
#define SLAB_DEBUG_OBJECTS  0x00400000UL /* Flag to prevent checks on free */
#define SLAB_NOLEAKTRACE	0x00800000UL /* Avoid kmemleak tracing */
#define SLAB_NOTRACK		0x01000000UL /* Don't track use of uninitialized memory */
#define SLAB_FAILSLAB   	0x02000000UL /* Fault injection mark */
#define SLAB_NO_DEBUG		0x10000000UL /* Disable slub debug for special cache */
	TSIZE Flags;

	TUINT ObjSz, InUse, Off;
	char Name[32];
} SLUB_INF;
SLUB_INF *GetSlubInf(struct _SLUB_INF *pSlubInf, TADDR SlubAddr);

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
typedef const struct _SOBJ_INF
{
#define SLUB_TRACK_CNT	8
	TADDR MBt[SLUB_TRACK_CNT], FBt[SLUB_TRACK_CNT];
	TUINT MBtCnt, FBtCnt;

#define SOBJ_E_REDZ 	(1 << 0) /* red zone异常 */
#define SOBJ_E_PAD  	(1 << 1) /* 对齐填充异常 */
#define SOBJ_E_UFREE	(1 << 2) /* 释放后又使用了slub */
#define SOBJ_E_FP   	(1 << 3) /* freepointer异常 */
	TUINT Err;
} SOBJ_INF;
SOBJ_INF *GetSlubObjInf(SLUB_INF pSlubInf, struct _SOBJ_INF *pSObjInf, TADDR SObjAddr);

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
int ForEachSlubObj(TADDR SlubAddr, int (*pFunc)(TADDR ObjAddr, int IsFree, void *pArg), void *pArg);

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
int ForEachSlubAddr(int (*pFunc)(TADDR SlubAddr, void *pArg), void *pArg);


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
int SlubInit(void);

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
void SlubDeInit(void);

#endif
