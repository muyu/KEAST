/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	os.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/4/1
* Description: os��غ�������
**********************************************************************************/
#ifndef _OS_H
#define _OS_H

#ifdef _WIN32
#include <windows.h>
#include <direct.h> /* ����_chdir(), _mkdir() */
#include <io.h> /* ����_findfirst(), _findnext(), _findclose(), access() */
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
#include <crtdbg.h> /* ������stdlib.h֮�������ڴ����!!! */

#define Malloc		malloc
#define Calloc(Sz)	calloc(1, Sz)
#define Realloc		realloc
#define Free		free
#else
/*************************************************************************
* DESCRIPTION
*	������ڴ�
*
* PARAMETERS
*	Size: ��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: �ڴ�ָ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Malloc(size_t Size);

/*************************************************************************
* DESCRIPTION
*	������ڴ沢��ʽ��Ϊ0
*
* PARAMETERS
*	Size: ��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: �ڴ�ָ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Calloc(size_t Size);

/*************************************************************************
* DESCRIPTION
*	�������ڴ�
*
* PARAMETERS
*	Size: ��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: �µ��ڴ�ָ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Realloc(void *p, size_t NewSz);

/*************************************************************************
* DESCRIPTION
*	�ͷŶ��ڴ�
*
* PARAMETERS
*	pMem: �ڴ�ָ��
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void Free(void *pMem);
#endif

/*************************************************************************
* DESCRIPTION
*	����Ŀ¼
*
* PARAMETERS
*	pDir: Ŀ¼
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void MkFileDir(const char *pDir);

/*************************************************************************
* DESCRIPTION
*	��ȡraw kernel log�ļ���
*
* PARAMETERS
*	pFileName: ����raw kernel log�ļ���, ��С����>=MAX_PATH
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int FindRawKernLog(char *pFileName);

/*************************************************************************
* DESCRIPTION
*	ע��ӳ��ʧ��ʱ�����ͷŵĺ���(�ռ����)
*
* PARAMETERS
*	pFunc: ����ָ��
*	Size: Ԥ���ͷŵĴ�С(�ֽ�)
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void RegTryUnmapFunc(int (*pFunc)(TSIZE Size));

/*************************************************************************
* DESCRIPTION
*	��ȡ0�ڴ�, ���Ա��޸�
*
* PARAMETERS
*	Size: ��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *GetZeroBuf(TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	��ȡ0ֻ���ڴ�
*
* PARAMETERS
*	Size: ��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *GetZeroRoBuf(TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	����0�ڴ�Ϊֻ��
*
* PARAMETERS
*	pBuf: buffer
*	Size: ��С(�ֽ�)
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void ProtectZeroBuf(const void *pBuf, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	�ͷ�0�ڴ�
*
* PARAMETERS
*	pBuf: buffer
*	Size: ��С(�ֽ�)
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutZeroBuf(const void *pBuf, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	ӳ���ļ����ڴ�
*
* PARAMETERS
*	pFpHdl: fp���
*	Off: ƫ����(�ֽ�)
*	Size: ��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *FpMapRo(FP_HDL *pFpHdl, TSIZE Off, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	�ͷ��ļ�ӳ���ڴ�
*
* PARAMETERS
*	pFpHdl: fp���
*	pBuf: buffer
*	Size: ��С(�ֽ�)
*
* RETURNS
*	��
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
*	��ȡָ���ļ��ľ��
*
* PARAMETERS
*	pFile: �ļ�·�����ļ���
*	FromSymDir: 0: pFile��·��, 1: ��SstOpt.pSymDirĿ¼����pFile
*
* RETURNS
*	NULL: ʧ��
*	����: fp���
*
* GLOBAL AFFECTED
*
*************************************************************************/
FP_HDL *GetFpHdl(const char *pFile, int FromSymDir);

/*************************************************************************
* DESCRIPTION
*	����fp���
*
* PARAMETERS
*	pFpHdl: fp���
*
* RETURNS
*	�µ�fp���
*
* GLOBAL AFFECTED
*
*************************************************************************/
FP_HDL *DupFpHdl(FP_HDL *pFpHdl);

/*************************************************************************
* DESCRIPTION
*	�ͷ��ļ����
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
void PutFpHdl(FP_HDL *pFpHdl);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��osģ��
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
void OsInit(void);

/*************************************************************************
* DESCRIPTION
*	ע��osģ��
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
void OsDeInit(void);

#endif
