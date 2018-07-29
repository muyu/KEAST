/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	dwline.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/23
* Description: .debug_line解析(支持dwarf2~4)
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "string_res.h"
#include <module.h>
#include "dwarf_int.h"
#include "dwline.h"


typedef enum _DWLINE_NO_OPS
{
	DW_LNS_EXT_OP,
	DW_LNS_CPY,
	DW_LNS_ADV_ADDR,
	DW_LNS_ADV_LINE,
	DW_LNS_SET_FILE,
	DW_LNS_SET_COLN,
	DW_LNS_NEG_STMT,
	DW_LNS_SET_BB,
	DW_LNS_CADD_ADDR,
	DW_LNS_FADV_ADDR,
	DW_LNS_SET_PRO_END,
	DW_LNS_SET_EPIL_START,
	DW_LNS_SET_ISA,
} DWLINE_NO_OPS;

typedef enum _DWLINE_NO_EXT_OPS
{
	DW_LNE_NONE,
	DW_LNE_ESEQ,
	DW_LNE_SET_ADDR,
	DW_LNE_DEF_FILE,
	DW_LNE_SET_DIS,
	DW_LNE_LO_USER = 0x80,
	DW_LNE_HI_USER = 0xFF
} DWLINE_NO_EXT_OPS;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	对比两个dwl序列
*
* PARAMETERS
*	e1: seq pointer1
*	e2: seq pointer2
*
* RETURNS
*	-1: <
*	1: >
*	0: ==
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DwlSeqCmp(const void *e1, const void *e2)
{
	const DWL_SEQINF * const pSeq1 = e1, * const pSeq2 = e2;

	if (pSeq1->SAddr < pSeq2->SAddr)
		return -1;
	if (pSeq1->SAddr > pSeq2->SAddr)
		return 1;
	if (pSeq1->EAddr < pSeq2->EAddr)
		return 1;
	if (pSeq1->EAddr > pSeq2->EAddr)
		return -1;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	dwl序列排序
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DwlSeqSort(DWL_HDL *pDwlHdl)
{
	DWL_SEQINF * const pESeq = &pDwlHdl->pSeqs[pDwlHdl->Cnt];
	DWL_SEQINF *pSeq, *p;
	TADDR EAddr;

	qsort(pDwlHdl->pSeqs, pDwlHdl->Cnt, sizeof(*pDwlHdl->pSeqs), DwlSeqCmp);
	/* 结合有重叠的seq和移除嵌套的seq */
	EAddr = pDwlHdl->pSeqs[0].EAddr;
	for (pSeq = p = &pDwlHdl->pSeqs[1]; p < pESeq; p++) {
		if (p->SAddr < EAddr) {
			if (p->EAddr <= EAddr) { /* 移除嵌套的seq */
				if (p->EAddr == EAddr && p->SAddr == pSeq[-1].SAddr && p->pSSeq < pSeq[-1].pSSeq/* 相同时选择地址小的!!! */) {
					pSeq[-1].pSSeq = p->pSSeq;
					pSeq[-1].pHead = p->pHead;
				}
				continue;
			}
			p->SAddr = EAddr; /* 结合有重叠的seq */
		}
		EAddr = p->EAddr;
		if (p > pSeq) /* close up the gap */
			*pSeq = *p;
		pSeq++;
	}
	pDwlHdl->Cnt = pSeq - pDwlHdl->pSeqs;
	pDwlHdl->pSeqs = Realloc(pDwlHdl->pSeqs, sizeof(*pDwlHdl->pSeqs) * pDwlHdl->Cnt);
}

/*************************************************************************
* DESCRIPTION
*	获取line head信息
*
* PARAMETERS
*	pLineh: line head
*	ppSSeq: 返回op流起始地址, 为NULL表示该line head异常!
*	pMinInstLen: 返回MinInstLen
*	pLineRange: 返回LineRange
*	pOpBase: 返回OpBase
*	ppOpLenTbl: 返回pOpLenTbl
*
* RETURNS
*	下一个line head
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static const void *GetLineHead(const void *pLineh, const void **ppSSeq
  , unsigned *pMinInstLen, int *pLineBase, unsigned *pLineRange, unsigned *pOpBase, const unsigned char **ppOpLenTbl)
{
	const char *pOpBuf;
	const void *pEnd;
	unsigned Off;
/* 结构
	struct {
		TUINT/TSIZE Len;
		unsigned short Ver; // DWARF version
		TUINT/TSIZE HeadSz; //下面几个变量大小
		unsigned char MinInstLen;
		unsigned char MaxOpsPerInst; //DWARF4才有
		unsigned char DefIsStmt;
		char LineBase;
		unsigned char LineRange, OpBase, OpLenTbl[OpBase - 1];
		char IncDirs[], FileNames[];
	}
*/
#ifdef SST64
	if (OFFVAL_U4(pLineh, 0) != 0xFFFFFFFF) {
#endif
		pLineh = (char *)pLineh + 4;
		pEnd = (char *)pLineh + OFFVAL_U4(pLineh, -4);
		Off = 6;
		pOpBuf = (char *)pLineh + Off + OFFVAL_U4(pLineh, 2);
#ifdef SST64
	} else {
		pLineh = (char *)pLineh + 12;
		pEnd = (char *)pLineh + OFFVAL_U8(pLineh, -8);
		Off = 10;
		pOpBuf = (char *)pLineh + Off + OFFVAL_U8(pLineh, 2);
	}
#endif
	if (OFFVAL_U2(pLineh, 0) > 4/* DWARF2~4 */) {
		*ppSSeq = NULL;
		SSTERR(RS_SEG_VERSION, NULL, ".debug_line", OFFVAL_U2(pLineh, 0));
	} else {
		*pMinInstLen = OFFVAL_U1(pLineh, Off);
		if (OFFVAL_U2(pLineh, 0) == 4)
			Off++;
		*pLineBase = OFFVAL_S1(pLineh, Off + 2);
		*pLineRange = OFFVAL_U1(pLineh, Off + 3);
		*pOpBase = OFFVAL_U1(pLineh, Off + 4);
		*ppOpLenTbl = (unsigned char *)pLineh + Off + 5;
		while (pOpBuf > (char *)*ppOpLenTbl && pOpBuf[-1] != '\0') /* 纠正错误 */
			pOpBuf--;
		*ppSSeq = pOpBuf;
	}
	return pEnd;
}

/*************************************************************************
* DESCRIPTION
*	根据地址建立dwl句柄所在的序列
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	Addr: 地址
*
* RETURNS
*	NULL: 失败/找不到
*	其他: 序列
*
* LOCAL AFFECTED
*
*************************************************************************/
static void BuildSeqInfs(DWL_HDL *pDwlHdl)
{
#define SET_SADDR(Addr) { if (Flag) { Flag = 0; SAddr = (Addr); }}
	const char *p = pDwlHdl->pBase, * const pEnd = p + pDwlHdl->Size, *q;
	unsigned Op, OpBase = 0, MinInstLen = 0, LineRange = 0, MaxCnt = 0, Flag;
	const unsigned char *pOpLenTbl = NULL;
	const void *pHead, *pSSeq;
	DWL_SEQINF *pNewSeqs;
	TADDR SAddr, Addr;
	int LineBase;
	TSIZE Sz;

	if (pDwlHdl->Cnt)
		return;
	while (p < pEnd) {
		q = GetLineHead(pHead = p, &pSSeq, &MinInstLen, &LineBase, &LineRange, &OpBase, &pOpLenTbl);
		if (!pSSeq) {
			p = q;
			continue;
		}
		for (p = pSSeq; p < q; ) {
			pSSeq = p;
			Flag = 1;
			SAddr = Addr = 0;
			while (1) {
				Op = *(unsigned char *)p++;
				if (Op >= OpBase) { /* special operand */
					Addr += (Op - OpBase) / LineRange * MinInstLen;
					SET_SADDR(Addr);
					continue;
				}
				switch (Op) {
				case DW_LNS_EXT_OP:
					p = Getleb128(&Sz, p);
					switch (*(unsigned char *)p++) {
					case DW_LNE_ESEQ: SET_SADDR(Addr); goto _ESEQ;
					case DW_LNE_SET_ADDR: Addr = OFFVAL_ADDR(p, 0); p += sizeof(Addr); break;
					case DW_LNE_DEF_FILE: p = Getleb128(&Sz, p + strlen(p) + 1); p = Getleb128(&Sz, p); p = Getleb128(&Sz, p); break;
					case DW_LNE_SET_DIS: p = Getleb128(&Sz, p); break;
					default: SSTERR(RS_UNKNOWN_DWOP, NULL, ".debug_line", p[-1]); goto _FAIL;
					}
					break;
				case DW_LNS_CPY: SET_SADDR(Addr); break;
				case DW_LNS_ADV_ADDR: p = Getleb128(&Sz, p); Addr += (MinInstLen * Sz); break;
				case DW_LNS_ADV_LINE: case DW_LNS_SET_FILE: case DW_LNS_SET_COLN: p = Getleb128(&Sz, p); break;
				case DW_LNS_NEG_STMT: case DW_LNS_SET_BB: break;
				case DW_LNS_CADD_ADDR: Addr += (MinInstLen * ((255 - OpBase) / LineRange)); break;
				case DW_LNS_FADV_ADDR: Addr += OFFVAL_U2(p, 0); p += 2; break;
				default: /* 忽略未知的标准opcode */
					for (Op = pOpLenTbl[Op - 1]; Op > 0; Op--) {
						p = Getleb128(&Sz, p);
					}
					break;
				}
			}
_ESEQ:		if (!SAddr)
				continue;
			if (pDwlHdl->Cnt + 1 > MaxCnt) {
				MaxCnt = (MaxCnt * 2 + 512);
				pNewSeqs = Realloc(pDwlHdl->pSeqs, sizeof(*pDwlHdl->pSeqs) * MaxCnt);
				if (!pNewSeqs) {
					SSTWARN(RS_OOM);
					goto _FAIL2;
				}
				pDwlHdl->pSeqs = pNewSeqs;
			}
			pDwlHdl->pSeqs[pDwlHdl->Cnt].pSSeq = pSSeq;
			pDwlHdl->pSeqs[pDwlHdl->Cnt].pHead = pHead;
			pDwlHdl->pSeqs[pDwlHdl->Cnt].SAddr = SAddr;
			pDwlHdl->pSeqs[pDwlHdl->Cnt].EAddr = Addr;
			pDwlHdl->Cnt++;
		}
_FAIL:;
	}
_FAIL2:
	if (pDwlHdl->pSeqs)
		DwlSeqSort(pDwlHdl);
	else /* 表示已经加载! */
		pDwlHdl->Cnt = 1;
}

/*************************************************************************
* DESCRIPTION
*	根据路径索引获取路径
*
* PARAMETERS
*	pFs: 文件/目录信息
*	ppFEx/FExCnt: 额外文件名数组/个数, 个数为0时, ppFEx表示文件名
*	pPathIdx: 输入路径索引/失败时返回剩下的索引
*
* RETURNS
*	NULL: 失败/找不到
*	其他: 路径，不用时需调用Free()释放!!!
*
* LOCAL AFFECTED
*
*************************************************************************/
static char *GetDwlPathEx(const char *pFs, const char **ppFEx, unsigned FExCnt, unsigned *pPathIdx)
{
	unsigned PathIdx = *pPathIdx;
	const char *pFile, *pDir;
	char *pPath;
	TSIZE Sz;

	if (ppFEx && !FExCnt) {
		pFile = (char *)ppFEx;
		goto _FOUND;
	}
	for (pFile = pFs; *pFile != '\0'; pFile += strlen(pFile) + 1); /* 先跳过目录 */
	for (pFile++; *pFile != '\0'; PathIdx--) {
		if (!PathIdx)
			goto _FOUND;
		pFile = Getleb128(&Sz, pFile + strlen(pFile) + 1);
		pFile = Getleb128(&Sz, pFile);
		pFile = Getleb128(&Sz, pFile);
	}
	if (PathIdx >= FExCnt) {
		*pPathIdx = PathIdx;
		return NULL;
	}
	pFile = ppFEx[PathIdx];
_FOUND:
	if (pFile[0] == '/')
		goto _OUT;
	pDir = Getleb128(&Sz, pFile + strlen(pFile) + 1);
	for (PathIdx = 1, pDir = pFs; *pDir != '\0'; pDir += strlen(pDir) + 1, PathIdx++) { /* 查找对应目录 */
		if (PathIdx != (unsigned)Sz)
			continue;
		pFs = strstr(pDir, "/kernel"); /* 尝试剔除kernel目录 */
		if (pFs)
			pFs = strchr(pFs + 1, '/');
		pDir = (pFs ? pFs + 1 : pDir);
		pPath = Malloc(strlen(pFile) + strlen(pDir) + 2);
		if (pPath) {
			sprintf(pPath, "%s/%s", pDir, pFile);
			return pPath;
		}
		break;
	}
_OUT:
	return strdup(pFile);
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取序列所在的行信息
*
* PARAMETERS
*	pSeq: 序列
*	pLineNo: 返回行号
*	Addr: 地址, 包含THUMB bit
*
* RETURNS
*	NULL: 失败/找不到
*	其他: 所在路径
*
* LOCAL AFFECTED
*
*************************************************************************/
static char *GetLineInf(DWL_SEQINF *pSeq, TUINT *pLineNo, TADDR Addr)
{
#define RECORD_LINEINF(Adr, LNo, FIdx)  { if ((Adr) > Addr) goto _ESEQ; LineNo = (LNo); PathIdx = (FIdx); }
	unsigned tPathIdx, PathIdx, Op, OpBase = 0, FExCnt = 0, FExMCnt = 0, MinInstLen = 0, LineRange = 0;
	const char *p, *pEnd, *pFs, **ppFEx = NULL, **ppNewFEx;
	const unsigned char *pOpLenTbl = NULL;
	TUINT tLineNo, LineNo;
	char *pFile = NULL;
	const void *pSSeq;
	int LineBase = 0;
	TADDR tAddr;
	TSSIZE sSz;
	TSIZE Sz;

	pEnd = GetLineHead(pSeq->pHead, &pSSeq, &MinInstLen, &LineBase, &LineRange, &OpBase, &pOpLenTbl);
	pFs = (char *)&pOpLenTbl[OpBase - 1];
	p = pSeq->pSSeq;
	tLineNo = 1;
	LineNo = 0xFFFFFFFF;
	tPathIdx = PathIdx = 0;
	tAddr = 0;
	while (1) {
		Op = *(unsigned char *)p++;
		if (Op >= OpBase) { /* special operand */
			Op -= OpBase;
			tAddr  += Op / LineRange * MinInstLen;
			tLineNo += LineBase + (Op % LineRange);
			RECORD_LINEINF(tAddr, tLineNo, tPathIdx);
			continue;
		}
		switch (Op) {
		case DW_LNS_EXT_OP:
			p = Getleb128(&Sz, p);
			switch (*(unsigned char *)p++) {
			case DW_LNE_ESEQ: RECORD_LINEINF(tAddr, tLineNo, tPathIdx); goto _ESEQ;
			case DW_LNE_SET_ADDR: tAddr = OFFVAL_ADDR(p, 0); p += sizeof(tAddr); break;
			case DW_LNE_DEF_FILE:
				if (FExCnt + 1 < FExMCnt) {
					ppNewFEx = Realloc((void *)ppFEx, sizeof(*ppFEx) * (FExMCnt * 2 + 10));
					if (!ppNewFEx)
						break;
					ppFEx = ppNewFEx;
					FExMCnt = FExMCnt * 2 + 10;
				}
				ppFEx[FExCnt++] = p;
				p = Getleb128(&Sz, p + strlen(p) + 1); p = Getleb128(&Sz, p); p = Getleb128(&Sz, p);
				break;
			case DW_LNE_SET_DIS: p = Getleb128(&Sz, p); break;
			default:; /* BuildSeqInfs()已处理, 不会再遇到! */
			}
			break;
		case DW_LNS_CPY: RECORD_LINEINF(tAddr, tLineNo, tPathIdx); break;
		case DW_LNS_ADV_ADDR: p = Getleb128(&Sz, p); tAddr += (MinInstLen * Sz); break;
		case DW_LNS_ADV_LINE: p = Getsleb128(&sSz, p); tLineNo += (TINT)sSz; break;
		case DW_LNS_SET_FILE: p = Getleb128(&Sz, p); tPathIdx = (unsigned)(Sz - 1); break;
		case DW_LNS_SET_COLN: p = Getleb128(&Sz, p); break;
		case DW_LNS_NEG_STMT: case DW_LNS_SET_BB: break;
		case DW_LNS_CADD_ADDR: tAddr += (MinInstLen * ((255 - OpBase) / LineRange)); break;
		case DW_LNS_FADV_ADDR: tAddr += OFFVAL_U2(p, 0); p += 2; break;
		default:
			for (Op = pOpLenTbl[Op - 1]; Op > 0; Op--) {
				p = Getleb128(&Sz, p);
			}
			break;
		}
	}
_ESEQ:
	if (LineNo != 0xFFFFFFFF) {
		*pLineNo = LineNo;
		pFile = GetDwlPathEx(pFs, ppFEx, FExCnt, &PathIdx);
	}
	Free((void *)ppFEx);
	return pFile;
}

/*************************************************************************
* DESCRIPTION
*	根据路径索引获取路径
*
* PARAMETERS
*	pHead: line base
*	PathIdx: 路径索引
*
* RETURNS
*	NULL: 失败/找不到
*	其他: 路径，不用时需调用Free()释放!!!
*
* LOCAL AFFECTED
*
*************************************************************************/
static char *GetDwlFileInf(const void *pHead, unsigned PathIdx)
{
	unsigned Op, OpBase = 0, MinInstLen = 0, LineRange = 0;
	const unsigned char *pOpLenTbl = NULL;
	const char *p, *pEnd;
	const void *pSSeq;
	int LineBase;
	char *pFile;
	TSIZE Sz;

	pEnd = GetLineHead(pHead, &pSSeq, &MinInstLen, &LineBase, &LineRange, &OpBase, &pOpLenTbl);
	pFile = GetDwlPathEx((char *)&pOpLenTbl[OpBase - 1], NULL, 0, &PathIdx);
	if (pFile)
		return pFile;
	p = pSSeq;
	while (1) {
_START: Op = *(unsigned char *)p++;
		if (Op >= OpBase) /* special operand */
			continue;
		switch (Op) {
		case DW_LNS_EXT_OP:
			p = Getleb128(&Sz, p);
			switch (*(unsigned char *)p++) {
			case DW_LNE_ESEQ:
				if (p < pEnd) /* 没找着 */
					goto _START;
				return NULL;
			case DW_LNE_SET_ADDR: p += sizeof(TADDR); break;
			case DW_LNE_DEF_FILE:
				if (!PathIdx--)
					return GetDwlPathEx((char *)&pOpLenTbl[OpBase - 1], (const char **)p, 0, &PathIdx);
				p = Getleb128(&Sz, p + strlen(p) + 1); p = Getleb128(&Sz, p); p = Getleb128(&Sz, p);
				break;
			case DW_LNE_SET_DIS: p = Getleb128(&Sz, p); break;
			default:; /* BuildSeqInfs()已处理, 不会再遇到! */
			}
			break;
		case DW_LNS_ADV_ADDR: case DW_LNS_ADV_LINE: case DW_LNS_SET_FILE: case DW_LNS_SET_COLN: p = Getleb128(&Sz, p); break;
		case DW_LNS_CPY: case DW_LNS_NEG_STMT: case DW_LNS_SET_BB: case DW_LNS_CADD_ADDR: break;
		case DW_LNS_FADV_ADDR: p += 2; break;
		default:
			for (Op = pOpLenTbl[Op - 1]; Op > 0; Op--) {
				p = Getleb128(&Sz, p);
			}
			break;
		}
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取dwl句柄所在的序列
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	Addr: 地址, 包含THUMB bit
*
* RETURNS
*	NULL: 失败/找不到
*	其他: 序列
*
* LOCAL AFFECTED
*
*************************************************************************/
static DWL_SEQINF *SearchSeqInf(DWL_HDL *pDwlHdl, TADDR Addr)
{
	unsigned Low, High, Mid;
	DWL_SEQINF *pSeq = NULL;

	BuildSeqInfs(pDwlHdl);
	if (!pDwlHdl->pSeqs)
		return NULL;
	for (Low = 0, High = pDwlHdl->Cnt; Low < High; ) { /* 2分搜索 */
		Mid = (Low + High) >> 1;
		pSeq = &pDwlHdl->pSeqs[Mid];
		if (Addr < pSeq->SAddr)
			High = Mid;
		else if (Addr >= pSeq->EAddr)
			Low = Mid + 1;
		else
			break;
	}
	return (Addr >= pSeq->SAddr && Addr < pSeq->EAddr ? pSeq : NULL);
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据地址获取所在路径和行号
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	pLineNo: 返回行号
*	Addr: 地址, 包含THUMB bit
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DwlAddr2Line(DWL_HDL *pDwlHdl, TUINT *pLineNo, TADDR Addr)
{
	DWL_SEQINF *pSeq;

	pSeq = SearchSeqInf(pDwlHdl, Addr);
	return (pSeq ? GetLineInf(pSeq, pLineNo, Addr) : NULL);
}

/*************************************************************************
* DESCRIPTION
*	根据偏移和路径索引获取路径
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	LineOff: line偏移量
*	PathIdx: 路径索引
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DwlIdx2Path(DWL_HDL *pDwlHdl, TSIZE LineOff, unsigned PathIdx)
{
	return GetDwlFileInf((char *)pDwlHdl->pBase + LineOff, PathIdx - 1);
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwl句柄
*
* PARAMETERS
*	pDwlHdl: 返回dwl句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_line偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwl句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWL_HDL *GetDwlHdl(DWL_HDL *pDwlHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwlHdl->Size = Size;
	pDwlHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwlHdl->pBase)
		goto _ERR1;
	return pDwlHdl;
_ERR1:
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_line");
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	释放dwl句柄
*
* PARAMETERS
*	pDwlHdl: dwl句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwlHdl(DWL_HDL *pDwlHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwlHdl->pBase, pDwlHdl->Size);
	Free(pDwlHdl->pSeqs);
}
