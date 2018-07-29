/*****************************************************************************
* Copyright(C),2017-2017,MediaTek Inc.
* FileName:	logeigenclient.c
* Author:		chao.peng
* Version:	V1.0
* Date:		2017/3/15
* Description: 从logeigen系统中查询关联的Cr及记录当前Cr
******************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "string_res.h"
#ifdef _WIN32
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define SOCKET int
#endif
#include "os.h"
#include <module.h>
#include <mrdump.h>
#include <callstack.h>
#include <armdis.h>
#include <db/db.h>
#include "cJSON.h"
#include "logeigenclient.h"


#define HOST_IP			"172.26.2.235"
#define HOST_PORT		6030
#define KEY_INF_LEN		(2048 - 64) /* logeigen系统要求 */

#define QUERY_LOG_EIGEN_BY_KEY_INFO		1
#define QUERY_ESERVICE_LIST_BY_EIGEN	2
#define ADD_ESERVICE_INFO				20
#define ADD_ESERVICE_LOG_EIGEN			22
#define ADD_ESERVICE_LOG				23


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	和logeigen系统通信，返回反馈的信息
*
* PARAMETERS
*	ppRoot：返回反馈的json root,不用时需要释放, 可以为NULL
*	pFmt: 格式化发送的数据(json格式)
*
* RETURNS
*	NULL: 失败
*	其他: 反馈的信息
*
* LOCAL AFFECTED
*
*************************************************************************/
static cJSON *SendAndRecvInf(cJSON **ppRoot, const char *pFmt, ...)
{
	const int TimeOutMs = 4000;
	char RecvBuf[16 * 1024], *pData;
#ifdef _WIN32
	SOCKADDR_IN SockAddr;
#else
	struct sockaddr_in SockAddr;
#endif
	cJSON *pRt = NULL;
	int DataSz, rt;
	SOCKET SockId;
	va_list ap;

	SockId = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (SockId < 0) {
		DBGERR("创建socket失败\n");
		return pRt;
	}
	setsockopt(SockId, SOL_SOCKET, SO_RCVTIMEO, (void *)&TimeOutMs, sizeof(TimeOutMs));
	SockAddr.sin_family = AF_INET;
	SockAddr.sin_port = htons(HOST_PORT);
#ifdef _WIN32
	SockAddr.sin_addr.S_un.S_addr = inet_addr(HOST_IP);
#else
	SockAddr.sin_addr.s_addr = inet_addr(HOST_IP);
#endif
	if (connect(SockId, (void *)&SockAddr, sizeof(SockAddr)) != 0) {
		DBGERR("连接logeigen系统失败\n");
		goto _OUT;
	}
	/* 构建发送的数据 */
	va_start(ap, pFmt);
	DataSz = vsnprintf(NULL, 0, pFmt, ap);
	pData = malloc(DataSz + 1);
	if (!pData) {
		va_end(ap);
		SSTWARN(RS_OOM);
		goto _OUT;
	}
	vsprintf(pData, pFmt, ap);
	va_end(ap);
	/* 发送和接收 */
	rt = send(SockId, pData, DataSz, 0);
	free(pData);
	if (rt <= 0 || (rt = recv(SockId, RecvBuf, sizeof(RecvBuf) - 1, 0)) <= 0) {
		DBGERR("发送或接收失败\n");
		goto _OUT;
	}
	/* 解析接收到的信息 */
	if (ppRoot) {
		RecvBuf[rt] = '\0';
		*ppRoot = cJSON_Parse(RecvBuf);
		pRt = cJSON_GetObjectItem(*ppRoot, "result");
		pRt = (pRt && pRt->valueint >= 0 ? cJSON_GetObjectItem(cJSON_GetObjectItem(*ppRoot, "datas"), "queryResult") : NULL);
	}
_OUT:
#ifdef _WIN32
	closesocket(SockId);
#else
	close(SockId);
#endif
	return pRt;
}

/*************************************************************************
* DESCRIPTION
*	获取当前解析DB的build类型
*
* PARAMETERS
*	无
*
* RETURNS
*	build type
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *GetBuildType(void)
{
	switch (BuildVar()) {
	case BUILD_USER:	return "user";
	case BUILD_USERDBG: return "userdebug";
	case BUILD_ENG:		return "eng";
	default: return "??";
	}
}

/*************************************************************************
* DESCRIPTION
*	显示关联的Cr信息
*
* PARAMETERS
*	fp: 文件句柄
*	EigenId: 特征值
*	pCrId: 当前的Cr id, 可以为NULL
*	MaxDispCnt: 最多显示几条记录
*
* RETURNS
*	0: 当前Cr已包含在logeigen系统或Cr id为NULL
*	1: 当前Cr未包含在logeigen系统
*
* LOCAL AFFECTED
*
*************************************************************************/
int DispRelatedInf(FILE *fp, int EigenId, const char *pCrId, int MaxDispCnt)
{
#define DIS_ROW_FLAG 0x80000000
	const char *pObjNames[] = {"eserviceId", "buildType", "patch", "mediatekPlatfrom", "mediatekBranch", "customer", "assignTeam", "assignOwner"};
	cJSON *pInf, *pRoot = NULL, *pItem, *pCrItem, *pObj;
	size_t MaxSzs[ARY_CNT(pObjNames)], Sz;
	int i, rt = !!pCrId;
	size_t j;

	pInf = SendAndRecvInf(&pRoot, "{\"cmd\":%d,\"datas\":{\"eigenId\":%d}}", QUERY_ESERVICE_LIST_BY_EIGEN, EigenId);
	if (!pInf)
		goto _OUT;
	for (i = j = 0; j < ARY_CNT(pObjNames); j++) { /* 初始化提示栏长度 */
		MaxSzs[j] = strlen(GetCStr(RS_EIGEN_INF0 + j))|DIS_ROW_FLAG;
	}
	cJSON_ArrayForEach(pItem, pInf) {
		pCrItem = cJSON_GetObjectItem(pItem, "eservice");
		pObj = cJSON_GetObjectItem(pCrItem, "eserviceId");
		if (!pObj || !pObj->valuestring)
			continue;
		if (pCrId && !strcmp(pCrId, pObj->valuestring)) {
			rt = 0;
			continue;
		}
		if (i >= MaxDispCnt)
			continue;
		i++;
		for (j = 0; j < ARY_CNT(pObjNames); j++) { /* 记录最大长度 */
			pObj = cJSON_GetObjectItem(pCrItem, pObjNames[j]);
			if (!pObj || !pObj->valuestring)
				continue;
			Sz = strlen(pObj->valuestring);
			MaxSzs[j] = (Sz > (MaxSzs[j]&~DIS_ROW_FLAG) ? Sz : (MaxSzs[j]&~DIS_ROW_FLAG));
		}
	}
	if (!i)
		goto _OUT;
	LOG(fp, RS_RELATED_CR);
	for (j = 0; j < ARY_CNT(pObjNames); j++) { /* 输出提示栏 */
		if (!(MaxSzs[j]&DIS_ROW_FLAG))
			LOGC(fp, "%-*s", MaxSzs[j] + 1, GetCStr(RS_EIGEN_INF0 + j));
	}
	cJSON_ArrayForEach(pItem, pInf) { /* 输出内部 */
		pCrItem = cJSON_GetObjectItem(pItem, "eservice");
		pObj = cJSON_GetObjectItem(pCrItem, "eserviceId");
		if (!pObj || !pObj->valuestring)
			continue;
		if (--i < 0)
			break;
		LOGC(fp, "\n");
		for (j = 0; j < ARY_CNT(pObjNames); j++) {
			if (!(MaxSzs[j]&DIS_ROW_FLAG))
				LOGC(fp, "%-*s", MaxSzs[j] + 1, cJSON_GetObjectItem(pCrItem, pObjNames[j])->valuestring);
		}
	}
	LOGC(fp, "\n\n\n");
_OUT:
	cJSON_Delete(pRoot);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	通过KE的信息获取完整backtrace
*
* PARAMETERS
*	pKeyInf: 反馈特征
*	Sz: pKeyInf长度(单位: 字节)
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int LogEigenGetKeyInf(char *pKeyInf, size_t KeySz)
{
	KEXP_INF * const pKExpInf = GetKExpInf();
	CORE_CNTX *pCpuCntx;
	CS_HDL *pCsHdl;
	CS_FRAME *pCsF;
	size_t Sz = 0;

	switch (pKExpInf->eType) {
	case EXP_NO_EXP: case EXP_HWT: case EXP_HW_REBOOT: case EXP_ATF_CRASH: case EXP_THERMAL_RB: case EXP_SPM_RB:
		return 1;
	default:
		pCpuCntx = GetCoreCntx(pKExpInf->CpuId);
		if (!pCpuCntx)
			return 1;
		pCsHdl = GetNativeCsHdl(pCpuCntx->RegAry, 0);
		if (!pCsHdl)
			return 1;
		pKeyInf[KeySz - 1] = '\0';
		for (pCsF = pCsHdl; pCsF; pCsF = pCsF->pNext) {
			if (!pCsF->pFName)
				break;
			snprintf(&pKeyInf[Sz], KeySz - 1 - Sz, "%s + 0x%x\\n", pCsF->pFName, pCsF->Off);
			Sz = strlen(pKeyInf);
		}
		PutCsHdl(pCsHdl);
	}
	return !Sz;
}

/*************************************************************************
* DESCRIPTION
*	用特征来查询特征值
*
* PARAMETERS
*	pType: 异常类型
*	pAccurate: 精确度, 只能为"false"或"true"
*	pKeyInf: 特征数据
*
* RETURNS
*	< 0: 失败
*	0: 未找到
*	> 0: 成功, 返回eigen id
*
* LOCAL AFFECTED
*
*************************************************************************/
static int QueryEigenId(const char *pType, const char *pAccurate, const char *pKeyInf)
{
	cJSON *pInf, *pRoot = NULL;
	int EigenId = -1;

	pInf = SendAndRecvInf(&pRoot
		, "{\"cmd\":%d,\"datas\":{\"accurate\":%s,\"type\":\"%s\",\"keyInfo\":\"%s\"}}"
		, QUERY_LOG_EIGEN_BY_KEY_INFO, pAccurate, pType, pKeyInf);
	pInf = cJSON_GetObjectItem(pInf, "id");
	if (pInf)
		EigenId = pInf->valueint;
	else if (pRoot)
		EigenId = 0;
	cJSON_Delete(pRoot);
	return EigenId;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	显示关联的Cr信息
*
* PARAMETERS
*	fp: 输出到的文件
*	pCrId: Cr id, 为NULL则不记录
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispAndRecRelatedCr(FILE *fp, const char *pCrId)
{
	char KeyInf[KEY_INF_LEN];
	int EigenId;
#ifdef _WIN32
	WSADATA wsa;

	WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
	if (LogEigenGetKeyInf(KeyInf, sizeof(KeyInf))) /* 没有特征 */
		goto _OUT;
	EigenId = QueryEigenId("KE", "false", KeyInf);
	if (EigenId > 0) {
		SSTINFO(RS_OUT_RELATED_INF);
		if (DispRelatedInf(fp, EigenId, pCrId, 10)) {
			SendAndRecvInf(NULL
				, "{\"cmd\":%d,\"datas\":{\"eserviceInfo\":{\"eserviceId\":\"%s\",\"buildType\":\"%s\"}}}"
				, ADD_ESERVICE_INFO, pCrId, GetBuildType());
			SendAndRecvInf(NULL
				, "{\"cmd\":%d,\"datas\":{\"eserviceLog\":{\"eigenId\":%d,\"eservice\":{\"eserviceId\":\"%s\"}}}}"
				, ADD_ESERVICE_LOG, EigenId, pCrId);
		}
	} else if (!EigenId && pCrId) {
		SendAndRecvInf(NULL,
			"{\"cmd\":%d,\"datas\":{\"accurate\":%s,"
			"\"logEigen\":{\"keyInfo\":\"%s\",\"createTool\":\"%s\",\"source\":\"%s\",\"eigenType\":{\"rank\":%d,\"name\":\"%s\"}},"
			"\"eserviceInfo\":{\"eserviceId\":\"%s\",\"buildType\":\"%s\"}}}"
			, ADD_ESERVICE_LOG_EIGEN, "false", KeyInf, SST_NAME, "aee_db", 3, "KE", pCrId, GetBuildType());
	}
_OUT:;
#ifdef _WIN32
	WSACleanup();
#endif
}
