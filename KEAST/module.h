/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	module.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: 模块通用函数定义
**********************************************************************************/
#ifndef _MODULE_H
#define _MODULE_H

#include <stdio.h>
#include <ttypes.h>

/* 转换: 模块外部结构 <=> 模块内部结构 */
#define OFFSET_OF(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)
#define MOD_INT(Type, Hdl)		((Type##_INT *)((char *)(Hdl) - OFFSET_OF(Type##_INT, Ex)))
#define MOD_HDL(Hdl)			(&(Hdl)->Ex)

#define ARY_CNT(a)				(sizeof(a) / sizeof((a)[0]))
#define ARY_END(a)				a + ARY_CNT(a) - 1

/* 等效于:(x&bit1) ? bit2 : 0, 注意: bit1/bit2必须是2的指数!!! */
#define FLAG_TRANS(x, bit1, bit2) ((bit1) <= (bit2) ? ((x)&(bit1)) * ((bit2) / (bit1)) : ((x)&(bit1)) / ((bit1) / (bit2)))

#define OFFVAL_U1(Ptr, Off)		(*((unsigned char *)(Ptr) + (Off)))
#define OFFVAL_S1(Ptr, Off)		(*((char *)(Ptr) + (Off)))
#define OFFVAL_U2(Ptr, Off)		(*(unsigned short *)((char *)(Ptr) + (Off)))
#define OFFVAL_S2(Ptr, Off)		(*(short	 *)((char *)(Ptr) + (Off)))
#define OFFVAL_U4(Ptr, Off)		(*(unsigned  *)((char *)(Ptr) + (Off)))
#define OFFVAL_S4(Ptr, Off)		(*(int	   *)((char *)(Ptr) + (Off)))
#define OFFVAL_U8(Ptr, Off)		(*(unsigned long long *)((char *)(Ptr) + (Off)))
#define OFFVAL_S8(Ptr, Off)		(*(long long *)((char *)(Ptr) + (Off)))
#define OFFVAL_F(Ptr, Off)		(*(float	 *)((char *)(Ptr) + (Off)))
#define OFFVAL_D(Ptr, Off)		(*(double	*)((char *)(Ptr) + (Off)))
#define OFFVAL_P(Ptr, Off)		(*(void	 **)((char *)(Ptr) + (Off)))
#define OFFVAL(Type, Ptr, Off)	(*(Type	  *)((char *)(Ptr) + (Off)))
#define OFFVAL_ADDR(Ptr, Off)	(*(TADDR	 *)((char *)(Ptr) + (Off)))
#define OFFVAL_SIZE(Ptr, Off)	(*(TSIZE	 *)((char *)(Ptr) + (Off)))


#define MOD_NOT_SUPPORT		0 /* 不支持或未初始化, 必须为0!!! */
#define MOD_OOM				1 /* 内存不足, 必须在MOD_NOT_SUPPORT之后!!! */
#define MOD_ADDR			2 /* 获取地址失败 */
#define MOD_CORRUPTED		3 /* 句柄被踩坏 */
#define MOD_OK				4 /* 正常 */

#define MOD_USR_BIT			3 /* 各模块独自定义起始bit位 */
#define MOD_SYS_MAX			(1 << MOD_USR_BIT)
#define MOD_STATUS(ModErr)	((ModErr)&(MOD_SYS_MAX - 1))


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	输入模块异常信息到文件
*
* PARAMETERS
*	fp: 文件句柄
*	ModErr: 模块异常信息
*	pModName: 模块名称
*
* RETURNS
*	0: 正常
*	1: 异常
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DispModErr(FILE *fp, unsigned ModErr, const char *pModName);


typedef const struct _LIST_HEAD
{
	TADDR NextAddr;
	TADDR PrevAddr;
} LIST_HEAD;

#define LIST_ENTRY_ADDR(ListAddr, Type, Member) ((TADDR)(ListAddr) - OFFSET_OF(Type, Member))

/*************************************************************************
* DESCRIPTION
*	查找或插入哈希表
*
* PARAMETERS
*	pHtblHdl: 哈希表句柄
*	HashVal: 哈希值
*	pData: 查找或插入的数据, 不能为NULL!!!
*	pCmpFunc: 比较函数, 返回0表示一样, 否则不一样
*		pTblData: 哈希表数据
*		pData: 对比数据
*	DoAdd: 是否插入
*
* RETURNS
*	NULL: 失败
*	其他: 数据
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef void * HTBL_HDL;
void *HtblLookup(HTBL_HDL *pHtblHdl, unsigned HashVal, void *pData, int (*pCmpFunc)(void *pTblData, void *pData), int DoAdd);

/*************************************************************************
* DESCRIPTION
*	用回调函数处理扫描一遍hash表
*
* PARAMETERS
*	pHtblHdl: hash table handle
*
* RETURNS
*	hash表记录的个数
*
* GLOBAL AFFECTED
*
*************************************************************************/
unsigned HtblGetUsedCnt(HTBL_HDL *pHtblHdl);

/*************************************************************************
* DESCRIPTION
*	遍历hash表
*
* PARAMETERS
*	pHtblHdl: 哈希表句柄
*	pFunc: 回调函数
*		pData: 数据
*		pArg: arg
*	pArg: 额外的参数
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef int (*HTBL_CB)(void *pData, void *pArg);
int HtblForEach(HTBL_HDL *pHtblHdl, HTBL_CB pFunc, void *pArg);

/*************************************************************************
* DESCRIPTION
*	创建哈希表
*
* PARAMETERS
*	Size: 初始大小
*	pFreeFunc: 释放函数(释放哈希表时调用), 可以为NULL
*
* RETURNS
*	NULL: 失败
*	其他: 哈希表句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
HTBL_HDL *GetHtblHdl(unsigned Size, void (*pFreeFunc)(void *pData));

/*************************************************************************
* DESCRIPTION
*	释放哈希表, 对每项数据调用释放函数(如果有注册的话)
*
* PARAMETERS
*	HtblHdl: 哈希表句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutHtblHdl(HTBL_HDL *pHtblHdl);

/*************************************************************************
* DESCRIPTION
*	获取指定cpu的per cpu变量地址
*
* PARAMETERS
*	PerCpuBase: per cpu变量地址
*	CpuId: CPU id
*
* RETURNS
*	0: 失败
*	其他: 指定cpu的per cpu变量地址
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR PerCpuAddr(TADDR PerCpuBase, TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	遍历链表
*
* PARAMETERS
*	ListAddr: list_head地址
*	ListOff: 结构体相对于list_head的偏移量
*	pFunc: 回调函数
*		HdlAddr: 句柄地址
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
int ForEachList(TADDR ListAddr, TUINT ListOff, int (*pFunc)(TADDR HdlAddr, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	最高位为1的bit位置(0~32)
*	0返回为0, 1返回为1, 3返回为2
*
* PARAMETERS
*	Mask: 值
*
* RETURNS
*	最高位为1的bit位置(0~32)
*
* GLOBAL AFFECTED
*
*************************************************************************/
TUINT Fhs(TUINT Mask);

/*************************************************************************
* DESCRIPTION
*	显示数据(可能是地址)对应的符号
*
* PARAMETERS
*	pDispFunc: 显示回调函数
*	fp: 文件句柄
*	Val: 数据
*
* RETURNS
*	长度
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ShowSymVal(int (*pDispFunc)(FILE *fp, const char *pFmt, ...), FILE *fp, TADDR Val);

/*************************************************************************
* DESCRIPTION
*	从空格隔开的字符串中提取子字符串
*
* PARAMETERS
*	ppStr: 字符串指针地址，会被更新
*	cc: 隔断字符
*
* RETURNS
*	子字符串
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *GetSubStr(char **ppStr, char cc);

/*************************************************************************
* DESCRIPTION
*	复制文件
*
* PARAMETERS
*	pDstFile: 目标文件
*	pSrcFile: 源文件
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int CCpyFile(const char *pDstFile, const char *pSrcFile);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: 模块关闭
*	1: 模块可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ModuleInit(void);

/*************************************************************************
* DESCRIPTION
*	注销模块
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
void ModuleDeInit(void);

#endif
