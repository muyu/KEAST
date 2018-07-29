/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	reference.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/4/8
* Description: �ο���Ϣ��ʾ
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "string_res.h"
#include "os.h"
#include "reference.h"


typedef struct _LIB_REFMAP
{
	unsigned LibOff;
	unsigned OwnerOff;
} LIB_REFMAP;

static const char *pRefStrs;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�Ƚ������ַ���
*
* PARAMETERS
*	e1, e2: �ַ���
*
* RETURNS
*	1: >
*	0: ==
*	-1: <
*
* LOCAL AFFECTED
*
*************************************************************************/
static int RefLibNameCmp(const void *e1, const void *e2)
{
	return strcmp(e1, pRefStrs + ((const LIB_REFMAP *)e2)->LibOff);
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʾ�ο���Ϣ
*
* PARAMETERS
*	fp: �ļ����
*	pLibName: �����ļ���, ����ΪNULL
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispRefInf(FILE *fp, const char *pLibName)
{
	const struct
	{
#define REFDB_MAGIC  0xDB6E6179
		unsigned Magic;
		unsigned TotSz, Version;
		unsigned CoworkOff, StrsOff, MapCnt;
	} *pHead;
	LIB_REFMAP *pRefMap = NULL;
	char FilePath[MAX_PATH];
	FP_HDL *pFpHdl;
	
	if (!IS_REFERENCE() || !SstOpt.pInstallDir)
		return;
	FilePath[sizeof(FilePath) - 1] = '\0';
	snprintf(FilePath, sizeof(FilePath) - 1, "%s/Ref.db", SstOpt.pInstallDir);
	pFpHdl = GetFpHdl(FilePath, 0);
	if (!pFpHdl)
		return;
	pHead = FpMapRo(pFpHdl, 0, pFpHdl->FileSz);
	if (!pHead || pHead->Magic != REFDB_MAGIC || pHead->TotSz != pFpHdl->FileSz) {
		SSTERR(RS_FILE_DAMAGED, FilePath);
		goto _OUT;
	}
	if (pHead->Version != 1) /* ��֧�ְ汾1 */
		goto _OUT;
	pRefStrs = (char *)pHead + pHead->StrsOff;
	if (pLibName)
		pRefMap = bsearch(pLibName, pHead + 1, pHead->MapCnt, sizeof(LIB_REFMAP), RefLibNameCmp);
	if (pRefMap)
		LOGC(fp, "Owner   : %s, Coworker: %s\n", pRefStrs + pRefMap->OwnerOff, pRefStrs + pHead->CoworkOff);
	else
		LOGC(fp, "Owner   : %s\n", pRefStrs + pHead->CoworkOff);
_OUT:
	if (pHead)
		FpUnmap(pFpHdl, pHead, pFpHdl->FileSz);
	PutFpHdl(pFpHdl);
}
