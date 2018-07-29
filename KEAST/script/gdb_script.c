/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	gdb_script.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/11/3
* Description: 生成gdb脚本
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
*	添加gdb额外的脚本
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
*	设置gdb所需的符号文件
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
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
*	根据coredump生成gdb主脚本
*
* PARAMETERS
*	无
*
* RETURNS
*	NULL: 失败
*	其他: gdb主脚本名
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *GenGdbMainScript(void)
{
#define GDB_MAINSCRIPT UTILS_DIR"/init.gdb"
	FILE *fp;

	/* 查找可执行文件 */
	if ((pVmSHdl->Flag&(SYM_MATCH|SYM_LCLSYM)) != (SYM_MATCH|SYM_LCLSYM)) /* 不存在或不匹配 */
		return NULL;
	SSTINFO(RS_GENSCRIPT, "gdb");
	/* 生成脚本 */
	fp = fopen(GDB_MAINSCRIPT, "wb");
	if (!fp) {
		SSTERR(RS_CREATE, GDB_MAINSCRIPT);
		return NULL;
	}
	LOGC(fp, "# ");
	LOG(fp, RS_GEN_NOTE);
	LOGC(fp, "set osabi GNU/Linux\nset print thread-events off\nset print pretty on\ndir src\n"
			 "file %s\ncore %s\n"
		, SymGetFilePath(pVmSHdl)/* 前面已判断匹配，肯定成功的!!! */, GetKExpInf()->pCoreFile);
	ForEachSymHdl(SetGdbNeededSyms, fp);
	AddExtraGdbScript(fp);
	fclose(fp);
	return GDB_MAINSCRIPT;
}

#ifdef _WIN32
/*************************************************************************
* DESCRIPTION
*	获取GAT安装路径
*
* PARAMETERS
*	pPath: 容纳路径的buffer
*	Len: buffer长度
*
* RETURNS
*	NULL: 失败
*	其他: GAT安装路径
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
*	生成gdb启动脚本
*
* PARAMETERS
*	pFileName: 启动脚本名
*	pMainFile: 主脚本文件名
*	pGatPath: GAT目录
*
* RETURNS
*	无
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
*	根据coredump生成gdb脚本
*
* PARAMETERS
*	pFileName: 脚本名
*
* RETURNS
*	无
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
