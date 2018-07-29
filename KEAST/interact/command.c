/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	command.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/4/11
* Description: ������
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <conio.h>
#else
#include <termios.h>
#endif
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include <script/script_gen.h>
#include "command.h"


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���1��vma��Ϣ
*
* PARAMETERS
*	SAddr/EAddr: [��ʼ��ַ, ������ַ)
*	Flag: ��־
*	pArg: ����
*
* RETURNS
*	0: ����
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowVmaInf(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg)
{
	static const char *FlagStrs[] = {"   ", "  X", " W ", " WX", "R  ", "R X", "RW ", "RWX"};
	FILE * const fp = pArg;
	VMA_HDL *pVmaHdl;

	LOGC(fp, TADDRFMT"-"TADDRFMT" %s ", SAddr, EAddr, FlagStrs[(Flag&VMA_MMASK) >> VMA_MSHIFT]);
	switch (Flag&VMA_TMASK) {
	case VMA_ELF: LOGC(fp, " ELF  "); break;
	case VMA_FILE: LOGC(fp, " FILE "); break;
	case VMA_HEAP: LOGC(fp, " HEAP "); break;
	case VMA_STACK: LOGC(fp, " STACK"); break;
	case VMA_IGNORE: LOGC(fp, "IGNORE"); break;
	default: LOGC(fp, "      "); break;
	}
	pVmaHdl = GetVmaHdl(SAddr);
	if (pVmaHdl->pName)
		LOGC(fp, " %s", pVmaHdl->pName);
	PutVmaHdl(pVmaHdl);
	LOGC(fp, "\n");
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	�������vma��Ϣ���ļ�
*
* PARAMETERS
*	��
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void GenMaps(void)
{
#define MAP_FILE "SYS_MAPS.txt"
	FILE *fp;

	fp = fopen(MAP_FILE, "w");
	if (!fp) {
		SSTERR(RS_CREATE, MAP_FILE);
		return;
	}
	ForEachVmaAll(ShowVmaInf, fp);
	fclose(fp);
	SSTINFO(RS_FILE_RDY, MAP_FILE);
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	������ѭ��
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
void CommandLoop(void)
{
#if defined(_DEBUG)
	GenMaps();
	return;
#else
#define CMD_SZ  64
	char Str[CMD_SZ + 1], *p;
	const char *pCmd;
	int rt;

	if (SstOpt.Flag&SST_HPAUSE_BIT)
		return;
	SSTINFO(RS_COMMAND);
#ifdef _WIN32
	rt = getch();
#else
{
	struct termios oldt, newt;
	
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON|ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	rt = getchar();
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}
#endif
	LOGC(stdout, "\n");
	switch (rt) {
	case 't': case 'T': rt = StartScriptProgram(0); goto _OUT;
	case 'g': case 'G': rt = StartScriptProgram(1); goto _OUT;
	case '\r': case '\n': break;
	default: rt = 0; goto _OUT;
	}
	SSTINFO(RS_CMDHELP);
	while (1) {
		LOGC(stdout, ">");
		if (!fgets(Str, sizeof(Str), stdin) || Str[0] == '\n')
			continue;
		Str[strlen(Str) - 1] = '\0'; /* ȥ��\n */
		p = Str;
		pCmd = GetSubStr(&p, ' ');
		if (!strcmp(pCmd, "q")) {
			rt = 0;
			break;
		}
		if (!strcmp(pCmd, "m")) {
			GenMaps();
			continue;
		}
		SSTERR(RS_UNKNOWN_CMD);
	}
_OUT:
	if (!rt)
		SstOpt.Flag |= SST_HPAUSE_BIT; /* ����main.c��pauseһ��! */
#endif
}
