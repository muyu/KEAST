/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	logger_tracker.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: log模块解析
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "string_res.h"
#include <ac.h>
#include <module.h>
#include "os.h"
#include <mrdump.h>
#include "logger.h"
#include "logger_tracker.h"

#define GET4LE(x) ((*(unsigned char *)(x))|(*(unsigned char *)(x + 1) << 8)|(*(unsigned char *)(x + 2) << 16)|(*(unsigned char *)(x + 3) << 24))

typedef struct _EVENT_LOG_INFO
{
	unsigned Idx;
	const char *pTags;
	unsigned Len;
	const char **pNames;
} EVENT_LOG_INFO;

typedef struct _DISP_LOG_INFO
{
	FILE *fp;
	unsigned Step;
} DISP_LOG_INFO;

typedef struct _PARSE_KELOG_INFO
{
	FILE *fp;
	int NoLog;
	AC_HDL *pAcHdl;
} PARSE_KELOG_INFO;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	格式化main/redio/system log数据到文件
*
* PARAMETERS
*	pEntry: log结构体
*	pMsg: log buffer
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispMainLogLine(LoggerEntry *pEntry, char *pMsg, void *pArg)
{
	static const char Levels[] = {'?', '?', 'V', 'D', 'I', 'W', 'E', 'F', 'S'};
	DISP_LOG_INFO *pInf = (DISP_LOG_INFO *)pArg;
	char *pTag, *pBuf, Lv;
	struct tm *pLTime;
	time_t CurTime;

	Lv = Levels[(unsigned)pMsg[0] >= ARY_CNT(Levels) ? 0 : pMsg[0]];
	pTag = &pMsg[1];
	pMsg = pTag + strlen(pTag) + 1;
	CurTime = pEntry->Sec;
	pLTime = localtime(&CurTime);
	do {
		LOGC(pInf->fp, "%02d-%02d %02d:%02d:%02d.%03d %5d %5d %c %-*s "
			, pLTime->tm_mon + 1, pLTime->tm_mday, pLTime->tm_hour, pLTime->tm_min, pLTime->tm_sec, pEntry->Nsec / 1000000, pEntry->Pid, pEntry->Tid, Lv, pInf->Step, pTag);
		pBuf = strchr(pMsg, '\n');
		if (!pBuf) {
			LOGC(pInf->fp, "%s\n", pMsg);
			break;
		}
		pBuf[pBuf >= pMsg + 1 && pBuf[-1] == '\r' ? -1 : 0] = '\0';
		LOGC(pInf->fp, "%s\n", pMsg);
		pMsg = pBuf + 1;
	} while (pMsg[0]);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	显示一个event
*
* PARAMETERS
*	fp: 文件句柄
*	pInfo: event相关信息
*	Idx: 当前成员index
*	pBuf: buffer
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static char *DispEvent(FILE *fp, EVENT_LOG_INFO *pInfo, unsigned Idx, char *pBuf)
{
	typedef enum _EVENT_LOG_TYPE
	{
		EVENT_TYPE_INT	= 0,
		EVENT_TYPE_LONG   = 1,
		EVENT_TYPE_STRING = 2,
		EVENT_TYPE_LIST   = 3,
	} EVENT_LOG_TYPE;
	
	unsigned i, Len;
	long long Val8;

	if (pInfo && Idx < pInfo->Len && *pBuf != EVENT_TYPE_LIST)
		LOGC(fp, "%s = ", pInfo->pNames[Idx]);
	switch (*pBuf++) {
	case EVENT_TYPE_INT:
		Len = GET4LE(pBuf);
		pBuf += sizeof(Len);
		LOGC(fp, "%d", (int)Len);
		break;
	case EVENT_TYPE_LONG:
		Len = GET4LE(pBuf);
		pBuf += sizeof(Len);
		i = GET4LE(pBuf);
		pBuf += sizeof(i);
		Val8 = (long long)Len|((long long)i << 32);
		LOGC(fp, "%I64d", Val8);
		break;
	case EVENT_TYPE_STRING:
		Len = GET4LE(pBuf);
		pBuf += sizeof(Len);
		i = pBuf[Len]; /* 备份 */
		pBuf[Len] = '\0';
		LOGC(fp, "\"%s\"", pBuf);
		pBuf += Len;
		pBuf[0] = (unsigned char)i;
		break;
	case EVENT_TYPE_LIST:
		LOGC(fp, "[");
		Len = *pBuf;
		pBuf = DispEvent(fp, pInfo, Idx, pBuf + 1);
		for (i = 1; i < Len; i++) {
			LOGC(fp, ", ");
			pBuf = DispEvent(fp, pInfo, Idx + i, pBuf);
		}
		LOGC(fp, "]");
		break;
	default:
		LOGC(fp, "error:%d", *(pBuf - 1));
		break;
	}
	return pBuf;
}

/*************************************************************************
* DESCRIPTION
*	根据tag查找以之相关的信息
*
* PARAMETERS
*	Tag: 文件句柄
*
* RETURNS
*	NULL: 无信息
*	其他: 相关信息
*
* LOCAL AFFECTED
*
*************************************************************************/
static EVENT_LOG_INFO *LookupEventInfo(unsigned Tag)
{
#define EVENT_INFO_U(Tag, Str) {Tag, Str, sizeof(s##Tag) / sizeof(s##Tag[0]), s##Tag}
	static const char *s42[]	= {"to life the universe etc"};
	static const char *s2719[]  = {"config mask"};
	static const char *s2720[]  = {"id", "event", "source", "account"};
	static const char *s2721[]  = {"total", "user", "system", "iowait", "irq", "softirq"};
	static const char *s2722[]  = {"level", "voltage", "temperature"};
	static const char *s2723[]  = {"status", "health", "present", "plugged", "technology"};
	static const char *s2724[]  = {"wakeLocksCleared"};
	static const char *s2725[]  = {"wakelockCount"};
	static const char *s2726[]  = {"on", "broadcastDuration", "wakelockCount"};
	static const char *s2727[]  = {"which", "wakelockCount"};
	static const char *s2728[]  = {"offOrOn", "becauseOfUser", "totalTouchDownTime", "touchCycles"};
	static const char *s2729[]  = {"releasedorAcquired", "tag"};
	static const char *s2730[]  = {"duration", "minLevel", "maxLevel"};
	static const char *s2741[]  = {"reason"};
	static const char *s2742[]  = {"authority"};
	static const char *s2744[]  = {"data"};
	static const char *s2745[]  = {"data"};
	static const char *s2746[]  = {"data", "system", "cache"};
	static const char *s2747[]  = {"aggregation time", "count"};
	static const char *s2748[]  = {"path"};
	static const char *s2750[]  = {"pkg", "id", "tag", "userid", "notification"};
	static const char *s2751[]  = {"pkg", "id", "tag", "userid", "required_flags", "forbidden_flags"};
	static const char *s2820[]  = {"Package"};
	static const char *s3000[]  = {"time"};
	static const char *s3110[]  = {"value"};

	static const char *s20001[] = {"custom", "custom", "custom", "custom"};
	static const char *s20002[] = {"total", "zygote"};
	static const char *s30006[] = {"User", "Token", "Task ID", "Component Name"};
	static const char *s30010[] = {"User", "PID", "UID", "Process Name"};
	static const char *s30014[] = {"User", "PID", "UID", "Process Name", "Type", "Component"};
	static const char *s30030[] = {"User", "Service Record", "Name", "PID"};
	static const char *s30031[] = {"User", "Service Record", "PID"};
	static const char *s50114[] = {"oldState", "oldGprsState", "newState", "newGprsState"};
	static const char *s52000[] = {"db", "sql", "time", "blocking_package", "sample_percent"};
	static const char *s52002[] = {"uri", "projection", "selection", "sortorder", "time", "blocking_package", "sample_percent"};
	static const char *s52003[] = {"uri", "projection", "selection", "time", "blocking_package", "sample_percent"};
	static const char *s52004[] = {"descriptor", "method_num", "time", "blocking_package", "sample_percent"};
	static const char *s70000[] = {"screen_state"};
	static EVENT_LOG_INFO Ary[] =
	{
		EVENT_INFO_U(42,  "answer"),
		{314, "pi", 0, NULL},
		{2718, "e", 0, NULL},
		EVENT_INFO_U(2719,  "configuration_changed"),
		EVENT_INFO_U(2720,  "sync"),
		EVENT_INFO_U(2721,  "cpu"),
		EVENT_INFO_U(2722,  "battery_level"),
		EVENT_INFO_U(2723,  "battery_status"),
		EVENT_INFO_U(2724,  "power_sleep_requested"),
		EVENT_INFO_U(2725,  "power_screen_broadcast_send"),
		EVENT_INFO_U(2726,  "power_screen_broadcast_done"),
		EVENT_INFO_U(2727,  "power_screen_broadcast_stop"),
		EVENT_INFO_U(2728,  "power_screen_state"),
		EVENT_INFO_U(2729,  "power_partial_wake_state"),
		EVENT_INFO_U(2730,  "battery_discharge"),
		{2740, "location_controller", 0, NULL},
		EVENT_INFO_U(2741,  "force_gc"),
		EVENT_INFO_U(2742,  "tickle"),
		EVENT_INFO_U(2744,  "free_storage_changed"),
		EVENT_INFO_U(2745,  "low_storage"),
		EVENT_INFO_U(2746,  "free_storage_left"),
		EVENT_INFO_U(2747,  "contacts_aggregation"),
		EVENT_INFO_U(2748,  "cache_file_deleted"),
		EVENT_INFO_U(2750,  "notification_enqueue"),
		EVENT_INFO_U(2751,  "notification_cancel"),
		EVENT_INFO_U(2820,  "backup_data_changed"),
		EVENT_INFO_U(3000,  "boot_progress_start"),
		{3010, "boot_progress_system_run", 1, s3000},
		{3020, "boot_progress_preload_start", 1, s3000},
		{3030, "boot_progress_preload_end", 1, s3000},
		{3040, "boot_progress_ams_ready", 1, s3000},
		{3050, "boot_progress_enable_screen", 1, s3000},
		{3060, "boot_progress_pms_start", 1, s3000},
		{3070, "boot_progress_pms_system_scan_start", 1, s3000},
		{3080, "boot_progress_pms_data_scan_start", 1, s3000},
		{3090, "boot_progress_pms_scan_end", 1, s3000},
		{3100, "boot_progress_pms_ready", 1, s3000},
		EVENT_INFO_U(3110,  "unknown_sources_enabled"),
		{4000, "calendar_upgrade_receiver", 1, s3000},
		{4100, "contacts_upgrade_receiver", 1, s3000},

		EVENT_INFO_U(20001, "dvm_gc_info"),
		EVENT_INFO_U(20002, "dvm_gc_madvise_info"),
		
		EVENT_INFO_U(30006, "am_restart_activity"),
		EVENT_INFO_U(30010, "am_proc_bound"),
		EVENT_INFO_U(30014, "am_proc_start"),
		EVENT_INFO_U(30030, "am_create_service"),
		EVENT_INFO_U(30031, "am_destroy_service"),
		EVENT_INFO_U(50114, "gsm_service_state_change"),
		EVENT_INFO_U(52000, "db_sample"),
		EVENT_INFO_U(52002, "content_query_sample"),
		EVENT_INFO_U(52003, "content_update_sample"),
		EVENT_INFO_U(52004, "binder_sample"),


		EVENT_INFO_U(70000, "screen_toggled"),
	};
	int hi, lo, mid, cmp;

	lo = 0;
	hi = ARY_CNT(Ary) - 1;
	while (lo <= hi) {
		mid = (lo + hi) >> 1;
		cmp = Ary[mid].Idx - Tag;
		if (cmp < 0) {
			lo = mid + 1;
		} else if (cmp > 0) {
			hi = mid - 1;
		} else {
			return &Ary[mid];
		}
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	格式化event log数据到文件
*
* PARAMETERS
*	pEntry: log结构体
*	pMsg: log buffer
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispEventsLogLine(LoggerEntry *pEntry, char *pMsg, void *pArg)
{
	DISP_LOG_INFO *pInf = (DISP_LOG_INFO *)pArg;
	EVENT_LOG_INFO *pInfo;
	unsigned Tag;
	struct tm *pLTime;
	time_t CurTime;

	CurTime = pEntry->Sec;
	pLTime = localtime(&CurTime);
	LOGC(pInf->fp, "%02d-%02d %02d:%02d:%02d.%03d %5d %5d "
		, pLTime->tm_mon + 1, pLTime->tm_mday, pLTime->tm_hour, pLTime->tm_min, pLTime->tm_sec, pEntry->Nsec / 1000000, pEntry->Pid, pEntry->Tid);
	/* tag和字符串的对应关系存放在: /system/etc/event-log-tags */
	Tag = GET4LE(pMsg);
	pInfo = LookupEventInfo(Tag);
	if (pInfo)
		LOGC(pInf->fp, "%-*s ", pInf->Step, pInfo->pTags);
	else
		LOGC(pInf->fp, "%-*d ", pInf->Step, Tag);
	DispEvent(pInf->fp, pInfo, 0, &pMsg[4]);
	LOGC(pInf->fp, "\n");
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	格式化kernel log数据到文件
*
* PARAMETERS
*	pBuff: 一条log信息
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispKernelLogLine(char *pBuff, void *pArg)
{
	DISP_LOG_INFO *pInf = (DISP_LOG_INFO *)pArg;

	LOGC(pInf->fp, "%s\n", pBuff);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	格式化kernel log数据到文件
*
* PARAMETERS
*	pBuff: 一条log信息
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int KernelFindErrorLog(char *pBuff, void *pArg)
{
#define KELOGE_PAT_MAP \
	_NL(KELOGE_SPINLOCK,	"BUG: spinlock ") \
	_NL(KELOGE_RWLOCK,	  "BUG: rwlock ") \
	_NL(KELOGE_BADPAGE,	 "BUG: Bad page state in process ") \
	_NL(KELOGE_BADPAGE1,	"BUG: Bad page state: ") \
	_NL(KELOGE_BADMAP,	  "BUG: Bad page map in process ") \
	_NL(KELOGE_BADMAP1,	 "BUG: Bad page map: ") \
	_NL(KELOGE_SCHED,	   "BUG: scheduling while atomic: ") \
	_NL(KELOGE_DENTRY,	  "BUG: Dentry ") \
	_NL(KELOGE_RSSCNT,	  "BUG: Bad rss-counter state mm:") \
	_NL(KELOGE_WQLEAK,	  "BUG: workqueue leaked lock or atomic: ") \
	_NL(KELOGE_SMPID,	   "BUG: using smp_processor_id() in preemptible [") \
	_NL(KELOGE_PRINTK,	  "BUG: recent printk recursion!") \
	_NL(KELOGE_END,		 "Internal error: ")
	typedef enum _KELOGE_RS_ID
	{
		KELOGE_START = 0,
#define _NL(Enum, Str) Enum,
		KELOGE_PAT_MAP
#undef _NL
	} KELOGE_RS_ID;
	PARSE_KELOG_INFO *pInf = (PARSE_KELOG_INFO *)pArg;
	unsigned Idx, Len;
	const char *p = pBuff;
	
	Idx = AcTrieSearch(pInf->pAcHdl, &p);
	if (Idx) {
		if (Idx == KELOGE_END)
			return 1;
		if (pInf->NoLog) {
			pInf->NoLog = 0;
			LOG(pInf->fp, RS_ELOG_DUMP);
			LOGC(pInf->fp, "kernel log:\n");
		}
		Len = strlen(pBuff);
		if (pBuff[Len - 1] == '\r') /* 去除'\r' */
			pBuff[Len - 1] = '\0';
		if (pBuff[0] == '\r') /* 去除'\r' */
			pBuff++;
		LOGC(pInf->fp, "%s\n", pBuff);
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	检查文件是否有效
*
* PARAMETERS
*	pFileName: 文件名
*
* RETURNS
*	0: 有效
*	1: 无效
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FileIsInvalid(const char *pFileName)
{
	FILE *fp;
	long Sz;

	fp = fopen(pFileName, "rb");
	if (!fp)
		return 1;
	fseek(fp, 0, SEEK_END);
	Sz = ftell(fp);
	fclose(fp);
	return !Sz;
}

/*************************************************************************
* DESCRIPTION
*	保存log到文件
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
static void ConvertLogEx(void)
{
	DISP_LOG_INFO Inf;
	
	if (ExistLogInf(MAIN_LOG) && FileIsInvalid("SYS_ANDROID_LOG")) {
		Inf.fp = fopen("SYS_ANDROID_LOG", "w");
		if (Inf.fp) {
			SSTINFO(RS_OUTLOG, "main log");
			Inf.Step = 30;
			ParseDroidLog(MAIN_LOG, DispMainLogLine, &Inf);
			fclose(Inf.fp);
		} else {
			SSTERR(RS_CREATE, "SYS_ANDROID_LOG");
		}
	}
	if (ExistLogInf(SYSTEM_LOG) && FileIsInvalid("SYS_ANDROID_SYSTEM_LOG")) {
		Inf.fp = fopen("SYS_ANDROID_SYSTEM_LOG", "w");
		if (Inf.fp) {
			SSTINFO(RS_OUTLOG, "system log");
			Inf.Step = 20;
			ParseDroidLog(SYSTEM_LOG, DispMainLogLine, &Inf);
			fclose(Inf.fp);
		} else {
			SSTERR(RS_CREATE, "SYS_ANDROID_SYSTEM_LOG");
		}
	}
	if (ExistLogInf(EVENT_LOG) && FileIsInvalid("SYS_ANDROID_EVENT_LOG")) {
		Inf.fp = fopen("SYS_ANDROID_EVENT_LOG", "w");
		if (Inf.fp) {
			SSTINFO(RS_OUTLOG, "event log");
			Inf.Step = 30;
			ParseDroidLog(EVENT_LOG, DispEventsLogLine, &Inf);
			fclose(Inf.fp);
		} else {
			SSTERR(RS_CREATE, "SYS_ANDROID_EVENT_LOG");
		}
	}
	if (ExistLogInf(RADIO_LOG) && FileIsInvalid("SYS_ANDROID_RADIO_LOG")) {
		Inf.fp = fopen("SYS_ANDROID_RADIO_LOG", "w");
		if (Inf.fp) {
			SSTINFO(RS_OUTLOG, "radio log");
			Inf.Step = 6;
			ParseDroidLog(RADIO_LOG, DispMainLogLine, &Inf);
			fclose(Inf.fp);
		} else {
			SSTERR(RS_CREATE, "SYS_ANDROID_RADIO_LOG");
		}
	}
	if (ExistLogInf(KERNEL_LOG) && FileIsInvalid("SYS_KERNEL_LOG")) {
		Inf.fp = fopen("SYS_KERNEL_LOG", "w");
		if (Inf.fp) {
			SSTINFO(RS_OUTLOG, "kernel log");
			ParseKernelLog(DispKernelLogLine, &Inf);
			fclose(Inf.fp);
		} else {
			SSTERR(RS_CREATE, "SYS_KERNEL_LOG");
		}
	}
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	保存log到文件
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
void ConvertLog(void)
{
	LoggerInit();
	ConvertLogEx();
	LoggerDeInit();
}

/*************************************************************************
* DESCRIPTION
*	输出异常log
*
* PARAMETERS
*	fp: 报告输出句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void ShowErrLog(FILE *fp)
{
	static const char * const ErrKelogPats[] =
	{
#define _NL(Enum, Str) Str,
		KELOGE_PAT_MAP
#undef _NL
		NULL,
	};
	PARSE_KELOG_INFO ParseKelogInf;

	SSTINFO(RS_DUMP_ELOG);
	ConvertLogEx();
	if (!ExistLogInf(KERNEL_LOG))
		return;
	ParseKelogInf.pAcHdl = GetAcTrie(ErrKelogPats);
	if (!ParseKelogInf.pAcHdl) {
		SSTERR(RS_OOM);
		return;
	}
	ParseKelogInf.fp	= fp;
	ParseKelogInf.NoLog = 1;
	ParseKernelLog(KernelFindErrorLog, &ParseKelogInf);
	if (!ParseKelogInf.NoLog)
		LOGC(fp, "\n\n");
	PutAcTrie(ParseKelogInf.pAcHdl);
}

