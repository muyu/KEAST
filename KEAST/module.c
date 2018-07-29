/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	module.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: 模块通用函数定义
**********************************************************************************/
#include <string.h>
#include <stdio.h> /* just for sprintf() api */
#include "os.h"
#include "string_res.h"
#include <mrdump.h>
#include <module.h>


#define LOAD_NUMER  3 /* 75% */
#define LOAD_DENOM  4

typedef struct _HASH_UNIT
{
	unsigned HashVal;
	void *pData;
} HASH_UNIT;

typedef struct _HASH_TBL
{
	unsigned TblSize, UsedCnt;
	void (*pFreeFunc)(void *pData);
	HASH_UNIT *pEntries;
} HASH_TBL;


const unsigned *cpu_online_mask;
static TSIZE PerCpuOff[MAX_CORE];


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	找最近的2的n次方
*
* PARAMETERS
*	Val: 值
*
* RETURNS
*	2的n次方
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static size_t RoundUpPower2(size_t Val)
{
	Val--;
	Val |= Val >> 1;
	Val |= Val >> 2;
	Val |= Val >> 4;
	Val |= Val >> 8;
	Val |= Val >> 16;
	Val++;
	return Val;
}

/*************************************************************************
* DESCRIPTION
*	缩放哈希表
*
* PARAMETERS
*	pHashTbl: 哈希表句柄
*	NewSize: 新的大小
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static int ResizeHtbl(HASH_TBL *pHashTbl, unsigned NewSize)
{
	HASH_UNIT *pOldEs, *pNewEs;
	unsigned i, j;
	void *pData;

	pNewEs = Calloc(NewSize * sizeof(*pNewEs)); /* 扩充2倍 */
	if (pNewEs) {
		NewSize--;
		pOldEs = pHashTbl->pEntries;
		for (i = 0; i < pHashTbl->TblSize; i++) {
			pData = pOldEs[i].pData;
			if (pData) {
				j = pOldEs[i].HashVal&NewSize;
				while (pNewEs[j].pData) {
					j = (j + 1)&NewSize;
				}
				pNewEs[j] = pOldEs[i];
			}
		}
		Free(pOldEs);
		pHashTbl->pEntries = pNewEs;
		pHashTbl->TblSize  = NewSize + 1;
	}
	return !pNewEs;
}


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
int DispModErr(FILE *fp, unsigned ModErr, const char *pModName)
{
#ifdef _DEBUG
	if (ModErr >= MOD_SYS_MAX && !MOD_STATUS(ModErr))
		DBGERR("错误的模块异常值: %X\n", ModErr);
#endif
	switch (MOD_STATUS(ModErr)) {
	case MOD_OK:
	case MOD_NOT_SUPPORT:
	case MOD_OOM:		return 0;
	case MOD_ADDR:	   LOG(fp, RS_GET_ADDR,  pModName); break;
	case MOD_CORRUPTED:  LOG(fp, RS_CORRUPTED, pModName); break;
	default:;
	}
	LOGC(fp, "\n\n");
	return 1;
}

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
void *HtblLookup(HTBL_HDL *pHtblHdl, unsigned HashVal, void *pData, int (*pCmpFunc)(void *pTblData, void *pData), int DoAdd)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;
	HASH_UNIT *pEntry, *pEnd;
	void *rt = NULL;

	pEntry = &pHashTbl->pEntries[HashVal&(pHashTbl->TblSize - 1)];
	pEnd   = &pHashTbl->pEntries[pHashTbl->TblSize];
	while (pEntry->pData) {
		if (pEntry->HashVal == HashVal && !pCmpFunc(pEntry->pData, pData)) /* match */
			break;
		if (++pEntry == pEnd) /* 回转 */
			pEntry = pHashTbl->pEntries;
	}
	if (!pEntry->pData) {
		if (DoAdd) {
			pEntry->HashVal = HashVal;
			pEntry->pData = rt = pData;
			pHashTbl->UsedCnt++;
			if (pHashTbl->UsedCnt * LOAD_DENOM >= pHashTbl->TblSize * LOAD_NUMER && ResizeHtbl(pHashTbl, pHashTbl->TblSize << 1)) { /* 扩充2倍 */
				pHashTbl->UsedCnt--;
				pEntry->HashVal = 0;
				pEntry->pData = rt = NULL;
			}
		}
	} else {
		rt = pEntry->pData;
	}
	return rt;
}

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
unsigned HtblGetUsedCnt(HTBL_HDL *pHtblHdl)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;

	return pHashTbl->UsedCnt;
}

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
int HtblForEach(HTBL_HDL *pHtblHdl, HTBL_CB pFunc, void *pArg)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;
	HASH_UNIT *pEntry;
	unsigned i;
	int rt = 0;

	for (i = 0; i < pHashTbl->TblSize; i++) {
		pEntry = &pHashTbl->pEntries[i];
		if (pEntry->pData) {
			rt = pFunc(pEntry->pData, pArg);
			if (rt)
				break;
		}
	}
	return rt;
}

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
HTBL_HDL *GetHtblHdl(unsigned Size, void (*pFreeFunc)(void *pData))
{
	HASH_TBL *pHashTbl;

	Size = RoundUpPower2(Size);
	pHashTbl = Malloc(sizeof(*pHashTbl));
	if (pHashTbl) {
		pHashTbl->pEntries = Malloc(Size * sizeof(*pHashTbl->pEntries));
		if (pHashTbl->pEntries) {
			memset(pHashTbl->pEntries, 0, Size * sizeof(*pHashTbl->pEntries));
			pHashTbl->UsedCnt   = 0;
			pHashTbl->TblSize   = Size;
			pHashTbl->pFreeFunc = pFreeFunc;
		} else {
			Free(pHashTbl);
			pHashTbl = NULL;
		}
	}
	return (HTBL_HDL *)pHashTbl;
}

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
void PutHtblHdl(HTBL_HDL *pHtblHdl)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;
	void (*pFreeFunc)(void *pData);
	HASH_UNIT *pEntry;
	unsigned Size;

	pFreeFunc = pHashTbl->pFreeFunc;
	if (pFreeFunc) {
		for (pEntry = pHashTbl->pEntries, Size = pHashTbl->UsedCnt; Size > 0; pEntry++) {
			if (pEntry->pData) {
				pFreeFunc(pEntry->pData);
				Size--;
			}
		}
	}
	Free(pHashTbl->pEntries);
	Free(pHashTbl);
}

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
TADDR PerCpuAddr(TADDR PerCpuBase, TUINT CpuId)
{
	return PerCpuOff[CpuId] ? PerCpuBase + PerCpuOff[CpuId] : 0;
}

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
int ForEachList(TADDR ListAddr, TUINT ListOff, int (*pFunc)(TADDR HdlAddr, void *pArg), void *pArg)
{
	const TADDR ListHeadAddr = ListAddr;
	int rt = 0;

	do {
		if (DumpMem(ListAddr, &ListAddr, sizeof(ListAddr)))
			return 1;
		if (ListAddr == ListHeadAddr)
			break;
		rt = pFunc(ListAddr - ListOff, pArg);
	} while (!rt);
	return rt;
}

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
TUINT Fhs(TUINT Mask)
{
	static const char t[16] = {-28, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
	TUINT bit = 0;

	if (!(Mask&0xFFFF)) {
		bit += 16;
		Mask >>= 16;
	}
	if (!(Mask&0xFF)) {
		bit += 8;
		Mask >>= 8;
	}
	if (!(Mask&0xF)) {
		bit += 4;
		Mask >>= 4;
	}
	return (bit + t[Mask&0xF]);
}

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
int ShowSymVal(int (*pDispFunc)(FILE *fp, const char *pFmt, ...), FILE *fp, TADDR Val)
{
	const char *pName;
	SYM_HDL *pSymHdl;
	TADDR SAddr;
	int Sz;

	pSymHdl = GetSymHdlByAddr(Val);
	if (pSymHdl) {
		pName = SymAddr2Name(pSymHdl, &SAddr, Val, TALLVAR); /* 是否是变量? */
		if (pName) {
			Sz = pDispFunc(fp, "%s", pName);
			if (Val != SAddr)
				Sz += pDispFunc(fp, " + %d", (size_t)(Val - SAddr));
			return Sz;
		}
		pName = SymAddr2Name(pSymHdl, &SAddr, Val, TALLFUN); /* 是否是函数? */
		if (pName && SAddr == Val)
			return (pName == pUndFuncName ? pDispFunc(fp, TADDRFMT"()", Val) : pDispFunc(fp, "%s()", pName));
	}
	return Val + 0x8000 <= 0x10000/* [-32767, 32768] */ ? pDispFunc(fp, "%d", (int)Val) : pDispFunc(fp, TADDRFMT, Val);
}

/*************************************************************************
* DESCRIPTION
*	从隔断字符隔开的字符串中提取子字符串
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
const char *GetSubStr(char **ppStr, char cc)
{
	const char *tok = "";
	char *s;
	int c;

	s = *ppStr;
	if (s) {
		while (*s == cc)
			s++;
		if (*s) {
			tok = s;
			do {
				c = *s++;
				if (c == cc) {
					s[-1] = '\0';
					*ppStr = s;
					return tok;
				}
			} while (c);
		}
		*ppStr = NULL;
	}
	return tok;
}

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
int CCpyFile(const char *pDstFile, const char *pSrcFile)
{
	char Data[32768];
	FILE *fp, *tp;
	int rt = 1;
	size_t n;

	fp = fopen(pSrcFile, "rb");
	if (!fp)
		return rt;
	tp = fopen(pDstFile, "wb");
	if (!tp)
		goto _FAIL;
	while ((n = fread(Data, 1, sizeof(Data), fp)) > 0) {
		if (fwrite(Data, 1, n, tp) != n)
			goto _FAIL2;
	}
	rt = 0;
_FAIL2:
	fclose(tp);
_FAIL:
	fclose(fp);
	return rt;
}


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
int ModuleInit(void)
{
	TADDR PerCpuOffAddr;

	PerCpuOffAddr = SymName2Addr(pVmSHdl, NULL, "__per_cpu_offset", TGLBVAR);
	if (PerCpuOffAddr)
		DumpMem(PerCpuOffAddr, PerCpuOff, sizeof(PerCpuOff));
	return 1;
}

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
void ModuleDeInit(void)
{
	
}
