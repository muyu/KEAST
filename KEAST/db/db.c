/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	db.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/5/19
* Description: db模块解析
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "os.h"
#include <ac.h>
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include "rb_reason.h"
#include "db.h"


static BUILD_VAR _BuildVar;
static unsigned PlatformId;
static char MtkVers[64] = "??";


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	扫描文件，读取信息
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
static void ScanInternalFile(void)
{
	char data[1024], *p;
	const char *s;
	FILE *fp;

	fp = fopen("ZZ_INTERNAL", "r");
	if (!fp)
		return;
	if (!fgets(data, sizeof(data), fp))
		goto _OUT;
	p = data;
	GetSubStr(&p, ','); /* exp name: Kernel (KE) */
	GetSubStr(&p, ','); /* pid */
	GetSubStr(&p, ','); /* tid */
	GetSubStr(&p, ','); /* 99 */
	GetSubStr(&p, ','); /* default coredump dir: /data/core/ */
	GetSubStr(&p, ','); /* exp level */
	if (p && *p == ',')
		p++; /* 无值 */
	else
		GetSubStr(&p, ','); /* type: KERNAL-PANIC */
	if (p && *p == ',') {
		p++; /* 无值 */
	} else {
		s = GetSubStr(&p, ','); /* process: HWT */
		if (!strncmp(s, "KE at ", sizeof("KE at ") - 1))
			UpdateExcpTime(0, 0, EXP_DIE);
		else if (!strcmp(s, "HWT"))
			UpdateExcpTime(0, 0, EXP_HWT);
		else if (!strncmp(s, "HW_REBOOT", sizeof("HW_REBOOT") - 1)) /* 可能存在HW_REBOOT(SPM) */
			UpdateExcpTime(0, 0, EXP_HW_REBOOT);
		else if (!strcmp(s, "ATF_CRASH"))
			UpdateExcpTime(0, 0, EXP_ATF_CRASH);
		else if (!strcmp(s, "THERMAL_REBOOT"))
			UpdateExcpTime(0, 0, EXP_THERMAL_RB);
	}
	UpdateExcpUtc(GetSubStr(&p, ',')); /* exp time */
_OUT:
	fclose(fp);
}

/*************************************************************************
* DESCRIPTION
*	扫描文件，读取信息
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
static void ScanExpMainFile(void)
{
	char data[1024], *p;
	const char *s;
	FILE *fp;

	fp = fopen("__exp_main.txt", "r");
	if (!fp)
		return;
	while (fgets(data, sizeof(data), fp)) {
		if (strncmp(data, "Build Info: '", sizeof("Build Info: '") - 1))
			continue;
		p = &data[sizeof("Build Info: '") - 1];
		s = GetSubStr(&p, ':'); /* version */
		snprintf(MtkVers, sizeof(MtkVers) - 1, "%s", s);
		GetSubStr(&p, ':'); /* version2 */
		s = GetSubStr(&p, ':'); /* platform */
		if (!strncmp(s, "mt", 2) || !strncmp(s, "MT", 2))
			sscanf(s + 2, "%d", &PlatformId);
		GetSubStr(&p, ':');
		GetSubStr(&p, ':');
		s = GetSubStr(&p, ':'); /* build */
		if (!strncmp(s, "eng/", sizeof("eng/") - 1))
			_BuildVar = BUILD_ENG;
		else if (!strncmp(s, "user/", sizeof("user/") - 1))
			_BuildVar = BUILD_USER;
		else if (!strncmp(s, "userdebug/", sizeof("userdebug/") - 1))
			_BuildVar = BUILD_USERDBG;
		break;
	}
	fclose(fp);	
}

/*************************************************************************
* DESCRIPTION
*	扫描指定文件，提取异常时间等信息
*
* PARAMETERS
*	pFileName: 扫描文件名
*	pAcHdl: ac句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ParseKernLog(const char *pFileName, AC_HDL *pAcHdl)
{
#define KERNLOG_PAT_MAP \
	_NL(KERNLOG_SEGV,		"Unable to handle kernel ") \
	_NL(KERNLOG_BADMODE,	"Bad mode in Synchronous Abort handler") \
	_NL(KERNLOG_OOPS,		"Internal error: ") \
	_NL(KERNLOG_PANIC,		"Kernel panic - not syncing:") \
	_NL(KERNLOG_WDK1,		"kick=0x") \
	_NL(KERNLOG_WDK2,		"Qwdt at [") \
	_NL(KERNLOG_NESTED,		"Nested panic") \
	
	typedef enum _KERNLOG_RS_ID
	{
		KERNLOG_START = 0,
#define _NL(Enum, Str) Enum,
		KERNLOG_PAT_MAP
#undef _NL
	} KERNLOG_RS_ID;
	static const EXP_TYPE eTbl[] = {EXP_SEGV, EXP_DIE, EXP_DIE, EXP_PANIC};
	char data[1024], ProcName[TASK_COMM_LEN + 1];
	TUINT Val1, Val2, CpuId, Pid;
	unsigned eType;
	const char *p;
	FILE *fp;
	int rt;
	
	fp = fopen(pFileName, "rb");
	if (!fp)
		return;
	while (fgets(data, sizeof(data), fp)) {
		p = data;
		eType = AcTrieSearch(pAcHdl, &p);
		switch (eType) {
		case KERNLOG_WDK1:
			rt = sscanf(p, "%x,check=%x", &Val1, &Val2);
			if (rt == 2)
				UpdateHwtInf(Val1, Val2);
			break;
		case KERNLOG_SEGV:
		case KERNLOG_BADMODE:
		case KERNLOG_OOPS:
		case KERNLOG_PANIC:
			p = strchr(data, '[');
			if (!p)
				break;
			ProcName[0] = '\0';
			rt = sscanf(p, "[%d.%d%*[^(](%d)[%d:%"MACRO2STR(TASK_COMM_LEN)"[^]]", &Val1, &Val2, &CpuId, &Pid, ProcName);
			if (rt >= 2)
				UpdateExcpTime(Val1, Val2, eTbl[eType - 1]);
			if (rt == 5)
				UpdateCoreCntx(CpuId, NULL, 0, 0, Pid, ProcName);
			break;
		case KERNLOG_WDK2:
			rt = sscanf(p, "%d.%d", &Val1, &Val2);
			if (rt == 2)
				UpdateExcpTime(Val1, Val2, EXP_HWT);
			break;
		case KERNLOG_NESTED:
			if (!(GetKExpInf()->Sec|GetKExpInf()->USec))
				UpdateExcpTime(1, 1, EXP_DIE);
			break;
		default:;
		}
	}
	fclose(fp);
}

/*************************************************************************
* DESCRIPTION
*	扫描kernel log，提取异常时间等信息
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
static void ScanKernLog(void)
{
	static const char * const KernlogPats[] =
	{
#define _NL(Enum, Str) Str,
		KERNLOG_PAT_MAP
#undef _NL
		NULL,
	};
	AC_HDL *pAcHdl;
	
	pAcHdl = GetAcTrie(KernlogPats);
	if (!pAcHdl) {
		SSTERR(RS_OOM);
		return;
	}
	ParseKernLog("SYS_LAST_KMSG", pAcHdl);
	ParseKernLog("SYS_KERNEL_LOG", pAcHdl);
	PutAcTrie(pAcHdl);
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取平台号
*
* PARAMETERS
*	无
*
* RETURNS
*	返回平台号
*
* GLOBAL AFFECTED
*
*************************************************************************/
unsigned GetPlatform(void)
{
	return PlatformId;
}

/*************************************************************************
* DESCRIPTION
*	获取mediatek android版本号
*
* PARAMETERS
*	无
*
* RETURNS
*	返回版本号字符串
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *MtkVer(void)
{
	return MtkVers;
}

/*************************************************************************
* DESCRIPTION
*	获取编译类型
*
* PARAMETERS
*	无
*
* RETURNS
*	编译类型BUILD_VAR
*
* GLOBAL AFFECTED
*
*************************************************************************/
BUILD_VAR BuildVar(void)
{
	return _BuildVar;
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化db模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: db模块关闭
*	1: db模块可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbInit(void)
{
	ScanInternalFile();
	ScanExpMainFile();
	ScanKernLog();
	if (!access("SYS_ATF_CRASH", 0))
		UpdateExcpTime(0, 0, EXP_ATF_CRASH);
	RbReasonInit();
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	注销db模块
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
void DbDeInit(void)
{
	RbReasonDeInit();
}
