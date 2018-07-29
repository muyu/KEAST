/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	module.c
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: ģ��ͨ�ú�������
**********************************************************************************/
#include <string.h>
#include <stdio.h> /* just for sprintf() api */
#include "os.h"
#include "string_res.h"
#include <mrdump.h>
#include <module.h>


#define LOAD_NUMER  3 /* 75% */
#define LOAD_DENOM  4

typedef struct _HASH_UNIT
{
	unsigned HashVal;
	void *pData;
} HASH_UNIT;

typedef struct _HASH_TBL
{
	unsigned TblSize, UsedCnt;
	void (*pFreeFunc)(void *pData);
	HASH_UNIT *pEntries;
} HASH_TBL;


const unsigned *cpu_online_mask;
static TSIZE PerCpuOff[MAX_CORE];


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�������2��n�η�
*
* PARAMETERS
*	Val: ֵ
*
* RETURNS
*	2��n�η�
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static size_t RoundUpPower2(size_t Val)
{
	Val--;
	Val |= Val >> 1;
	Val |= Val >> 2;
	Val |= Val >> 4;
	Val |= Val >> 8;
	Val |= Val >> 16;
	Val++;
	return Val;
}

/*************************************************************************
* DESCRIPTION
*	���Ź�ϣ��
*
* PARAMETERS
*	pHashTbl: ��ϣ����
*	NewSize: �µĴ�С
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static int ResizeHtbl(HASH_TBL *pHashTbl, unsigned NewSize)
{
	HASH_UNIT *pOldEs, *pNewEs;
	unsigned i, j;
	void *pData;

	pNewEs = Calloc(NewSize * sizeof(*pNewEs)); /* ����2�� */
	if (pNewEs) {
		NewSize--;
		pOldEs = pHashTbl->pEntries;
		for (i = 0; i < pHashTbl->TblSize; i++) {
			pData = pOldEs[i].pData;
			if (pData) {
				j = pOldEs[i].HashVal&NewSize;
				while (pNewEs[j].pData) {
					j = (j + 1)&NewSize;
				}
				pNewEs[j] = pOldEs[i];
			}
		}
		Free(pOldEs);
		pHashTbl->pEntries = pNewEs;
		pHashTbl->TblSize  = NewSize + 1;
	}
	return !pNewEs;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	����ģ���쳣��Ϣ���ļ�
*
* PARAMETERS
*	fp: �ļ����
*	ModErr: ģ���쳣��Ϣ
*	pModName: ģ������
*
* RETURNS
*	0: ����
*	1: �쳣
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DispModErr(FILE *fp, unsigned ModErr, const char *pModName)
{
#ifdef _DEBUG
	if (ModErr >= MOD_SYS_MAX && !MOD_STATUS(ModErr))
		DBGERR("�����ģ���쳣ֵ: %X\n", ModErr);
#endif
	switch (MOD_STATUS(ModErr)) {
	case MOD_OK:
	case MOD_NOT_SUPPORT:
	case MOD_OOM:		return 0;
	case MOD_ADDR:	   LOG(fp, RS_GET_ADDR,  pModName); break;
	case MOD_CORRUPTED:  LOG(fp, RS_CORRUPTED, pModName); break;
	default:;
	}
	LOGC(fp, "\n\n");
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	���һ�����ϣ��
*
* PARAMETERS
*	pHtblHdl: ��ϣ����
*	HashVal: ��ϣֵ
*	pData: ���һ���������, ����ΪNULL!!!
*	pCmpFunc: �ȽϺ���, ����0��ʾһ��, ����һ��
*		pTblData: ��ϣ������
*		pData: �Ա�����
*	DoAdd: �Ƿ����
*
* RETURNS
*	NULL: ʧ��
*	����: ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *HtblLookup(HTBL_HDL *pHtblHdl, unsigned HashVal, void *pData, int (*pCmpFunc)(void *pTblData, void *pData), int DoAdd)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;
	HASH_UNIT *pEntry, *pEnd;
	void *rt = NULL;

	pEntry = &pHashTbl->pEntries[HashVal&(pHashTbl->TblSize - 1)];
	pEnd   = &pHashTbl->pEntries[pHashTbl->TblSize];
	while (pEntry->pData) {
		if (pEntry->HashVal == HashVal && !pCmpFunc(pEntry->pData, pData)) /* match */
			break;
		if (++pEntry == pEnd) /* ��ת */
			pEntry = pHashTbl->pEntries;
	}
	if (!pEntry->pData) {
		if (DoAdd) {
			pEntry->HashVal = HashVal;
			pEntry->pData = rt = pData;
			pHashTbl->UsedCnt++;
			if (pHashTbl->UsedCnt * LOAD_DENOM >= pHashTbl->TblSize * LOAD_NUMER && ResizeHtbl(pHashTbl, pHashTbl->TblSize << 1)) { /* ����2�� */
				pHashTbl->UsedCnt--;
				pEntry->HashVal = 0;
				pEntry->pData = rt = NULL;
			}
		}
	} else {
		rt = pEntry->pData;
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	�ûص���������ɨ��һ��hash��
*
* PARAMETERS
*	pHtblHdl: hash table handle
*
* RETURNS
*	hash���¼�ĸ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
unsigned HtblGetUsedCnt(HTBL_HDL *pHtblHdl)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;

	return pHashTbl->UsedCnt;
}

/*************************************************************************
* DESCRIPTION
*	����hash��
*
* PARAMETERS
*	pHtblHdl: ��ϣ����
*	pFunc: �ص�����
*		pData: ����
*		pArg: arg
*	pArg: ����Ĳ���
*
* RETURNS
*	0: �ɹ�
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int HtblForEach(HTBL_HDL *pHtblHdl, HTBL_CB pFunc, void *pArg)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;
	HASH_UNIT *pEntry;
	unsigned i;
	int rt = 0;

	for (i = 0; i < pHashTbl->TblSize; i++) {
		pEntry = &pHashTbl->pEntries[i];
		if (pEntry->pData) {
			rt = pFunc(pEntry->pData, pArg);
			if (rt)
				break;
		}
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	������ϣ��
*
* PARAMETERS
*	Size: ��ʼ��С
*	pFreeFunc: �ͷź���(�ͷŹ�ϣ��ʱ����), ����ΪNULL
*
* RETURNS
*	NULL: ʧ��
*	����: ��ϣ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
HTBL_HDL *GetHtblHdl(unsigned Size, void (*pFreeFunc)(void *pData))
{
	HASH_TBL *pHashTbl;

	Size = RoundUpPower2(Size);
	pHashTbl = Malloc(sizeof(*pHashTbl));
	if (pHashTbl) {
		pHashTbl->pEntries = Malloc(Size * sizeof(*pHashTbl->pEntries));
		if (pHashTbl->pEntries) {
			memset(pHashTbl->pEntries, 0, Size * sizeof(*pHashTbl->pEntries));
			pHashTbl->UsedCnt   = 0;
			pHashTbl->TblSize   = Size;
			pHashTbl->pFreeFunc = pFreeFunc;
		} else {
			Free(pHashTbl);
			pHashTbl = NULL;
		}
	}
	return (HTBL_HDL *)pHashTbl;
}

/*************************************************************************
* DESCRIPTION
*	�ͷŹ�ϣ��, ��ÿ�����ݵ����ͷź���(�����ע��Ļ�)
*
* PARAMETERS
*	HtblHdl: ��ϣ����
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutHtblHdl(HTBL_HDL *pHtblHdl)
{
	HASH_TBL *pHashTbl = (HASH_TBL *)pHtblHdl;
	void (*pFreeFunc)(void *pData);
	HASH_UNIT *pEntry;
	unsigned Size;

	pFreeFunc = pHashTbl->pFreeFunc;
	if (pFreeFunc) {
		for (pEntry = pHashTbl->pEntries, Size = pHashTbl->UsedCnt; Size > 0; pEntry++) {
			if (pEntry->pData) {
				pFreeFunc(pEntry->pData);
				Size--;
			}
		}
	}
	Free(pHashTbl->pEntries);
	Free(pHashTbl);
}

/*************************************************************************
* DESCRIPTION
*	��ȡָ��cpu��per cpu������ַ
*
* PARAMETERS
*	PerCpuBase: per cpu������ַ
*	CpuId: CPU id
*
* RETURNS
*	0: ʧ��
*	����: ָ��cpu��per cpu������ַ
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR PerCpuAddr(TADDR PerCpuBase, TUINT CpuId)
{
	return PerCpuOff[CpuId] ? PerCpuBase + PerCpuOff[CpuId] : 0;
}

/*************************************************************************
* DESCRIPTION
*	��������
*
* PARAMETERS
*	ListAddr: list_head��ַ
*	ListOff: �ṹ�������list_head��ƫ����
*	pFunc: �ص�����
*		HdlAddr: �����ַ
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: �����쳣
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachList(TADDR ListAddr, TUINT ListOff, int (*pFunc)(TADDR HdlAddr, void *pArg), void *pArg)
{
	const TADDR ListHeadAddr = ListAddr;
	int rt = 0;

	do {
		if (DumpMem(ListAddr, &ListAddr, sizeof(ListAddr)))
			return 1;
		if (ListAddr == ListHeadAddr)
			break;
		rt = pFunc(ListAddr - ListOff, pArg);
	} while (!rt);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	���λΪ1��bitλ��(0~32)
*	0����Ϊ0, 1����Ϊ1, 3����Ϊ2
*
* PARAMETERS
*	Mask: ֵ
*
* RETURNS
*	���λΪ1��bitλ��(0~32)
*
* GLOBAL AFFECTED
*
*************************************************************************/
TUINT Fhs(TUINT Mask)
{
	static const char t[16] = {-28, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4};
	TUINT bit = 0;

	if (!(Mask&0xFFFF)) {
		bit += 16;
		Mask >>= 16;
	}
	if (!(Mask&0xFF)) {
		bit += 8;
		Mask >>= 8;
	}
	if (!(Mask&0xF)) {
		bit += 4;
		Mask >>= 4;
	}
	return (bit + t[Mask&0xF]);
}

/*************************************************************************
* DESCRIPTION
*	��ʾ����(�����ǵ�ַ)��Ӧ�ķ���
*
* PARAMETERS
*	pDispFunc: ��ʾ�ص�����
*	fp: �ļ����
*	Val: ����
*
* RETURNS
*	����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ShowSymVal(int (*pDispFunc)(FILE *fp, const char *pFmt, ...), FILE *fp, TADDR Val)
{
	const char *pName;
	SYM_HDL *pSymHdl;
	TADDR SAddr;
	int Sz;

	pSymHdl = GetSymHdlByAddr(Val);
	if (pSymHdl) {
		pName = SymAddr2Name(pSymHdl, &SAddr, Val, TALLVAR); /* �Ƿ��Ǳ���? */
		if (pName) {
			Sz = pDispFunc(fp, "%s", pName);
			if (Val != SAddr)
				Sz += pDispFunc(fp, " + %d", (size_t)(Val - SAddr));
			return Sz;
		}
		pName = SymAddr2Name(pSymHdl, &SAddr, Val, TALLFUN); /* �Ƿ��Ǻ���? */
		if (pName && SAddr == Val)
			return (pName == pUndFuncName ? pDispFunc(fp, TADDRFMT"()", Val) : pDispFunc(fp, "%s()", pName));
	}
	return Val + 0x8000 <= 0x10000/* [-32767, 32768] */ ? pDispFunc(fp, "%d", (int)Val) : pDispFunc(fp, TADDRFMT, Val);
}

/*************************************************************************
* DESCRIPTION
*	�Ӹ����ַ��������ַ�������ȡ���ַ���
*
* PARAMETERS
*	ppStr: �ַ���ָ���ַ���ᱻ����
*	cc: �����ַ�
*
* RETURNS
*	���ַ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *GetSubStr(char **ppStr, char cc)
{
	const char *tok = "";
	char *s;
	int c;

	s = *ppStr;
	if (s) {
		while (*s == cc)
			s++;
		if (*s) {
			tok = s;
			do {
				c = *s++;
				if (c == cc) {
					s[-1] = '\0';
					*ppStr = s;
					return tok;
				}
			} while (c);
		}
		*ppStr = NULL;
	}
	return tok;
}

/*************************************************************************
* DESCRIPTION
*	�����ļ�
*
* PARAMETERS
*	pDstFile: Ŀ���ļ�
*	pSrcFile: Դ�ļ�
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int CCpyFile(const char *pDstFile, const char *pSrcFile)
{
	char Data[32768];
	FILE *fp, *tp;
	int rt = 1;
	size_t n;

	fp = fopen(pSrcFile, "rb");
	if (!fp)
		return rt;
	tp = fopen(pDstFile, "wb");
	if (!tp)
		goto _FAIL;
	while ((n = fread(Data, 1, sizeof(Data), fp)) > 0) {
		if (fwrite(Data, 1, n, tp) != n)
			goto _FAIL2;
	}
	rt = 0;
_FAIL2:
	fclose(tp);
_FAIL:
	fclose(fp);
	return rt;
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��ģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: ģ��ر�
*	1: ģ�����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ModuleInit(void)
{
	TADDR PerCpuOffAddr;

	PerCpuOffAddr = SymName2Addr(pVmSHdl, NULL, "__per_cpu_offset", TGLBVAR);
	if (PerCpuOffAddr)
		DumpMem(PerCpuOffAddr, PerCpuOff, sizeof(PerCpuOff));
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	ע��ģ��
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
void ModuleDeInit(void)
{
	
}
