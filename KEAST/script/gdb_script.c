/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	gdb_script.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/11/3
* Description: ����gdb�ű�
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "string_res.h"
#include "os.h"
#include <module.h>
#include <mrdump.h>
#include "script_int.h"
#include "gdb_script.h"


#ifdef SST64
#define GDB_NAME "aarch64-linux-android-gdb"
#else
#define GDB_NAME "arm-linux-androideabi-gdb"
#endif
#ifdef _WIN32
#define EXE_POSTFIX ".exe"
#else
#define EXE_POSTFIX
#endif


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���gdb����Ľű�
*
* PARAMETERS
*	fp: �ļ����
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void AddExtraGdbScript(FILE *fp)
{
	static const char *pSstScriptFiles[] =
	{
		"xxx.py",
	};
	char FilePath[MAX_PATH];
	size_t Len, i;

	FilePath[sizeof(FilePath) - 1] = '\0';
	if (!SstOpt.pInstallDir)
		return;
	snprintf(FilePath, sizeof(FilePath) - 1, "%s", SstOpt.pInstallDir);
	Len = strlen(FilePath);
	for (i = 0; i < ARY_CNT(pSstScriptFiles); i++) {
		snprintf(&FilePath[Len], sizeof(FilePath) - 1 - Len, "/%s", pSstScriptFiles[i]);
		if (!access(FilePath, 0))
			LOGC(fp, "source \"%s\"\n", FilePath);
	}
}

/*************************************************************************
* DESCRIPTION
*	����gdb����ķ����ļ�
*
* PARAMETERS
*	pSymHdl: sym���
*	pArg: ����
*
* RETURNS
*	0: ����
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SetGdbNeededSyms(SYM_HDL *pSymHdl, void *pArg)
{
	FILE * const fp = pArg;

	if (pSymHdl == pVmSHdl)
		return 0;
	if (IS_SCRLOADALL())
		SymForceLoad(pSymHdl);
	if (pSymHdl->Flag&SYM_MATCH)
		LOGC(fp, "add-symbol-file %s "TADDRFMT"\n", pSymHdl->pName, pSymHdl->LoadBias);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	����coredump����gdb���ű�
*
* PARAMETERS
*	��
*
* RETURNS
*	NULL: ʧ��
*	����: gdb���ű���
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *GenGdbMainScript(void)
{
#define GDB_MAINSCRIPT UTILS_DIR"/init.gdb"
	FILE *fp;

	/* ���ҿ�ִ���ļ� */
	if ((pVmSHdl->Flag&(SYM_MATCH|SYM_LCLSYM)) != (SYM_MATCH|SYM_LCLSYM)) /* �����ڻ�ƥ�� */
		return NULL;
	SSTINFO(RS_GENSCRIPT, "gdb");
	/* ���ɽű� */
	fp = fopen(GDB_MAINSCRIPT, "wb");
	if (!fp) {
		SSTERR(RS_CREATE, GDB_MAINSCRIPT);
		return NULL;
	}
	LOGC(fp, "# ");
	LOG(fp, RS_GEN_NOTE);
	LOGC(fp, "set osabi GNU/Linux\nset print thread-events off\nset print pretty on\ndir src\n"
			 "file %s\ncore %s\n"
		, SymGetFilePath(pVmSHdl)/* ǰ�����ж�ƥ�䣬�϶��ɹ���!!! */, GetKExpInf()->pCoreFile);
	ForEachSymHdl(SetGdbNeededSyms, fp);
	AddExtraGdbScript(fp);
	fclose(fp);
	return GDB_MAINSCRIPT;
}

#ifdef _WIN32
/*************************************************************************
* DESCRIPTION
*	��ȡGAT��װ·��
*
* PARAMETERS
*	pPath: ����·����buffer
*	Len: buffer����
*
* RETURNS
*	NULL: ʧ��
*	����: GAT��װ·��
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *GetGATPath(char *pPath, DWORD Len)
{
#define PATH_SUFFIX "\\modules\\..\\tools\\findowner.bat \"%1\""
	const char *pGATPath = NULL;
	HKEY HKey;
	long rt;
	
	rt = RegOpenKeyExA(HKEY_CLASSES_ROOT, "MediatekLogView_file\\shell\\open\\command", 0, KEY_ALL_ACCESS, &HKey);
	if (rt == ERROR_SUCCESS) {
		rt = RegQueryValueExA(HKey, NULL, NULL, NULL, (BYTE *)pPath, &Len);
		if (rt == ERROR_SUCCESS) {
			if (Len > sizeof(PATH_SUFFIX) && !memcmp(pPath + Len - sizeof(PATH_SUFFIX), PATH_SUFFIX, sizeof(PATH_SUFFIX))) {
				pPath[Len - sizeof(PATH_SUFFIX)] = '\0';
				pGATPath = pPath;
			} else {
				SSTERR(RS_GAT_PATH);
			}
		}
		RegCloseKey(HKey);
	}
	return pGATPath;
}
#endif

/*************************************************************************
* DESCRIPTION
*	����gdb�����ű�
*
* PARAMETERS
*	pFileName: �����ű���
*	pMainFile: ���ű��ļ���
*	pGatPath: GATĿ¼
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void GenGdbStartupScript(const char *pFileName, const char *pMainFile, const char *pGatPath)
{
	FILE *fp;

	if (!pMainFile)
		return;
	fp = fopen(pFileName, "w");
	if (!fp) {
		SSTERR(RS_CREATE, pFileName);
		return;
	}
#ifdef _WIN32
	LOGC(fp, ":: ");
	LOG(fp, RS_GEN_NOTE);
	LOGC(fp, "@echo off\ncd /d %%~dp0\nif not defined PYTHONHOME (\n");
	if (pGatPath)
		LOGC(fp, "\tset PYTHONHOME=%s\\prebuilt\\python\\bin\n"
			"\tset PYTHONPATH=%s\\prebuilt\\python\\lib\\python2.7;%s\\prebuilt\\python\\lib\\python2.7\\lib-dynload\n"
			, pGatPath, pGatPath, pGatPath);
	else if (!access("C:\\Python27\\python.exe", 0))
		LOGC(fp, "\tset PYTHONHOME=C:\\Python27\n\tset PYTHONPATH=C:\\Python27\\Lib;C:\\Python27\\DLLs\n");
	else
		LOG(fp, RS_NO_PYTHONVAR, "PYTHONHOME");
	LOGC(fp, ")\nset path=%%path%%;%%PYTHONHOME%%;%%PYTHONPATH%%");
	if (pGatPath)
		LOGC(fp, ";%s\\prebuilt\\android-sdk\\bin", pGatPath);
	LOGC(fp, "\nfor /f \"tokens=1,2*\" %%%%i in (\"gdb.exe "GDB_NAME EXE_POSTFIX"\") do (\n");
	LOGC(fp, "\tif exist %%%%~$path:%c (\n\t\t%%%%~$path:%c -q -w -x %s\n\t\texit\n\t)\n", 'i', 'i', pMainFile);
	LOGC(fp, "\tif exist %%%%~$path:%c (\n\t\t%%%%~$path:%c -q -w -x %s\n\t\texit\n\t)\n", 'j', 'j', pMainFile);
	LOGC(fp, ")\necho %s\npause", GetCStr(RS_NO_GDB));
#else
	(void)pGatPath;
	LOGC(fp, "#!/bin/bash\n# ");
	LOG(fp, RS_GEN_NOTE);
	LOGC(fp, "if [\t-z \"$PYTHONPATH\"\t]; then\n\tPythonVer=`python -V 2>&1`\n\t"
		"PythonPath=/usr/lib/python${PythonVer:7:3}\n\tif [\t! -d \"$PythonPath\"\t]; then\n"
		"\t\techo Error: Fail to find python lib path.\n\t\texit\n\tfi\n"
		"\tPYTHONPATH=$PythonPath\nfi\nexport PYTHONPATH\n");
	LOGC(fp, GDB_NAME EXE_POSTFIX" -q -w -x %s", pMainFile);
#endif
	fclose(fp);
#ifndef _WIN32
	chmod(pFileName, 00755);
#endif
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	����coredump����gdb�ű�
*
* PARAMETERS
*	pFileName: �ű���
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
void GenGdbScript(const char *pFileName)
{
	const char *pScriptDir = NULL;
#ifdef _WIN32
	char GatPath[MAX_PATH];

	pScriptDir = GetGATPath(GatPath, sizeof(GatPath));
#endif
	GenGdbStartupScript(pFileName, GenGdbMainScript(), pScriptDir);
}
