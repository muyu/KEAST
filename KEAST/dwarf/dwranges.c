/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwranges.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/9
* Description: .debug_ranges解析(支持dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include "dwranges.h"


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	指定地址是否在ranges里
*
* PARAMETERS
*	pDwrHdl: dwr句柄
*	RgeOff: range偏移量
*	Addr: 地址, 已去除基址, 包含THUMB bit
*
* RETURNS
*	~(TADDR)0: 不在
*	其他: 在并返回首地址
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR DwrCheckAddr(const DWR_HDL *pDwrHdl, TSIZE RgeOff, TADDR Addr)
{
	const TADDR *pAddrAry = (void *)((char *)pDwrHdl->pBase + RgeOff);
	int i;

	for (i = 0; pAddrAry[i]|pAddrAry[i + 1]; i += 2) {
		if (pAddrAry[i] == ~(TADDR)0) /* base address!!! */
			DBGERR("异常.debug_ranges地址: "TADDRFMT"\n", pAddrAry[i]);
		if (Addr >= pAddrAry[i] && Addr < pAddrAry[i + 1])
			return pAddrAry[0];
	}
	return ~(TADDR)0;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dwr句柄
*
* PARAMETERS
*	pDwrHdl: 返回dwr句柄
*	pFpHdl: fp句柄
*	FOff/Size: .debug_ranges偏移地址/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: dwr句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWR_HDL *GetDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl, TSIZE FOff, TSIZE Size)
{
	pDwrHdl->Size = Size;
	pDwrHdl->pBase = FpMapRo(pFpHdl, FOff, Size);
	if (!pDwrHdl->pBase)
		goto _ERR1;
	return pDwrHdl;
_ERR1:
	SSTWARN(RS_FILE_MAP, pFpHdl->pPath, ".debug_ranges");
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	释放dwr句柄
*
* PARAMETERS
*	pDwrHdl: dwr句柄
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwrHdl(DWR_HDL *pDwrHdl, FP_HDL *pFpHdl)
{
	FpUnmap(pFpHdl, pDwrHdl->pBase, pDwrHdl->Size);
}
