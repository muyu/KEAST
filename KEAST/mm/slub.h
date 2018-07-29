/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	slub.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: slubģ�����
**********************************************************************************/
#ifndef _SLUB_H
#define _SLUB_H


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡslub��Ϣ
*
* PARAMETERS
*	pSlubInf: ����slub��Ϣ
*	SlubAddr: kmem_cache��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: slub��Ϣ
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
*	��ȡslub obj��Ϣ
*
* PARAMETERS
*	pSlubInf: slub��Ϣ
*	pSObjInf: ����slub obj��Ϣ
*	SObjAddr: slub obj��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: slub obj��Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef const struct _SOBJ_INF
{
#define SLUB_TRACK_CNT	8
	TADDR MBt[SLUB_TRACK_CNT], FBt[SLUB_TRACK_CNT];
	TUINT MBtCnt, FBtCnt;

#define SOBJ_E_REDZ 	(1 << 0) /* red zone�쳣 */
#define SOBJ_E_PAD  	(1 << 1) /* ��������쳣 */
#define SOBJ_E_UFREE	(1 << 2) /* �ͷź���ʹ����slub */
#define SOBJ_E_FP   	(1 << 3) /* freepointer�쳣 */
	TUINT Err;
} SOBJ_INF;
SOBJ_INF *GetSlubObjInf(SLUB_INF pSlubInf, struct _SOBJ_INF *pSObjInf, TADDR SObjAddr);

/*************************************************************************
* DESCRIPTION
*	ɨ��slub��ҳ��
*
* PARAMETERS
*	pSlubHdl: slub���
*	pPage: ��Ҫ������ҳ��
*	pFunc: obj�������
*		pPC: ǰһ��obj�ṹ�壬����ΪNULL
*		pCC: ��ǰobj�ṹ��
*		pArg: arg
*	pArg: ����pFunc()�Ĳ���
*
* RETURNS
*	0: �ɹ�
*	1: �����쳣
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachSlubObj(TADDR SlubAddr, int (*pFunc)(TADDR ObjAddr, int IsFree, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	����slub
*
* PARAMETERS
*	pFunc: �ص�����
*		SlubAddr: slub��ַ
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
int ForEachSlubAddr(int (*pFunc)(TADDR SlubAddr, void *pArg), void *pArg);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��slubϵͳģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: slubϵͳ�ر�
*	1: slubϵͳ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SlubInit(void);

/*************************************************************************
* DESCRIPTION
*	ע��slubϵͳģ��
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
void SlubDeInit(void);

#endif
