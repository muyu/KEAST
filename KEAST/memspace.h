/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	memspace.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/9/17
* Description: 内存虚拟层
**********************************************************************************/
#ifndef _MEMSPACE_H
#define _MEMSPACE_H

#include <ttypes.h>

#define VMA_SIZE(Hdl)	   ((Hdl)->EAddr - (Hdl)->SAddr)

#if defined(__MINGW32__)
#include <stdint.h>   /*For define uintptr_t*/
#endif



typedef const struct _VMA_HDL
{
	TADDR SAddr, EAddr, LinkAddr; /* [起始地址, 结束地址), 链接地址 */
	const char *pName; /* 可能为NULL!!! */
	const void *pHdl; /* 为VMA_ELF时是SYM_HDL, 为VMA_DEX时是DEX_HDL, 可能为NULL!! */

#define VMA_NAME	(1 << 3) /* 名字不是常量字符串 */
#define VMA_RBITS	4 /* 预留bit位数(内部使用) */

#define VMA_MSHIFT	VMA_RBITS
#define VMA_X		(1 << (VMA_MSHIFT + 0)) /* 可执行 */
#define VMA_W		(1 << (VMA_MSHIFT + 1)) /* 可写 */
#define VMA_R		(1 << (VMA_MSHIFT + 2)) /* 可读 */
#define VMA_RW		(VMA_R|VMA_W)
#define VMA_MBITS	3
#define VMA_MMASK	(((1 << VMA_MBITS) - 1) << VMA_MSHIFT)
#define VMA_ATTR(pVmaHdl) ((pVmaHdl)->Flag&VMA_MMASK)

#define VMA_TSHIFT	(VMA_MSHIFT + VMA_MBITS)
#define VMA_NONE	(0 << VMA_TSHIFT)
#define VMA_ELF		(1 << VMA_TSHIFT)
#define VMA_FILE	(2 << VMA_TSHIFT) /* 普通文件 */
#define VMA_HEAP	(3 << VMA_TSHIFT)
#define VMA_STACK	(4 << VMA_TSHIFT)
#define VMA_IGNORE	(5 << VMA_TSHIFT)
#define VMA_TBITS	4
#define VMA_TMASK	(((1 << VMA_TBITS) - 1) << VMA_TSHIFT)
#define VMA_TYPE(pVmaHdl)	((pVmaHdl)->Flag&VMA_TMASK)
	unsigned Flag;
} VMA_HDL;


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
*	Addr: 地址
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *Vma2Mem(VMA_HDL *pVmaHdl, TADDR Addr, TSIZE Size);

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
int DumpMem(TADDR Addr, void *pBuf, TSIZE Size);

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
char *GetString(TADDR Addr);

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
void VmaSetAttr(VMA_HDL *pVmaHdl, const char *pName, unsigned Flag);

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
void VmaRegHdl(VMA_HDL *pVmaHdl, const void *pHdl);

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
void VmaLink(VMA_HDL *pVmaHdl, TADDR Addr);


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
VMA_HDL *GetVmaHdl(TADDR Addr);

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
void PutVmaHdl(VMA_HDL *pVmaHdl);

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
int TryMergeVma(TADDR SAddr, TADDR EAddr, int (*pFunc)(VMA_HDL *pVmaHdl, void *pArg), void *pArg);

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
#define ForEachVmaAll(pFunc, pArg)  ForEachVma(0, ~(TADDR)0, pFunc, pArg)
int ForEachVma(TADDR SAddr, TADDR EAddr, int (*pFunc)(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg), void *pArg);


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
void PrepareMemSpace(const void *pFpHdl);

/*************************************************************************
* DESCRIPTION
*	创建1个内存区域
*
* PARAMETERS
*	Addr/Size: 起始地址和大小(字节), 必须PAGE对齐!!!
*	Attr: 读写属性(VMA_R/VMA_W/VMA_X)
*	FOff: 文件偏移量
*	pBuf: buffer, 不为NULL时, FOff为0!!!
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
int CreateMemRegion(TADDR Addr, TSIZE Size, unsigned Attr, TSIZE FOff, const void *pBuf
	, int (*pCmpFunc)(TADDR Addr, TSIZE Size, const void *pBuf1, const void *pBuf2, void *pArg), void *pArg);

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
void DestoryMemSpace(void);

#endif
