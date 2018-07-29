/*****************************************************************************
* Copyright(C),2017-2017,MediaTek Inc.
* FileName:	logeigenclient.c
* Author:		chao.peng
* Version:	V1.0
* Date:		2017/3/15
* Description: ��logeigenϵͳ�в�ѯ������Cr����¼��ǰCr
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
#define KEY_INF_LEN		(2048 - 64) /* logeigenϵͳҪ�� */

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
*	��logeigenϵͳͨ�ţ����ط�������Ϣ
*
* PARAMETERS
*	ppRoot�����ط�����json root,����ʱ��Ҫ�ͷ�, ����ΪNULL
*	pFmt: ��ʽ�����͵�����(json��ʽ)
*
* RETURNS
*	NULL: ʧ��
*	����: ��������Ϣ
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
		DBGERR("����socketʧ��\n");
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
		DBGERR("����logeigenϵͳʧ��\n");
		goto _OUT;
	}
	/* �������͵����� */
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
	/* ���ͺͽ��� */
	rt = send(SockId, pData, DataSz, 0);
	free(pData);
	if (rt <= 0 || (rt = recv(SockId, RecvBuf, sizeof(RecvBuf) - 1, 0)) <= 0) {
		DBGERR("���ͻ����ʧ��\n");
		goto _OUT;
	}
	/* �������յ�����Ϣ */
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
*	��ȡ��ǰ����DB��build����
*
* PARAMETERS
*	��
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
*	��ʾ������Cr��Ϣ
*
* PARAMETERS
*	fp: �ļ����
*	EigenId: ����ֵ
*	pCrId: ��ǰ��Cr id, ����ΪNULL
*	MaxDispCnt: �����ʾ������¼
*
* RETURNS
*	0: ��ǰCr�Ѱ�����logeigenϵͳ��Cr idΪNULL
*	1: ��ǰCrδ������logeigenϵͳ
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
	for (i = j = 0; j < ARY_CNT(pObjNames); j++) { /* ��ʼ����ʾ������ */
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
		for (j = 0; j < ARY_CNT(pObjNames); j++) { /* ��¼��󳤶� */
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
	for (j = 0; j < ARY_CNT(pObjNames); j++) { /* �����ʾ�� */
		if (!(MaxSzs[j]&DIS_ROW_FLAG))
			LOGC(fp, "%-*s", MaxSzs[j] + 1, GetCStr(RS_EIGEN_INF0 + j));
	}
	cJSON_ArrayForEach(pItem, pInf) { /* ����ڲ� */
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
*	ͨ��KE����Ϣ��ȡ����backtrace
*
* PARAMETERS
*	pKeyInf: ��������
*	Sz: pKeyInf����(��λ: �ֽ�)
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
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
*	����������ѯ����ֵ
*
* PARAMETERS
*	pType: �쳣����
*	pAccurate: ��ȷ��, ֻ��Ϊ"false"��"true"
*	pKeyInf: ��������
*
* RETURNS
*	< 0: ʧ��
*	0: δ�ҵ�
*	> 0: �ɹ�, ����eigen id
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
*	��ʾ������Cr��Ϣ
*
* PARAMETERS
*	fp: ��������ļ�
*	pCrId: Cr id, ΪNULL�򲻼�¼
*
* RETURNS
*	��
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
	if (LogEigenGetKeyInf(KeyInf, sizeof(KeyInf))) /* û������ */
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
