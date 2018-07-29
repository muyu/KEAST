/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	script_gen.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/5/5
* Description: 生成cmm/gdb脚本
**********************************************************************************/
#include <stdio.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include "cmm_script.h"
#include "script_int.h"
#include "gdb_script.h"
#include "script_gen.h"


#ifdef _WIN32
#define GDB_FILENAME "gdb.bat"
#else
#define GDB_FILENAME "gdb.sh"
#endif
#define CMM_FILENAME "debug.cmm"


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	启动脚本程序
*
* PARAMETERS
*	Type: 0: trace32, 1: gdb
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int StartScriptProgram(int Type)
{
#ifdef _WIN32
	const char *pScriptName;
	char Buf[MAX_PATH];
	int rt, i;

	pScriptName = (Type ? GDB_FILENAME : CMM_FILENAME);
	if (access(pScriptName, 0)) {
		SSTERR(RS_FILE_NOT_EXIST, pScriptName);
		return 1;
	}
	if (!Type && getcwd(Buf, sizeof(Buf))) { /* 不支持有空格的路径 */
		for (i = 0; i < ARY_CNT(Buf) && Buf[i]; i++) {
			if (isspace(Buf[i])) {
				SSTERR(RS_PATH_EXIST_SPACE);
				return 1;
			}
		}
	}
	rt = (int)ShellExecute(NULL, NULL, pScriptName, NULL, NULL, SW_SHOWNORMAL);
	if (rt > 32 || Type)
		return 0;
	SSTERR(RS_NO_T32, "\\\\script\\script\\WirelessTools\\ToolRelease\\OfflineDebugSuite\\SP_OfflineDebugSuite\\t32sim.rar");
#else
	SSTWARN(RS_NO_FEATURE);
#endif
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	根据coredump生成脚本(cmm脚本、gdb脚本)
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
void GenScript(void)
{
	if (SstOpt.Flag&(SST_GENCMM_BIT|SST_GENGDB_BIT)) {
		MkFileDir(UTILS_DIR);
		MkFileDir("src");
	}
	if (IS_GENCMM())
		GenCmmScript(CMM_FILENAME);
	if (IS_GENGDB())
		GenGdbScript(GDB_FILENAME);
}
