/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	script_gen.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/5/5
* Description: ����cmm/gdb�ű�
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
*	�����ű�����
*
* PARAMETERS
*	Type: 0: trace32, 1: gdb
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
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
	if (!Type && getcwd(Buf, sizeof(Buf))) { /* ��֧���пո��·�� */
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
*	����coredump���ɽű�(cmm�ű���gdb�ű�)
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
