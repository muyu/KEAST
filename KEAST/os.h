/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	os.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/4/1
* Description: os相关函数定义
**********************************************************************************/
#ifndef _OS_H
#define _OS_H

#ifdef _WIN32
#include <windows.h>
#include <direct.h> /* 引入_chdir(), _mkdir() */
#include <io.h> /* 引入_findfirst(), _findnext(), _findclose(), access() */
#define snprintf	_snprintf
#else
#include <unistd.h>
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#define MAX_PATH	PATH_MAX
#endif
#include <ttypes.h>

#if defined(__MINGW32__)
#include <stdint.h>   /*For define uintptr_t*/
#endif

typedef const struct _FP_HDL
{
	const char *pPath;
	TSIZE FileSz;
} FP_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
#ifdef _DEBUG
#include <stdlib.h>
#include <crtdbg.h> /* 必须在stdlib.h之后，用于内存调试!!! */

#define Malloc		malloc
#define Calloc(Sz)	calloc(1, Sz)
#define Realloc		realloc
#define Free		free
#else
/*************************************************************************
* DESCRIPTION
*	申请堆内存
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: 内存指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Malloc(size_t Size);

/*************************************************************************
* DESCRIPTION
*	申请堆内存并格式化为0
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: 内存指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Calloc(size_t Size);

/*************************************************************************
* DESCRIPTION
*	调整堆内存
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: 新的内存指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Realloc(void *p, size_t NewSz);

/*************************************************************************
* DESCRIPTION
*	释放堆内存
*
* PARAMETERS
*	pMem: 内存指针
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void Free(void *pMem);
#endif

/*************************************************************************
* DESCRIPTION
*	创建目录
*
* PARAMETERS
*	pDir: 目录
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void MkFileDir(const char *pDir);

/*************************************************************************
* DESCRIPTION
*	获取raw kernel log文件名
*
* PARAMETERS
*	pFileName: 返回raw kernel log文件名, 大小必须>=MAX_PATH
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int FindRawKernLog(char *pFileName);

/*************************************************************************
* DESCRIPTION
*	注册映射失败时尝试释放的函数(空间回收)
*
* PARAMETERS
*	pFunc: 函数指针
*	Size: 预期释放的大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void RegTryUnmapFunc(int (*pFunc)(TSIZE Size));

/*************************************************************************
* DESCRIPTION
*	获取0内存, 可以被修改
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *GetZeroBuf(TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	获取0只读内存
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *GetZeroRoBuf(TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	设置0内存为只读
*
* PARAMETERS
*	pBuf: buffer
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void ProtectZeroBuf(const void *pBuf, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放0内存
*
* PARAMETERS
*	pBuf: buffer
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutZeroBuf(const void *pBuf, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	映射文件到内存
*
* PARAMETERS
*	pFpHdl: fp句柄
*	Off: 偏移量(字节)
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *FpMapRo(FP_HDL *pFpHdl, TSIZE Off, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	释放文件映射内存
*
* PARAMETERS
*	pFpHdl: fp句柄
*	pBuf: buffer
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void FpUnmap(FP_HDL *pFpHdl, const void *pBuf, TSIZE Size);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取指定文件的句柄
*
* PARAMETERS
*	pFile: 文件路径或文件名
*	FromSymDir: 0: pFile是路径, 1: 到SstOpt.pSymDir目录搜索pFile
*
* RETURNS
*	NULL: 失败
*	其他: fp句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
FP_HDL *GetFpHdl(const char *pFile, int FromSymDir);

/*************************************************************************
* DESCRIPTION
*	复制fp句柄
*
* PARAMETERS
*	pFpHdl: fp句柄
*
* RETURNS
*	新的fp句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
FP_HDL *DupFpHdl(FP_HDL *pFpHdl);

/*************************************************************************
* DESCRIPTION
*	释放文件句柄
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
void PutFpHdl(FP_HDL *pFpHdl);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化os模块
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
void OsInit(void);

/*************************************************************************
* DESCRIPTION
*	注销os模块
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
void OsDeInit(void);

#endif
