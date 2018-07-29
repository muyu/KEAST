/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	main.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/16
* Description: 主启动文件
**********************************************************************************/
#include <string.h>
#include <stdlib.h> /* 引入atoi() */
#ifdef _WIN32
//#include <crtdbg.h>
#include <conio.h> /* 引入_getch() */
#endif
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include <exception.h>
#include <interact/feedback.h>
#include <drivers/android/logger_tracker.h>




static RS_UNIT RsAryChe[] =
{
#define _NL(Enum, str1, str2) {str1},
	RS_MAP
#undef _NL
};

static RS_UNIT RsAryEng[] =
{
#define _NL(Enum, str1, str2) {str2},
	RS_MAP
#undef _NL
};

RS_UNIT *pRsAry = RsAryChe;//RsAryEng;
int SstErr[16], SstErrIdx; /* 错误码 */
SST_OPT SstOpt = { NULL, DEF_SYMDIR, NULL, sizeof(DEF_SYMDIR) - 1, 0 };


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	检查字符串资源
*
* PARAMETERS
*	无
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void CheckRS(void)
{
#ifdef _DEBUG
	unsigned i, j;
	char c1, c2;

	for (i = 0; i < ARY_CNT(RsAryChe); i++) {
		for (j = i + 1; j < ARY_CNT(RsAryChe); j++) {
			if (RsAryChe[i].pStr == RsAryChe[j].pStr)
				DBGERR("重复%d: %s\n", i, RsAryChe[i].pStr);
		}
	}
	for (i = 0; i < ARY_CNT(RsAryChe); i++) {
		c1 = RsAryChe[i].pStr[strlen(RsAryChe[i].pStr) - 1];
		c2 = RsAryEng[i].pStr[strlen(RsAryEng[i].pStr) - 1];
		if ((c1 == '\n' && c2 != '\n') || (c1 != '\n' && c2 == '\n'))
			DBGERR("无回车%d: %s\n%s\n", i, RsAryChe[i].pStr, RsAryEng[i].pStr);
	}
#endif
}

/*************************************************************************
* DESCRIPTION
*	设置为繁体语言
*
* PARAMETERS
*	无
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void SetLangTw(void)
{
	pRsAry = RsAryEng;
}

/*************************************************************************
* DESCRIPTION
*	分析参数
*
* PARAMETERS
*	argc: 参数个数
*	argv: 参数列表
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ParseArg(int argc, const char *argv[])
{
	unsigned Flag;
	int i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-t")) {
			SstOpt.Flag |= SST_ALLTHREADS_BIT;
			continue;
		}
		if (!strcmp(argv[i], "-cmm")) {
			SstOpt.Flag |= SST_GENCMM_BIT;
			continue;
		}
		if (!strcmp(argv[i], "-gdb")) {
			SstOpt.Flag |= SST_GENGDB_BIT;
			continue;
		}
		if (!strcmp(argv[i], "-lang")) {
			if (++i >= argc) { SSTERR(RS_NEED_ARG, "-lang", 1); return 1; }
			if (!strcmp(argv[i], "eng")) {
				SstOpt.Flag = (SstOpt.Flag&~SST_LANG_MASK)|SST_ENG_LANG;
				pRsAry = RsAryEng;
			} else if (!strcmp(argv[i], "tw")) {
				SstOpt.Flag = (SstOpt.Flag&~SST_LANG_MASK)|SST_TW_LANG;
				SetLangTw();
			}
			continue;
		}
		if (!strcmp(argv[i], "-ext")) {
			if (++i >= argc) { SSTERR(RS_NEED_ARG, "-ext", 1); return 1; }
			Flag = atoi(argv[i]);
			SstOpt.Flag = (Flag&SST_LANG_MASK ? (SstOpt.Flag&~SST_LANG_MASK)|Flag : SstOpt.Flag|Flag);
			if (IS_ENGLISH())
				pRsAry = RsAryEng;
			else if (IS_TRADITIONAL())
				SetLangTw();
			continue;
		}
		if (!strcmp(argv[i], "-e")) {
			if (++i >= argc) { SSTERR(RS_NEED_ARG, "-e", 1); return 1; }
			SstOpt.pCrId = argv[i];
			continue;
		}
		if (!strcmp(argv[i], "-s")) {
			if (++i >= argc) { SSTERR(RS_NEED_ARG, "-s", 1); return 1; }
			SstOpt.pSymDir = argv[i];
			continue;
		}
		if (!strcmp(argv[i], "-c")) {
			if (++i >= argc) { SSTERR(RS_NEED_ARG, "-c", 1); return 1; }
			continue;
		}
		if (!strcmp(argv[i], "-l")) {
			if (++i >= argc) { SSTERR(RS_NEED_ARG, "-l", 1); return 1; }
			SstOpt.pInstallDir = argv[i];
			continue;
		}
		if (!strcmp(argv[i], "-h")) { SSTINFO(RS_USAGE, argv[0]); return 1; }
		if (argv[i][0] != '-') {
			if (!chdir(argv[i]))
				continue;
			SSTERR(RS_EDIR, argv[i]);
			return 1;
		}
		SSTERR(RS_UNKNOWN_ARG, argv[i]);
		return 1;
	}
	SstOpt.SymDirLen = strlen(SstOpt.pSymDir);
	return 0;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	主函数
*
* PARAMETERS
*	argc: 参数个数
*	argv: 参数列表
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int main(int argc, const char *argv[])
{
	const char *pOutFile;
	FILE *fp;
	int rt;

	LOGC(stdout, "== "SST_NAME" v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)" ==\n");
	//CheckRS();
	OsInit();
	if (ParseArg(argc, argv))
		goto _OUT;
	ConvertLog();
	pOutFile = GetCStr(RS_REPORT);
#ifdef _DEBUG
	pOutFile = (pOutFile[0] == 'A' ? "Analysis report1.txt" : "分析报告1.txt");
#endif
	fp = fopen(pOutFile, "w+");
	if (!fp) {
		SSTERR(RS_CREATE, pOutFile);
		goto _OUT;
	}
	LOG(fp, RS_EX_HEAD);
	rt = PrepareMrdump(fp);
	if (!rt) { 
		ExceptionTrack(fp, pOutFile);  //分析kernel slub task
		DestoryMrdump();
	}
	fclose(fp);
	if (rt)
		remove(pOutFile);
_OUT:
	OsDeInit();
#ifdef _WIN32
#ifdef _CRTDBG_MAP_ALLOC /* memory leak check just for _WIN32 debug version */
	if (_CrtDumpMemoryLeaks())
		DBGERR("存在内存泄露\n");
#endif
	if (!(SstOpt.Flag&SST_HPAUSE_BIT)) {
		SSTINFO(RS_PAUSE);
		_getch();
	}
#endif
	return SstErr[0];
}
