/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	logger.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: log提取
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include "logger.h"

#pragma pack(4)
typedef const struct _KLOGV2
{
	unsigned long long NSec;
	unsigned short Len, TextLen, DictLen;
	unsigned char Facility, Flag;
} KLOGV2;
#pragma pack()

typedef struct _LoggerLog
{
	const char *pBuf;
	unsigned WOff, Head;
	unsigned Size;

#define F_LOG_RAW   (1 << 0)
#define F_LOG_FIXUP (1 << 1) /* 表示需要修复 */
	unsigned Flag;
} LoggerLog;

static LoggerLog Logger[MAX_LOGGER_CNT];


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	从mrdump获取android log信息
*
* PARAMETERS
*	pLog: 待填充的结构体
*	pFuncName: 查找的函数名
*	pVarName: 查找的变量名
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int GetDroidLogInfo(LoggerLog *pLog, const char *pFuncName, const char *pVarName)
{
#if 0
	const unsigned *pInst, *pVal;
	unsigned Addr, tAddr;
	
	if (GET_KGLBFUNATR_BY_NAME(pVmSHdl, &Addr, NULL, pFuncName)) {
		SSTERR(RS_NO_FUN, pFuncName);
		return 1;
	}
	tAddr = FUN_PROLOGUE(Addr) + 5 * 4; /* 跳过函数头 */
	pInst = DumpKLMem(tAddr, 0x100); /* 不需check,不可能失败,除非kallsym异常 */
	if ((pInst[0] >> 12) != 0x059F6 || (pInst[4] >> 12) != 0x05965 || (pInst[14] >> 12) != 0xE5961) { /* log_xxx */
		SSTERR(RS_UN_PATTERN, pVarName, *pInst);
		return 1;
	}
	tAddr = ARM_LTORG_VAL(pInst) + (pInst[4]&0xFFF) - 4; /* log_xxx.w_off地址 */
	pVal  = DumpKLMem(tAddr, 3 * 4); /* 结构体log_xxx，不需调用FreeMem释放 */
	if (!pVal) {
		SSTERR(RS_VAR_VAL, pVarName);
		return 1;
	}
	pLog->Flag = F_LOG_RAW;
	pLog->WOff = *pVal;
	pLog->Head = *(pVal + 1);
	pLog->Size = *(pVal + 2);
	tAddr = *(unsigned *)((unsigned)pVal + (pInst[14]&0xFFF) + 4 - (pInst[4]&0xFFF)); /* log_xxx.buffer指向的地址 */
	pLog->pBuf = DumpKLMem(tAddr, pLog->Size);
	if (!pLog->pBuf)
		SSTERR(RS_VAR_VAL, pVarName);
	return !pLog->pBuf;
#else
	(void)pLog, (void)pFuncName, (void)pVarName;
	return 1;
#endif
}

/*************************************************************************
* DESCRIPTION
*	从mrdump获取kernel log信息
*
* PARAMETERS
*	pLog: 待填充的结构体
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int GetKernelLogInfo(LoggerLog *pLog)
{
#if 0
	const unsigned *pInst, *pVal;
	unsigned Addr, tAddr;

	if (GET_KLCLFUNATR_BY_NAME(pVmSHdl, &Addr, NULL, "emit_log_char")) {
		SSTERR(RS_NO_FUN, "emit_log_char");
		return 1;
	}
	pInst = DumpKLMem(FUN_PROLOGUE(Addr), 0x78); /* 不需check,不可能失败,除非kallsym异常 */
	if (!IS_ARM_LTORG_INST(pInst, 1) /* base1 */
	 || !IS_ARM_LTORG_INST(pInst + 1, 3) /* base2 */
	 || (pInst[2] >> 12) != 0xE5912 /* log_buf_len */
	 || (pInst[3] >> 12) != 0xE5934 /* log_end */
	 || (pInst[5] >> 12) != 0xE591C /* log_buf */
	 || (pInst[17] >> 12) != 0xE5933) { /* logged_chars */
		SSTERR(RS_UN_PATTERN, "log_buf", pInst[0]);
		return 1;
	}
	tAddr = ARM_LTORG_VAL(pInst); /* base1 */
	pVal  = DumpKLMem(tAddr + (pInst[2]&0xFFF), sizeof(pLog->Size)); /* 变量log_buf_len，不需调用FreeMem释放 */
	if (!pVal) {
		SSTERR(RS_VAR_VAL, "log_buf_len");
		return 1;
	}
	pLog->Size = *pVal;
	pVal  = DumpKLMem(tAddr + (pInst[5]&0xFFF), sizeof(pLog->pBuf)); /* 变量log_buf，不需调用FreeMem释放 */
	if (!pVal || (pLog->pBuf = DumpKLMem(*pVal, pLog->Size)) == NULL) { /* log_buf指向的位置也不需要释放 */
		SSTERR(RS_VAR_VAL, "log_buf");
		return 1;
	}
	tAddr = ARM_LTORG_VAL(pInst + 1); /* base2 */
	pVal  = DumpKLMem(tAddr + (pInst[3]&0xFFF), sizeof(pLog->WOff)); /* 变量log_end，不需调用FreeMem释放 */
	if (!pVal) {
		SSTERR(RS_VAR_VAL, "log_end");
		return 1;
	}
	pLog->WOff = *pVal&(pLog->Size - 1);
	pVal = DumpKLMem(tAddr + (pInst[17]&0xFFF), sizeof(pLog->Head)); /* 变量logged_chars，不需调用FreeMem释放 */
	if (!pVal) {
		SSTERR(RS_VAR_VAL, "logged_chars");
		return 1;
	}
	pLog->Head = (pLog->Size + pLog->WOff - *pVal)&(pLog->Size - 1);
	pLog->Flag = 0;
	return !pLog->pBuf;
#else
	(void)pLog;
	return 1;
#endif
}

/*************************************************************************
* DESCRIPTION
*	从文件获取log信息
*
* PARAMETERS
*	Type: log类型
*	pFileName: 文件名
*	Flag: log标记(F_LOG_*)
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int GetLogFromFile(LOGGER_TYPE Type, const char *pFileName, unsigned Flag)
{
	LoggerLog * const pLog = &Logger[Type];
	FILE *fp;

	fp = fopen(pFileName, "rb");
	if (!fp)
		return 1;
	fseek(fp, 0, SEEK_END);
	pLog->WOff = ftell(fp);
	if (!pLog->WOff || (pLog->pBuf = Malloc(pLog->WOff)) == NULL) {
		fclose(fp);
		return 1;
	}
	fseek(fp, 0, SEEK_SET);
	pLog->Size = fread((void *)pLog->pBuf, 1, pLog->WOff, fp);
	fclose(fp);
	if (pLog->Size != pLog->WOff) {
		Free((void *)pLog->pBuf);
		pLog->pBuf = NULL;
		pLog->Size = 0;
		return 1;
	}
	pLog->Head = 0;
	pLog->Flag = Flag;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	从raw kernel log文件获取log信息
*
* PARAMETERS
*	无
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int GetRawKernLogFile(void)
{
	char Path[MAX_PATH];

	return (FindRawKernLog(Path) ? 1 : GetLogFromFile(KERNEL_LOG, Path, F_LOG_RAW|F_LOG_FIXUP));
}

/*************************************************************************
* DESCRIPTION
*	修复android log结构体信息
*
* PARAMETERS
*	pLog: 待修复的结构体
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FixupDroidLog(LoggerLog *pLog)
{
	(void)pLog; /* 暂时没有需求 */
}

/*************************************************************************
* DESCRIPTION
*	修复kernel log结构体信息
*
* PARAMETERS
*	pLog: 待修复的结构体
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FixupKernelLog(LoggerLog *pLog)
{
	unsigned Size = pLog->Size, Idx = 0, PrevIdx = 0;
	const char * const pBuf = pLog->pBuf;
	unsigned long long StartNSec, EndNSec;
	KLOGV2 *pMsg;

	if (Size <= sizeof(*pMsg)) {
		Free((void *)pLog->pBuf);
		pLog->pBuf = NULL;
		pLog->Size = 0;
		return;
	}
	Size -= sizeof(*pMsg);
	EndNSec = StartNSec = ((KLOGV2 *)pBuf)->NSec;
	do {
		pMsg = (KLOGV2 *)&pBuf[Idx];
		if (((pMsg->DictLen + pMsg->TextLen + sizeof(*pMsg) + 3)&~3) != pMsg->Len
		  && ((pMsg->DictLen + pMsg->TextLen + sizeof(*pMsg) + 7)&~7) != pMsg->Len) {
			Idx += 4; /* 自动查找下一个有效msg!!! */
			continue;
		}
		EndNSec = pMsg->NSec;
		if (StartNSec > EndNSec) { /* 寻找时间戳起点 */
			StartNSec = EndNSec;
			pLog->WOff = PrevIdx;
			pLog->Head = Idx;
		}
		Idx += pMsg->Len;
		PrevIdx = Idx;
	} while (Idx < Size);
	if (pLog->WOff == pLog->Size) {
		pLog->WOff = PrevIdx;
		pLog->Head = 0;
	}
	pLog->Size = PrevIdx;
}

/*************************************************************************
* DESCRIPTION
*	获取指定log的buffer
*
* PARAMETERS
*	Type: log类型
*	pSize: 返回buffer大小
*
* RETURNS
*	NULL: 失败
*	其他: buffer，无用时，必须用Free()释放!!!
*
* LOCAL AFFECTED
*
*************************************************************************/
static void *GetLogBuffer(LOGGER_TYPE Type, unsigned *pSize)
{
	LoggerLog * const pLog = &Logger[Type];
	unsigned Left;
	char *pBuf;

	if (pLog->Flag&F_LOG_FIXUP) {
		pLog->Flag &= ~F_LOG_FIXUP;
		Type == KERNEL_LOG ? FixupKernelLog(pLog) : FixupDroidLog(pLog);
	}
	if (!pLog->Size)
		return NULL;
	pBuf = Malloc(pLog->Size + 1/* kernel log需要额外'\0'守卫 */);
	if (!pBuf) {
		SSTWARN(RS_OOM);
		return NULL;
	}
	if (pLog->Head >= pLog->WOff) {
		Left = pLog->Size - pLog->Head;
		memcpy(pBuf, pLog->pBuf + pLog->Head, Left);
		memcpy(pBuf + Left, pLog->pBuf, pLog->WOff);
		Left += pLog->WOff;
	} else {
		Left = pLog->WOff - pLog->Head;
		memcpy(pBuf, pLog->pBuf + pLog->Head, Left);
	}
	*pSize = Left;
	return pBuf;
}

/*************************************************************************
* DESCRIPTION
*	扫描kernel log
*
* PARAMETERS
*	pBuf: log buffer
*	Size: log buffer大小
*	pFunc: 分析回调函数指针
*		pBuff: 一条log信息
*		pArg: 传进来的参数
*	pArg: 传给pFunc的参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: pFunc返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ParseKernelLogV1(char *pBuf, unsigned Size, int (*pFunc)(char *pBuff, void *pArg), void *pArg)
{
	char *pBuff, *q, *t;
	int rt = 0;

	t = &pBuf[Size];
	*t = '\0';
	for (pBuff = pBuf; pBuff < t; ) {
		if (*pBuff == '\0') { /* 剔除0 */
			pBuff++;
			continue;
		}
		q = strchr(pBuff, '\n');
		if (q)
			*q++ = '\0';
		else
			q = pBuff + strlen(pBuff) + 1;
		rt = pFunc(pBuff, pArg);
		if (rt)
			break;
		pBuff = q;
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	扫描raw kernel log
*
* PARAMETERS
*	pBuf: log buffer
*	Size: log buffer大小
*	pFunc: 分析回调函数指针
*		pBuff: 一条log信息
*		pArg: 传进来的参数
*	pArg: 传给pFunc的参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: pFunc返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ParseKernelLogV2(char *pBuf, unsigned Size, int (*pFunc)(char *pBuff, void *pArg), void *pArg)
{
	enum
	{
		LOG_NOCONS  = 1,
		LOG_NEWLINE = 2,
		LOG_PREFIX  = 4,
		LOG_CONT	= 8,
	};
	char Buf[4096], PrefixBuf[32];
	char *pSrc, *pSEnd, *pDst = Buf;
	unsigned PrefixBufSz, Idx = 0;
	KLOGV2 *pMsg;
	int rt = 1;

	if (Size <= sizeof(*pMsg))
		return rt;
	Size -= sizeof(*pMsg);
	do {
		pMsg = (KLOGV2 *)&pBuf[Idx];
		if (((pMsg->DictLen + pMsg->TextLen + sizeof(*pMsg) + 3)&~3) != pMsg->Len
		  && ((pMsg->DictLen + pMsg->TextLen + sizeof(*pMsg) + 7)&~7) != pMsg->Len) { /* 忽略无效的msg */
			Idx += 4; /* 尝试查找下一个有效msg!!! */
			continue;
		}
		Idx += pMsg->Len;
		if (pDst + pMsg->TextLen > Buf + sizeof(Buf) - sizeof(PrefixBuf)) /* 溢出 */
			goto _PRINT;
		PrefixBufSz = snprintf(PrefixBuf, sizeof(PrefixBuf), "[%5lu.%06lu]", (unsigned)(pMsg->NSec / 1000000000), (unsigned)((pMsg->NSec % 1000000000) / 1000));
		if (pDst == Buf || (pMsg->Flag&LOG_PREFIX)) {
			memcpy(pDst, PrefixBuf, PrefixBufSz);
			pDst += PrefixBufSz;
		}
		for (pSrc = (char *)(pMsg + 1), pSEnd = pSrc + pMsg->TextLen; pSrc < pSEnd; ) {
			if ((*pDst++ = *pSrc++) == '\n') {
				pDst[-1] = '\0';
				rt = pFunc(Buf, pArg);
				if (rt)
					return rt;
				memcpy(Buf, PrefixBuf, PrefixBufSz);
				pDst = &Buf[PrefixBufSz];
			}
		}
		if ((pMsg->Flag&(LOG_CONT|LOG_NEWLINE)) != LOG_CONT) {
_PRINT:	 if (pDst != Buf) {
				pDst[pDst[-1] == '\r' ? -1 : 0] = '\0'; /* 填充'\0' */
				pDst = Buf;
				rt = pFunc(Buf, pArg);
				if (rt)
					return rt;
			}
		}
	} while (Idx < Size);
	if (pDst != Buf) {
		*pDst = '\0';
		rt = pFunc(Buf, pArg);
	}
	return rt;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	确认是否存在log
*
* PARAMETERS
*	Type: log类型
*
* RETURNS
*	0: 不存在
*	1: 存在
*
* LOCAL AFFECTED
*
*************************************************************************/
int ExistLogInf(LOGGER_TYPE Type)
{
	return !!Logger[Type].Size;
}

/*************************************************************************
* DESCRIPTION
*	扫描android log
*
* PARAMETERS
*	Type: log类型,不能是KERNEL_LOG!!!
*	pFunc: 分析回调函数指针
*		pEntry: 一条log信息
*		pMsg: log buffer
*		pArg: 传进来的参数
*	pArg: 传给pFunc的参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: pFunc返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ParseDroidLog(LOGGER_TYPE Type, int (*pFunc)(LoggerEntry *pEntry, char *pMsg, void *pArg), void *pArg)
{
	LoggerEntry *pEntry;
	unsigned Size, Idx;
	char *pBuf;
	int rt = 1;

	pBuf = GetLogBuffer(Type, &Size);
	if (!pBuf)
		return rt;
	if (Size > sizeof(*pEntry)) {
		Size -= sizeof(*pEntry);
		Idx = 0;
		do {
			pEntry = (LoggerEntry *)&pBuf[Idx];
			Idx += pEntry->HdrSize;
			if (!pEntry->HdrSize || Idx >= Size) {
				rt = 1;
				break;
			}
			rt = pFunc(pEntry, (char *)&pBuf[Idx], pArg);
			Idx += pEntry->Len;
		} while (!rt && Idx < Size);
	}
	Free(pBuf);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	扫描kernel log
*
* PARAMETERS
*	pFunc: 分析回调函数指针
*		pBuff: 一条log信息
*		pArg: 传进来的参数
*	pArg: 传给pFunc的参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: pFunc返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ParseKernelLog(int (*pFunc)(char *pBuff, void *pArg), void *pArg)
{
	char *pBuf;
	int rt = 1;
	unsigned Size;

	pBuf = GetLogBuffer(KERNEL_LOG, &Size);
	if (pBuf) {
		rt = (Logger[KERNEL_LOG].Flag&F_LOG_RAW ? ParseKernelLogV2(pBuf, Size, pFunc, pArg) : ParseKernelLogV1(pBuf, Size, pFunc, pArg));
		Free(pBuf);
	}
	return rt;
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化logger模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: logger模块关闭
*	1: logger模块可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int LoggerInit(void)
{
	int rt = 0;

	if (GetKExpInf()->mType == MRD_T_MRDUMP) {
		if (GetDroidLogInfo(&Logger[MAIN_LOG],   "panic_dump_main",   "log_main"))
			rt |= (1 << MAIN_LOG);
		if (GetDroidLogInfo(&Logger[SYSTEM_LOG], "panic_dump_system", "log_system"))
			rt |= (1 << SYSTEM_LOG);
		if (GetDroidLogInfo(&Logger[EVENT_LOG],  "panic_dump_events", "log_events"))
			rt |= (1 << EVENT_LOG);
		if (GetDroidLogInfo(&Logger[RADIO_LOG],  "panic_dump_radio",  "log_radio"))
			rt |= (1 << RADIO_LOG);
		if (GetKernelLogInfo(&Logger[KERNEL_LOG]))
			rt |= (1 << KERNEL_LOG);
		return rt != (1 << MAX_LOGGER_CNT) - 1;
	}
	if (GetLogFromFile(MAIN_LOG, "SYS_MAIN_LOG_RAW", F_LOG_RAW|F_LOG_FIXUP))
		GetLogFromFile(MAIN_LOG, "SYS_ANDROID_LOG_RAW", F_LOG_RAW|F_LOG_FIXUP);
	if (GetLogFromFile(SYSTEM_LOG, "SYS_SYSTEM_LOG_RAW", F_LOG_RAW|F_LOG_FIXUP))
		GetLogFromFile(SYSTEM_LOG, "SYS_ANDROID_SYSTEM_LOG_RAW", F_LOG_RAW|F_LOG_FIXUP);
	GetLogFromFile(EVENT_LOG, "SYS_EVENTS_LOG_RAW", F_LOG_RAW|F_LOG_FIXUP);
	if (GetLogFromFile(RADIO_LOG, "SYS_RADIO_LOG_RAW", F_LOG_RAW|F_LOG_FIXUP))
		GetLogFromFile(RADIO_LOG, "SYS_ANDROID_RADIO_LOG_RAW", F_LOG_RAW|F_LOG_FIXUP);
	if (GetLogFromFile(KERNEL_LOG, "SYS_KERNEL_LOG", 0) && GetRawKernLogFile())
		GetLogFromFile(KERNEL_LOG, "SYS_LAST_KMSG", 0);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	注销logger模块
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
void LoggerDeInit(void)
{
	int i;
	
	if (GetKExpInf()->mType != MRD_T_MRDUMP) {
		for (i = MAIN_LOG; i <= KERNEL_LOG; i++) {
			Free((void *)Logger[i].pBuf);
			Logger[i].pBuf = NULL;
			Logger[i].Size = 0;
		}
	}
}
