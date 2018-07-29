/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	feedback.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/11/10
* Description: 反馈信息模块
**********************************************************************************/
#include <stdio.h>
#include <time.h>
#include "string_res.h"
#include <module.h>
#include "os.h"
#include <mrdump.h>
#include <callstack.h>
#include <db/db.h>
#include "feedback.h"


#define FB_VERSION			  "2"


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	记录进程信息
*
* PARAMETERS
*	fp: 文件句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ProcRecord(FILE *fp)
{
	KEXP_INF *pKExpInf = GetKExpInf();
	CS_HDL *pCsHdl = NULL;
	CS_FRAME *pCsF = NULL;
	CORE_CNTX *pCpuCntx;

	pCpuCntx = GetCoreCntx(pKExpInf->CpuId);
	if (pCpuCntx)
		pCsF = pCsHdl = GetNativeCsHdl(pCpuCntx->RegAry, 0);
	/* 记录异常线程调用栈 */
	if (pCsHdl) {
		LOGC(fp, "ExpCallStack:");
		do {
			if (pCsF->pFName && pCsF->pFName != pUndFuncName)
				LOGC(fp, "|%s@%s", pCsF->pMName, pCsF->pFName);
			else
				LOGC(fp, "|%s@%"TLONGFMT"X", pCsF->pMName, pCsF->FAddr);
		} while ((pCsF = pCsF->pNext) != NULL);
		LOGC(fp, "\n");
		PutCsHdl(pCsHdl);
	}
}

/*************************************************************************
* DESCRIPTION
*	记录异常信息
*
* PARAMETERS
*	fp: 文件句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ExcpRecord(FILE *fp)
{
	KEXP_INF * const pKExpInf = GetKExpInf();
	char Buf[MAX_PATH];
	struct tm *pLTime;
	time_t CurTime;
	size_t Idx;

#ifdef SST64
	LOGC(fp, "ExpType:KE64");
#else
	LOGC(fp, "ExpType:KE");
#endif
	LOGC(fp, ",%d,%d\nExpTime:%d.%06d\n", pKExpInf->eType, pKExpInf->mType, pKExpInf->Sec, pKExpInf->USec);
	LOGC(fp, "SSTver:"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"\nLaunch:%d,%d\n", SstOpt.Flag, 0);
	if (getcwd(Buf, sizeof(Buf)))
		LOGC(fp, "ExpPath:%s\n", Buf);
	CurTime = time(NULL);
	pLTime = localtime(&CurTime);
	LOGC(fp, "Time:%d-%d-%d_%d:%d:%d\n"
		, pLTime->tm_year + 1900, pLTime->tm_mon + 1, pLTime->tm_mday, pLTime->tm_hour, pLTime->tm_min, pLTime->tm_sec);
	for (Idx = 0; Idx < ARY_CNT(SstErr) && SstErr[Idx]; Idx++) {
		LOGC(fp, "SstErr%d:%s"/* 无需\n, 因为字符串结尾有\n */, Idx, GetCStr(SstErr[Idx]));
	}
}

/*************************************************************************
* DESCRIPTION
*	打开fb.txt
*
* PARAMETERS
*	无
*
* RETURNS
*	NULL: 失败
*	其他: 文件句柄
*
* LOCAL AFFECTED
*
*************************************************************************/
static FILE *OpenFbFile(void)
{
	char FilePath[MAX_PATH];

	if (!IS_FEEDBACK() || !SstOpt.pInstallDir)
		return NULL;
	FilePath[sizeof(FilePath) - 1] = '\0';
	snprintf(FilePath, sizeof(FilePath) - 1, "%s/fb.txt", SstOpt.pInstallDir);
	return fopen(FilePath, "a+");
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	反馈信息到文件
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
void FeedBack(void)
{
	FILE *fp;

	fp = OpenFbFile();
	if (!fp)
		return;
	LOGC(fp, "==v"FB_VERSION"==\n");
	ExcpRecord(fp);
	ProcRecord(fp);
	fclose(fp);
}
