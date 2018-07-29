/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwaranges.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_aranges解析(支持dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include <module.h>
#include "dwaranges.h"


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取aranges head信息
*
* PARAMETERS
*	pARgeh: aranges head
*	ppAddrAry: 返回地址列表, 为NULL表示该aranges head异常!
*	pCuOff: 返回cu off
*
* RETURNS
*	下一个aranges head
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static const void *GetARgeHead(const void *pARgeh, const TADDR **ppAddrAry, TSIZE *pCuOff)
{
	unsigned Ver, PtrOff;
	const void *pEnd;
/* 结构
	struct {
		TUINT/TSIZE Len;
		unsigned short Ver; // DWARF version
		TUINT/TSIZE CuOff; // cu offset
		unsigned char PtrSz, SegSz; // 指针宽度/segment宽度, 4/8字节
	}
*/
#ifdef SST64
	if (OFFVAL_U4(pARgeh, 0) != 0xFFFFFFFF) {
#endif
		pEnd = (char *)pARgeh + 4 + OFFVAL_U4(pARgeh, 0);
		Ver = OFFVAL_U2(pARgeh, 4);
		*pCuOff = OFFVAL_U4(pARgeh, 6);
		PtrOff = 10;
#ifdef SST64
	} else {
		pEnd = (char *)pARgeh + 12 + OFFVAL_U8(pARgeh, 4);
		*pCuOff = OFFVAL_U8(pARgeh, 14);
		Ver = OFFVAL_U2(pARgeh, 12);
		PtrOff = 22;
	}
#endif
	if (Ver != 2/* DWARF2~5 */ || OFFVAL_U2(pARgeh, PtrOff) != sizeof(TADDR)) {
		*ppAddrAry = NULL;
		SSTERR(RS_SEG_VERSION, NULL, ".debug_aranges", Ver|(OFFVAL_U2(pARgeh, PtrOff) << 16));
	} else {
		*ppAddrAry = (void *)((char *)pARgeh + ((PtrOff + 2 + 2 * sizeof(TADDR) - 1)&~(2 * sizeof(TADDR) - 1))/* 对齐 */);
	}
	return pEnd;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据地址获取cu off
*
* PARAMETERS
*	pDwarHdl: dwar句柄
*	Addr: 地址, 包含THUMB bit
*
* RETURNS
*	~(TSIZE)0: 没有对应的cu
*	其他: cu off
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DwarGetCuOff(const DWAR_HDL *pDwarHdl, TADDR Addr)
{
	const void *pARgeh, * const pEnd = (char *)pDwarHdl->pBase + pDwarHdl->Size;
	const TADDR *pAddrAry;
	TSIZE CuOff;

	for (pARgeh = pDwarHdl->pBase; (void *)pARgeh < pEnd; ) {
		pARgeh = GetARgeHead(pARgeh, &pAddrAry, &CuOff);
		if (!pAddrAry)
			continue;
		while (pAddrAry[1]) {
			if (Addr - pAddrAry[0] < pAddrAry[1])
				return CuOff;
			pAddrAry += 2;
		}
	}
	return ~(TSIZE)0;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwar句柄
*
* PARAMETERS
*	pDwarHdl: 返回dwar句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_aranges偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwar句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWAR_HDL *GetDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwarHdl->Size = Size;
	pDwarHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwarHdl->pBase)
		goto _ERR1;
	return pDwarHdl;
_ERR1:
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_aranges");
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	释放dwar句柄
*
* PARAMETERS
*	pDwarHdl: dwar句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwarHdl(DWAR_HDL *pDwarHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwarHdl->pBase, pDwarHdl->Size);
}
