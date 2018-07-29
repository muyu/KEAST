/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwloc.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_loc解析(支持dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include <module.h>
#include "dwloc.h"


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据地址和loc偏移量获取expr buffer
*
* PARAMETERS
*	pDwlcHdl: dwlc句柄
*	pSize: 返回buffer大小
*	LocOff: loc偏移量
*	Addr: 地址, 已去除基址, 包含THUMB bit
*
* RETURNS
*	NULL: 失败
*	其他: expr buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *DwlcGetExpr(const DWLC_HDL *pDwlcHdl, TSIZE *pSize, TSIZE LocOff, TADDR Addr)
{
	const TADDR *pAddrAry = (void *)((char *)pDwlcHdl->pBase + LocOff);
	unsigned short Len;

	for (; pAddrAry[0] && pAddrAry[1]; pAddrAry = (void *)((char *)&pAddrAry[2] + 2 + Len)) {
		if (pAddrAry[0] == ~(TADDR)0) /* base address!!! */
			DBGERR("异常.debug_loc地址: "TADDRFMT"\n", pAddrAry[0]);
		Len = OFFVAL_U2(&pAddrAry[2], 0);
		if (Addr >= pAddrAry[0] && Addr < pAddrAry[1]) {
			*pSize = Len;
			return (char *)&pAddrAry[2] + 2;
		}
	}
	return NULL;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwlc句柄
*
* PARAMETERS
*	pDwlcHdl: 返回dwlc句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_loc偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwlc句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWLC_HDL *GetDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwlcHdl->Size = Size;
	pDwlcHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwlcHdl->pBase)
		goto _ERR1;
	return pDwlcHdl;
_ERR1:
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_loc");
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	释放dwlc句柄
*
* PARAMETERS
*	pDwlcHdl: dwlc句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwlcHdl(DWLC_HDL *pDwlcHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwlcHdl->pBase, pDwlcHdl->Size);
}
