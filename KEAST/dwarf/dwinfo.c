/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwinfo.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/6
* Description: .debug_info解析(支持dwarf2/3)
**********************************************************************************/
#include <string.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include "dwarf_int.h"
#include "dwabbrev.h"
#include "dwaranges.h"
#include "dwranges.h"
#include "dwloc.h"
#include "dwline.h"
#include "dwinfo.h"

#if defined(__MINGW32__)
#include <stdint.h>   /*For define uintptr_t*/
#endif

typedef enum _DW_AT
{
	DW_AT_sibling = 0x01,
	DW_AT_location = 0x02,
	DW_AT_name = 0x03,
	DW_AT_ordering = 0x09,
	DW_AT_subscr_data = 0x0a,
	DW_AT_byte_size = 0x0b,
	DW_AT_bit_offset = 0x0c,
	DW_AT_bit_size = 0x0d,
	DW_AT_element_list = 0x0f,
	DW_AT_stmt_list = 0x10,
	DW_AT_low_pc = 0x11,
	DW_AT_high_pc = 0x12,
	DW_AT_language = 0x13,
	DW_AT_member = 0x14,
	DW_AT_discr = 0x15,
	DW_AT_discr_value = 0x16,
	DW_AT_visibility = 0x17,
	DW_AT_import = 0x18,
	DW_AT_string_length = 0x19,
	DW_AT_common_reference = 0x1a,
	DW_AT_comp_dir = 0x1b,
	DW_AT_const_value = 0x1c,
	DW_AT_containing_type = 0x1d,
	DW_AT_default_value = 0x1e,
	DW_AT_inline = 0x20,
	DW_AT_is_optional = 0x21,
	DW_AT_lower_bound = 0x22,
	DW_AT_producer = 0x25,
	DW_AT_prototyped = 0x27,
	DW_AT_return_addr = 0x2a,
	DW_AT_start_scope = 0x2c,
	DW_AT_bit_stride = 0x2e,
	DW_AT_upper_bound = 0x2f,
	DW_AT_abstract_origin = 0x31,
	DW_AT_accessibility = 0x32,
	DW_AT_address_class = 0x33,
	DW_AT_artificial = 0x34,
	DW_AT_base_types = 0x35,
	DW_AT_calling_convention = 0x36,
	DW_AT_count = 0x37,
	DW_AT_data_member_location = 0x38,
	DW_AT_decl_column = 0x39,
	DW_AT_decl_file = 0x3a,
	DW_AT_decl_line = 0x3b,
	DW_AT_declaration = 0x3c,
	DW_AT_discr_list = 0x3d,
	DW_AT_encoding = 0x3e,
	DW_AT_external = 0x3f,
	DW_AT_frame_base = 0x40,
	DW_AT_friend = 0x41,
	DW_AT_identifier_case = 0x42,
	DW_AT_macro_info = 0x43,
	DW_AT_namelist_items = 0x44,
	DW_AT_priority = 0x45,
	DW_AT_segment = 0x46,
	DW_AT_specification = 0x47,
	DW_AT_static_link = 0x48,
	DW_AT_type = 0x49,
	DW_AT_use_location = 0x4a,
	DW_AT_variable_parameter = 0x4b,
	DW_AT_virtuality = 0x4c,
	DW_AT_vtable_elem_location = 0x4d,
	/* DWARF 3 values.  */
	DW_AT_allocated	 = 0x4e,
	DW_AT_associated	= 0x4f,
	DW_AT_data_location = 0x50,
	DW_AT_byte_stride   = 0x51,
	DW_AT_entry_pc	  = 0x52,
	DW_AT_use_UTF8	  = 0x53,
	DW_AT_extension	 = 0x54,
	DW_AT_ranges		= 0x55,
	DW_AT_trampoline	= 0x56,
	DW_AT_call_column   = 0x57,
	DW_AT_call_file	 = 0x58,
	DW_AT_call_line	 = 0x59,
	DW_AT_description   = 0x5a,
	DW_AT_binary_scale  = 0x5b,
	DW_AT_decimal_scale = 0x5c,
	DW_AT_small		 = 0x5d,
	DW_AT_decimal_sign  = 0x5e,
	DW_AT_digit_count   = 0x5f,
	DW_AT_picture_string = 0x60,
	DW_AT_mutable	   = 0x61,
	DW_AT_threads_scaled = 0x62,
	DW_AT_explicit	  = 0x63,
	DW_AT_object_pointer = 0x64,
	DW_AT_endianity	 = 0x65,
	DW_AT_elemental	 = 0x66,
	DW_AT_pure		  = 0x67,
	DW_AT_recursive	 = 0x68,
	/* DWARF 4.  */
	DW_AT_signature	   = 0x69,
	DW_AT_main_subprogram = 0x6a,
	DW_AT_data_bit_offset = 0x6b,
	DW_AT_const_expr	  = 0x6c,
	DW_AT_enum_class	  = 0x6d,
	DW_AT_linkage_name	= 0x6e,

	DW_AT_lo_user = 0x2000,
	DW_AT_MIPS_linkage_name = 0x2007, /* MIPS/SGI, GNU, and others. */
	DW_AT_hi_user = 0x3fff,
} DW_AT;

typedef enum _DW_FORM
{
	DW_FORM_none,
	DW_FORM_addr,
	DW_FORM_unused1,
	DW_FORM_block2, DW_FORM_block4,
	DW_FORM_data2, DW_FORM_data4, DW_FORM_data8,
	DW_FORM_string,
	DW_FORM_block, DW_FORM_block1,
	DW_FORM_data1,
	DW_FORM_flag,
	DW_FORM_sdata,
	DW_FORM_strp, /* 指向.debug_str */
	DW_FORM_udata,
	DW_FORM_ref_addr, /* 0x10 */
	DW_FORM_ref1, DW_FORM_ref2, DW_FORM_ref4, DW_FORM_ref8, DW_FORM_ref_udata,
	DW_FORM_indirect,
	/* DWARF 4 */
	DW_FORM_sec_offset,
	DW_FORM_exprloc,
	DW_FORM_flag_present,
	/* DWARF5 */
	DW_FORM_strx,
	DW_FORM_addrx,
	DW_FORM_ref_sup,
	DW_FORM_strp_sup,
	DW_FORM_data16,
	DW_FORM_unused2,
	/* DWARF4 */
	DW_FORM_ref_sig8, /* 0x20 */
} DW_FORM;

typedef struct _DWI_INT
{
	FP_HDL *pFpHdl;
	const void *pInfBase, *pStrsBase;
	TSIZE InfSz, StrsSz;
	DWA_HDL DwaHdl;
	DWR_HDL DwrHdl;
	DWLC_HDL DwlcHdl;
	DWL_HDL DwlHdl;
	DWAR_HDL DwarHdl; /* 仅支持函数地址查找 */
} DWI_INT;

static int AbbIsLen8B;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取cu head信息
*
* PARAMETERS
*	pCuBase: cu head
*	pDieInf: 返回cu die信息, 设置pCuBase/CuFlag/pData, pDieInf->pData为NULL表示该cu head异常!
*	pCuAbbOff: 返回cu abboff
*
* RETURNS
*	下一个cu head
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static const void *GetCuHead(const void *pCuBase, DIE_HDL *pDieInf, TSIZE *pCuAbbOff)
{
#pragma pack(1)
	const struct
	{
		TUINT Len;
		unsigned short Ver; /* DWARF version */
		TUINT AbbOff; /* abbrev offset */
		unsigned char PtrSz; /* 指针宽度, 4/8字节 */
	} *pCuh32 = pDieInf->pCuBase = pCuBase;
#pragma pack()

#ifdef SST64
	if (pCuh32->Len != ~(TUINT)0) {
#endif
		if (pCuh32->Ver > 4 || pCuh32->PtrSz != sizeof(TADDR)) {
			pDieInf->pData = NULL;
			SSTERR(RS_SEG_VERSION, NULL, ".debug_info", pCuh32->Ver|(pCuh32->PtrSz << 16));
		} else {
			pDieInf->pData = pCuh32 + 1;
			pDieInf->CuFlag = pCuh32->Ver;
			*pCuAbbOff = pCuh32->AbbOff;
		}
		return (char *)pCuh32 + pCuh32->Len + sizeof(pCuh32->Len);
#ifdef SST64
	} else {
#pragma pack(1)
		const struct
		{
			TSIZE Len;
			unsigned short Ver;
			TSIZE AbbOff;
			unsigned char PtrSz;
		} *pCuh64 = (void *)((char *)pCuh32 + sizeof(pCuh32->Len));
#pragma pack()

		if (pCuh64->Ver > 4 || pCuh64->PtrSz != sizeof(TADDR)) {
			pDieInf->pData = NULL;
			SSTERR(RS_SEG_VERSION, NULL, ".debug_info", pCuh64->Ver|(pCuh64->PtrSz << 16));
		} else {
			pDieInf->pData = pCuh64 + 1;
			pDieInf->CuFlag = pCuh64->Ver|CU_8B_LEN;
			*pCuAbbOff = pCuh64->AbbOff;
		}
		return (char *)pCuh64 + pCuh64->Len + sizeof(pCuh64->Len);
	}
#endif
}

/*************************************************************************
* DESCRIPTION
*	建立die句柄
*
* PARAMETERS
*	pDwi: dwi句柄
*	pDieHdl: 返回cu die句柄, 即使返回NULL也会更新pData, DieGetSiblingDieHdl()函数会用到!!!
*	pCuBase: cu head
*	pData: die buffer
*
* RETURNS
*	NULL: 失败/结束
*	其他: die句柄
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static DIE_HDL *BuildDieHdl(const DWI_INT *pDwi, DIE_HDL *pDieHdl, const void *pCuBase, const void *pData)
{
	TSIZE CuAbbOff = 0, AbbNum;

	pDieHdl->pData = pData = Getleb128(&AbbNum, pData);
	if (!AbbNum) /* 结束 */
		return NULL;
	GetCuHead(pCuBase, pDieHdl, &CuAbbOff);
	pDieHdl->pAttr = DwaGetAttrList(&pDwi->DwaHdl, &pDieHdl->Tag, CuAbbOff, AbbNum);
	if (!pDieHdl->pAttr) { /* 失败 */
		DBGERR("build die失败, %s: %X\n", pDwi->pFpHdl->pPath, (uintptr_t)pData - (uintptr_t)pDwi->pInfBase - 1);
		return NULL;
	}
	pDieHdl->pData = pData;
	pDieHdl->pDwiHdl = (DWI_HDL *)pDwi;
	return pDieHdl;
}

/*************************************************************************
* DESCRIPTION
*	跳过这个属性
*
* PARAMETERS
*	Form: 属性类型
*	pData: data buffer
*
* RETURNS
*	data buffer
*
* LOCAL AFFECTED
*
*************************************************************************/
static const void *SkipAttr(TSIZE Form, const void *pData)
{
_START:
	switch ((DW_FORM)Form) {
	case DW_FORM_ref1: case DW_FORM_flag: case DW_FORM_data1: return (char *)pData + 1;
	case DW_FORM_ref2: case DW_FORM_data2: return (char *)pData + 2;
	case DW_FORM_ref4: case DW_FORM_data4: return (char *)pData + 4;
	case DW_FORM_addr: return (char *)pData + sizeof(TADDR);
	case DW_FORM_ref_sig8: case DW_FORM_ref8: case DW_FORM_data8: return (char *)pData + 8;
	case DW_FORM_ref_addr: case DW_FORM_strp: case DW_FORM_sec_offset: return (char *)pData + (AbbIsLen8B ? 8 : 4);
	case DW_FORM_sdata: case DW_FORM_ref_udata: case DW_FORM_udata: return Getleb128(&Form, pData);
	case DW_FORM_block: case DW_FORM_exprloc: pData = Getleb128(&Form, pData); return (char *)pData + Form;
	case DW_FORM_string: return (char *)pData + strlen((char *)pData) + 1;
	case DW_FORM_block1: return (char *)pData + OFFVAL_U1(pData, 0) + 1;
	case DW_FORM_block2: return (char *)pData + OFFVAL_U2(pData, 0) + 2;
	case DW_FORM_block4: return (char *)pData + OFFVAL_U4(pData, 0) + 4;
	case DW_FORM_indirect: pData = Getleb128(&Form, pData); goto _START;
	case 0/* 空的 */: case DW_FORM_flag_present: break;
	default: SSTERR(RS_UNKNOWN_TYPE, "Form", (DW_FORM)Form);
	}
	return pData;
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取目标地址
*
* PARAMETERS
*	Form: 数据类型
*	pData: 数据buffer
*	SAddr: 起始地址, ~(TADDR)0表示无起始地址
*
* RETURNS
*	~(TADDR)0: 失败
*	其他: 目标地址
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR GetTarAddr(DW_FORM Form, const void *pData, TADDR SAddr)
{
	switch (Form) {
	case DW_FORM_addr:
		return OFFVAL_ADDR(pData, 0);
	case DW_FORM_data4:
		if (SAddr != ~(TADDR)0)
			return SAddr + OFFVAL_U4(pData, 0);
		/* 进入下一个cases */
#ifdef SST64
	case DW_FORM_data8:
		if (SAddr != ~(TADDR)0)
			return SAddr + OFFVAL_U8(pData, 0);
		/* 进入下一个cases */
#endif
	default: SSTERR(RS_UNKNOWN_TYPE, "Addr Form", Form); return ~(TADDR)0;
	}
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取段偏移量
*
* PARAMETERS
*	Form: 数据类型
*	pData: 数据buffer
*
* RETURNS
*	~(TSIZE)0: 失败
*	其他: 段偏移量
*
* LOCAL AFFECTED
*
*************************************************************************/
static TSIZE GetSecOff(DW_FORM Form, const void *pData)
{
	TSIZE SecOff;
	
	switch (Form) {
	case DW_FORM_sec_offset:
#ifdef SST64
		if (!AbbIsLen8B)
			SecOff = OFFVAL_U4(pData, 0);
		else
#endif
			SecOff = OFFVAL_SIZE(pData, 0);
		break;
	case DW_FORM_data4/* DWARF4不支持 */: SecOff = OFFVAL_U4(pData, 0); break;
	default: SSTERR(RS_UNKNOWN_TYPE, "Sec Form", Form); return ~(TSIZE)0;
	}
	return SecOff;
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取字符串
*
* PARAMETERS
*	pDwi: dwi句柄
*	Form: 数据类型
*	pData: 数据buffer
*
* RETURNS
*	NULL: 失败
*	其他: 字符串
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *GetDwiString(const DWI_INT *pDwi, DW_FORM Form, const void *pData)
{
	TSIZE Off;
	
	switch (Form) {
	case DW_FORM_strp:
#ifdef SST64
		if (!AbbIsLen8B)
			Off = OFFVAL_U4(pData, 0);
		else
#endif
			Off = OFFVAL_SIZE(pData, 0);
		return (Off < pDwi->StrsSz ? (char *)pDwi->pStrsBase + Off : NULL);
	case DW_FORM_string: return pData;
	default: SSTERR(RS_UNKNOWN_TYPE, "Str Form", Form); return NULL; /* DW_FORM_GNU_strp_alt/DW_FORM_GNU_str_index/DW_FORM_strx */
	}
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取引用buffer
*
* PARAMETERS
*	pDieHdl: die句柄
*	Form: 数据类型
*	ppCuBase: 返回cu base
*	pData: 数据buffer, 可能为NULL
*
* RETURNS
*	NULL: 失败
*	其他: 引用buffer
*
* LOCAL AFFECTED
*
*************************************************************************/
static const void *GetRefBuf(const DIE_HDL *pDieHdl, DW_FORM Form, const void **ppCuBase, const void *pData)
{
	const void *pCuh, *pCuBase, *pEnd;
	TSIZE Off, CuAbbOff;
	DIE_HDL DieHdl;
	DWI_INT *pDwi;

	if (!pData)
		return NULL;
	*ppCuBase = pDieHdl->pCuBase;
	switch (Form) {
	case DW_FORM_ref1: return (char *)pDieHdl->pCuBase + OFFVAL_U1(pData, 0); /* 相对于当前CU的偏移量 */
	case DW_FORM_ref2: return (char *)pDieHdl->pCuBase + OFFVAL_U2(pData, 0);
	case DW_FORM_data4/* DWARF4不支持 */: case DW_FORM_ref4: return (char *)pDieHdl->pCuBase + OFFVAL_U4(pData, 0);
#ifdef SST64
	case DW_FORM_data8/* DWARF4不支持 */: case DW_FORM_ref8: return (char *)pDieHdl->pCuBase + OFFVAL_U8(pData, 0);
#endif
	case DW_FORM_ref_udata: Getleb128(&Off, pData); return (char *)pDieHdl->pCuBase + Off;
	case DW_FORM_ref_addr: /* 相对于.debug_info的偏移量 */
		pDwi = (DWI_INT *)pDieHdl->pDwiHdl;
#ifdef SST64
		pData = (char *)pDwi->pInfBase + ((pDieHdl->CuFlag&CU_VER_MASK) == 2/* DWARF2差异 */ || AbbIsLen8B ? OFFVAL_SIZE(pData, 0) : OFFVAL_U4(pData, 0));
#else
		pData = (char *)pDwi->pInfBase + OFFVAL_SIZE(pData, 0);
#endif
		DBGERR("ref_addr\n");
		for (pCuh = pDwi->pInfBase, pEnd = (char *)pCuh + pDwi->InfSz; pCuh < pEnd; ) {
			pCuh = GetCuHead(pCuBase = pCuh, &DieHdl, &CuAbbOff);
			if (DieHdl.pData && pCuh > pData) {
				*ppCuBase = pCuBase;
				return pData;
			}
		}
		DBGERR("ref_addr异常\n");
		break;
	default: SSTERR(RS_UNKNOWN_TYPE, "Ref Form", (DW_FORM)Form); /* DW_FORM_ref_sig8, 相对于.debug_types的偏移量 */
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取data buffer
*
* PARAMETERS
*	pDieHdl: die句柄
*	pForm: 返回数据类型
*	DwAt: 属性
*	NeedOrg: 是否需要追溯原始信息
*
* RETURNS
*	NULL: 失败/无该属性
*	其他: data buffer
*
* LOCAL AFFECTED
*
*************************************************************************/
static const void *Attr2DataBuf(const DIE_HDL *pDieHdl, TSIZE *pForm, DW_AT DwAt, int NeedOrg)
{
	const void *pAttr = pDieHdl->pAttr, *pData = pDieHdl->pData;
	const void *pCuBase;
	DW_FORM Form = 0;
	DIE_HDL DieHdl;
	TSIZE Attr;

_RESTART:
	DieHdl.pData = NULL;
	AbbIsLen8B = pDieHdl->CuFlag&CU_8B_LEN;
	do {
		pAttr = Getleb128(pForm, Getleb128(&Attr, pAttr));
		if ((DW_AT)Attr == DwAt)
			return pData;
		if (Attr == DW_AT_specification || Attr == DW_AT_abstract_origin) {
			DieHdl.pData = pData;
			Form = *(DW_FORM *)pForm;
		}
		pData = SkipAttr(*pForm, pData);
	} while (Attr);
	if (NeedOrg
	  && (pData = GetRefBuf(pDieHdl, Form, &pCuBase, DieHdl.pData)) != NULL
	  && BuildDieHdl((DWI_INT *)pDieHdl->pDwiHdl, &DieHdl, pCuBase, pData)) {
		pAttr = DieHdl.pAttr;
		pData = DieHdl.pData;
		goto _RESTART;
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取无符号数据
*
* PARAMETERS
*	pDieHdl: die句柄
*	pVal: 返回数据
*	DwAt: 属性
*
* RETURNS
*	0: 成功
*	1: 失败/无该属性
*
* LOCAL AFFECTED
*
*************************************************************************/
static int Attr2UData(const DIE_HDL *pDieHdl, TSIZE *pVal, DW_AT DwAt)
{
	TSIZE Form;
	const void *pData = Attr2DataBuf(pDieHdl, &Form, DwAt, 1);
	size_t Size;

	if (!pData)
		return 1;
	switch ((DW_FORM)Form) {
	case DW_FORM_data1: *pVal = OFFVAL_U1(pData, 0); return 0;
	case DW_FORM_data2: *pVal = OFFVAL_U2(pData, 0); return 0;
	case DW_FORM_data4: *pVal = OFFVAL_U4(pData, 0); return 0;
#ifdef SST64
	case DW_FORM_data8: *pVal = OFFVAL_U8(pData, 0); return 0;
#endif
	case DW_FORM_block1:
	{
		TSIZE RegAry[REG_NUM];

		Size = OFFVAL_U1(pData, 0);
		RegAry[REG_SP] = 0;
		return DwEvalExpr((char *)pData + 1, Size, pVal, RegAry, 0
			, ".debug_info", ((DWI_INT *)pDieHdl->pDwiHdl)->pFpHdl->pPath);
	}
	case DW_FORM_udata: Getleb128(pVal, pData); return 0;
	default: SSTERR(RS_UNKNOWN_TYPE, "UData Form", Form);
	}
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取有符号数据
*
* PARAMETERS
*	pDieHdl: die句柄
*	pVal: 返回数据
*	DwAt: 属性
*
* RETURNS
*	0: 成功
*	1: 失败/无该属性
*
* LOCAL AFFECTED
*
*************************************************************************/
static int Attr2SData(const DIE_HDL *pDieHdl, TSSIZE *pVal, DW_AT DwAt)
{
	TSIZE Form;
	const void *pData = Attr2DataBuf(pDieHdl, &Form, DwAt, 1);

	if (!pData)
		return 1;
	switch ((DW_FORM)Form) {
	case DW_FORM_data1: *pVal = OFFVAL_S1(pData, 0); return 0;
	case DW_FORM_data2: *pVal = OFFVAL_S2(pData, 0); return 0;
	case DW_FORM_data4: *pVal = OFFVAL_S4(pData, 0); return 0;
#ifdef SST64
	case DW_FORM_data8: *pVal = OFFVAL_S8(pData, 0); return 0;
#endif
	case DW_FORM_sdata: Getsleb128(pVal, pData); return 0;
	default: SSTERR(RS_UNKNOWN_TYPE, "SData Form", Form);
	}
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取标志
*
* PARAMETERS
*	pDieHdl: die句柄
*	DwAt: 属性
*
* RETURNS
*	2: 失败/无该属性
*	0/1: 标志
*
* LOCAL AFFECTED
*
*************************************************************************/
static int Attr2Flag(const DIE_HDL *pDieHdl, DW_AT DwAt)
{
	TSIZE Form;
	const void *pData = Attr2DataBuf(pDieHdl, &Form, DwAt, 1);

	if (!pData)
		return 2;
	if (Form == DW_FORM_flag)
		return !!OFFVAL_U1(pData, 0);
	if (Form == DW_FORM_flag_present)
		return 1;
	SSTERR(RS_UNKNOWN_TYPE, "Flag Form", (DW_FORM)Form);
	return 2;
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取其他段偏移量
*
* PARAMETERS
*	pDieHdl: die句柄
*	DwAt: 属性
*
* RETURNS
*	~(TSIZE)0: 失败/无该属性
*	其他: 其他段偏移量
*
* LOCAL AFFECTED
*
*************************************************************************/
static TSIZE Attr2SecOff(const DIE_HDL *pDieHdl, DW_AT DwAt)
{
	TSIZE Form;
	const void *pData = Attr2DataBuf(pDieHdl, &Form, DwAt, 0);

	return (pData ? GetSecOff((DW_FORM)Form, pData) : ~(TSIZE)0);
}

/*************************************************************************
* DESCRIPTION
*	根据属性获取字符串内容
*
* PARAMETERS
*	pDieHdl: die句柄
*	DwAt: 属性
*
* RETURNS
*	NULL: 失败/无该属性
*	其他: 字符串
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *Attr2String(const DIE_HDL *pDieHdl, DW_AT DwAt)
{
	TSIZE Form;
	const void *pData = Attr2DataBuf(pDieHdl, &Form, DwAt, 1);

	return (pData ? GetDwiString((DWI_INT *)pDieHdl->pDwiHdl, Form, pData) : NULL);
}

/*************************************************************************
* DESCRIPTION
*	根据die句柄格式化enum
*
* PARAMETERS
*	pDieHdl: die句柄
*	pVal: 返回格式化数据
*	Num: enum值
*
* RETURNS
*	数据类型
*
* LOCAL AFFECTED
*
*************************************************************************/
static BASE_TYPE DieFormatEnum(const DIE_HDL *pDieHdl, BASE_VAL *pVal, TSIZE Num)
{
	const char *pStr;
	DIE_HDL CDieHdl;
	TSSIZE sVal;
	
	switch (DieGetByteSz(pDieHdl)) {
	case 1: Num = (char)Num; break;
	case 2: Num = (short)Num; break;
	case 4: Num = (TINT)Num; break;
	default:;
	}
	pVal->i = (TINT)Num;
	if (!DieGetChildDieHdl(pDieHdl, &CDieHdl))
		return CS_INT;
	do {
		if (CDieHdl.Tag != DW_TAG_enumerator)
			break;
		if (!Attr2SData(&CDieHdl, &sVal, DW_AT_const_value) && sVal == (TSSIZE)Num
		  && (pStr = Attr2String(&CDieHdl, DW_AT_name)) != NULL) {
			pVal->pStr = pStr;
			return CS_STRING;
		}
	} while (DieGetSiblingDieHdl(&CDieHdl, &CDieHdl));
	return CS_INT;
}

/*************************************************************************
* DESCRIPTION
*	根据die句柄格式化基本类型数据
*
* PARAMETERS
*	pDieHdl: die句柄
*	pVal: 返回格式化数据
*	pPtr: 数据buffer
*
* RETURNS
*	数据类型
*
* LOCAL AFFECTED
*
*************************************************************************/
static BASE_TYPE DieFormatBaseVal(const DIE_HDL *pDieHdl, BASE_VAL *pVal, const void *pPtr)
{
	typedef enum _DW_ATE
	{
		DW_ATE_none,
		DW_ATE_address,
		DW_ATE_boolean,
		DW_ATE_complex_float,
		DW_ATE_float,
		DW_ATE_signed,
		DW_ATE_signed_char,
		DW_ATE_unsigned,
		DW_ATE_unsigned_char,
		DW_ATE_imaginary_float,
		DW_ATE_packed_decimal,
		DW_ATE_numeric_string,
		DW_ATE_edited,
		DW_ATE_signed_fixed,
		DW_ATE_decimal_float,
		DW_ATE_UTF,
	} DW_ATE;
	TSIZE Encode, ByteSz;

	if (Attr2UData(pDieHdl, &Encode, DW_AT_encoding)) {
		pVal->i = OFFVAL(TINT, pPtr, 0);
		return CS_INT;
	}
	ByteSz = DieGetByteSz(pDieHdl);
	switch ((DW_ATE)Encode) {
	case DW_ATE_address:
		if (ByteSz != sizeof(TADDR))
			DBGERR("DW_ATE_address = %d\n", ByteSz);
		break;
	case DW_ATE_boolean: pVal->n = !!OFFVAL_U1(pPtr, 0); return CS_BOOL;
	case DW_ATE_float: pVal->f = OFFVAL(float, pPtr, 0); return CS_FLOAT;
	case DW_ATE_signed:
		if (ByteSz == 8) {
			pVal->j = OFFVAL_S8(pPtr, 0);
			return CS_I64;
		}
		pVal->i = OFFVAL(TINT, pPtr, 0);
		return CS_INT;
	case DW_ATE_signed_char: pVal->i = OFFVAL_S1(pPtr, 0); return CS_INT;
	case DW_ATE_unsigned:
		if (ByteSz == 8) {
			pVal->j = OFFVAL_U8(pPtr, 0);
			return CS_I64;
		}
		pVal->n = OFFVAL(TUINT, pPtr, 0);
		return CS_UINT;
	case DW_ATE_unsigned_char: pVal->n = OFFVAL_U1(pPtr, 0); return CS_UINT;
	default: DBGERR("ATE: %X\n", (DW_ATE)Encode);
	}
	pVal->p = OFFVAL_ADDR(pPtr, 0);
	return CS_ADDR;
}

/*************************************************************************
* DESCRIPTION
*	获取cu die基地址
*
* PARAMETERS
*	pDieHdl: die句柄
*
* RETURNS
*	~(TADDR)0: 失败
*	其他: 基地址
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR DieGetCuBaseAddr(const DIE_HDL *pDieHdl)
{
	TSIZE Attr, Form, CuAbbOff;
	const void *pAttr, *pData;
	DIE_HDL CuDieHdl;

	GetCuHead(pDieHdl->pCuBase, &CuDieHdl, &CuAbbOff);
	BuildDieHdl((DWI_INT *)pDieHdl->pDwiHdl, &CuDieHdl, pDieHdl->pCuBase, CuDieHdl.pData);
	pAttr = CuDieHdl.pAttr;
	pData = CuDieHdl.pData;
	AbbIsLen8B = CuDieHdl.CuFlag&CU_8B_LEN;
	do {
		pAttr = Getleb128(&Form, Getleb128(&Attr, pAttr));
		if ((DW_AT)Attr == DW_AT_low_pc || (DW_AT)Attr == DW_AT_entry_pc)
			return GetTarAddr(Form, pData, ~(TADDR)0);
		pData = SkipAttr(Form, pData);
	} while (Attr);
	return ~(TADDR)0;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取die句柄名称
*
* PARAMETERS
*	pDieHdl: die句柄
*
* RETURNS
*	NULL: 失败/无该属性
*	其他: 名称
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *DieGetName(const DIE_HDL *pDieHdl)
{
	return Attr2String(pDieHdl, DW_AT_name);
}

/*************************************************************************
* DESCRIPTION
*	获取die句柄链接名称
*
* PARAMETERS
*	pDieHdl: die句柄
*
* RETURNS
*	NULL: 失败/无该属性
*	其他: 链接名称
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *DieGetLinkName(const DIE_HDL *pDieHdl)
{
	const char *pStr;
	
	pStr = Attr2String(pDieHdl, DW_AT_linkage_name);
	return (pStr ? pStr : Attr2String(pDieHdl, DW_AT_name));
}

/*************************************************************************
* DESCRIPTION
*	获取die句柄占用字节
*
* PARAMETERS
*	pDieHdl: die句柄
*
* RETURNS
*	~(TSIZE)0: 失败/无该属性
*	其他: 占用字节
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DieGetByteSz(const DIE_HDL *pDieHdl)
{
	TSIZE Val;
	
	return (Attr2UData(pDieHdl, &Val, DW_AT_byte_size) ? ~(TSIZE)0 : Val);
}

/*************************************************************************
* DESCRIPTION
*	获取die句柄对应路径和行号
*
* PARAMETERS
*	pDieHdl: die句柄
*	pLineNo: 返回行号
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DieGetLineInf(const DIE_HDL *pDieHdl, TUINT *pLineNo)
{
	DWI_INT * const pDwi = (DWI_INT *)pDieHdl->pDwiHdl;
	TSIZE LineOff, Val, CuAbbOff;
	DIE_HDL CuDieHdl;

	if (!Attr2UData(pDieHdl, &Val, DW_AT_call_line))
		*pLineNo = (TUINT)Val;
	GetCuHead(pDieHdl->pCuBase, &CuDieHdl, &CuAbbOff);
	BuildDieHdl(pDwi, &CuDieHdl, pDieHdl->pCuBase, CuDieHdl.pData);
	LineOff = Attr2SecOff(&CuDieHdl, DW_AT_stmt_list);
	if (LineOff != ~(TSIZE)0 && !Attr2UData(pDieHdl, &Val, DW_AT_call_file))
		return DwlIdx2Path(&pDwi->DwlHdl, LineOff, (unsigned)Val);
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	获取die句柄偏移量(比如结构体成员偏移量)
*
* PARAMETERS
*	pDieHdl: die句柄
*
* RETURNS
*	~(TSIZE)0: 失败/无该属性
*	其他: 偏移量
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DieGetOffset(const DIE_HDL *pDieHdl)
{
	TSIZE Val;

	return (Attr2UData(pDieHdl, &Val, DW_AT_data_member_location) ? ~(TSIZE)0 : Val);
}

/*************************************************************************
* DESCRIPTION
*	指定地址是否在die里
*
* PARAMETERS
*	pDieHdl: die句柄
*	Addr: 地址, 包含THUMB bit
*
* RETURNS
*	0: 不在
*	其他: 在并返回die句柄首地址, 不包含THUMB bit
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR DieCheckAddr(const DIE_HDL *pDieHdl, TADDR Addr)
{
	const void *pAttr = pDieHdl->pAttr, *pData = pDieHdl->pData;
	TADDR LowAddr = 0, BaseAddr;
	TSIZE Attr, Form, SecOff;
	int Flag = 0;

	AbbIsLen8B = pDieHdl->CuFlag&CU_8B_LEN;
	do {
		pAttr = Getleb128(&Form, Getleb128(&Attr, pAttr));
		switch ((DW_AT)Attr) {
		case DW_AT_low_pc:
			LowAddr = GetTarAddr(Form, pData, ~(TADDR)0);
			if (Addr < LowAddr)
				return 0;
			Flag |= 1;
			break;
		case DW_AT_high_pc: /* address, constant */
			if (Addr + 1 >= GetTarAddr(Form, pData, LowAddr) + 1)
				return 0;
			Flag |= 2;
			if (Flag == 3)
				return LowAddr;
			break;
		case DW_AT_ranges:
			SecOff = GetSecOff(Form, pData);
			if (SecOff != ~(TSIZE)0
			  && (BaseAddr = DieGetCuBaseAddr(pDieHdl)) != ~(TADDR)0
			  && (Addr = DwrCheckAddr(&((DWI_INT *)pDieHdl->pDwiHdl)->DwrHdl, SecOff, Addr - BaseAddr)) != ~(TADDR)0)
				return BaseAddr + Addr;
			return 0;
		default:;
		}
		pData = SkipAttr(Form, pData);
	} while (Attr);
	return (Flag == 3 ? LowAddr : 0);
}

/*************************************************************************
* DESCRIPTION
*	通过格式化buffer获取die句柄的内容
*
* PARAMETERS
*	pDieHdl: die句柄
*	pVal: 返回内容
*	pPtr: 数据buffer
*
* RETURNS
*	数据类型
*
* GLOBAL AFFECTED
*
*************************************************************************/
BASE_TYPE DieFormatVal(const DIE_HDL *pDieHdl, BASE_VAL *pVal, const void *pPtr)
{
	DIE_HDL TDieHdl;

	for (TDieHdl = *pDieHdl; DieGetTypeDieHdl(&TDieHdl, &TDieHdl); ) {
		switch (TDieHdl.Tag) {
		case DW_TAG_enumeration_type:
			return DieFormatEnum(&TDieHdl, pVal, OFFVAL_SIZE(pPtr, 0));
		case DW_TAG_class_type: case DW_TAG_structure_type: case DW_TAG_union_type: case DW_TAG_subroutine_type: /* 是否需要显示?? */
			goto _FAIL;
		case DW_TAG_base_type:
			return DieFormatBaseVal(&TDieHdl, pVal, pPtr);
		case DW_TAG_pointer_type: case DW_TAG_reference_type: case DW_TAG_rvalue_reference_type:
			goto _FAIL;
		case DW_TAG_typedef: case DW_TAG_const_type: case DW_TAG_restrict_type: case DW_TAG_volatile_type:
			break;
		default:
			DBGERR("type tag: %X\n", TDieHdl.Tag);
			goto _FAIL;
		}
	}
_FAIL:
	pVal->p = OFFVAL_ADDR(pPtr, 0);
	return CS_ADDR;
}

/*************************************************************************
* DESCRIPTION
*	根据die句柄和位置获取其内容
*
* PARAMETERS
*	pDieHdl: die句柄
*	pVal: 返回内容(需要8字节)
*	pRegAry/RegMask: 寄存器组/寄存器有效位图
*	Addr: PC地址
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DieGetValByLoc(const DIE_HDL *pDieHdl, void *pVal, const TSIZE *pRegAry, TSIZE RegMask, TADDR Addr)
{
	const DWI_INT * const pDwi = (DWI_INT *)pDieHdl->pDwiHdl;
	const char *pSegName = ".debug_info";
	TSIZE Form, SecOff, Size;
	const void *pData;
	TADDR BaseAddr;
	
	pData = Attr2DataBuf(pDieHdl, &Form, DW_AT_location, 0);
	if (!pData)
		return 1;
	switch ((DW_FORM)Form) {
	case DW_FORM_block: case DW_FORM_exprloc: pData = Getleb128(&Size, pData); break;
	case DW_FORM_block1: Size = OFFVAL_U1(pData, 0); pData = (char *)pData + 1; break;
	case DW_FORM_block2: Size = OFFVAL_U2(pData, 0); pData = (char *)pData + 2; break;
	case DW_FORM_block4: Size = OFFVAL_U4(pData, 0); pData = (char *)pData + 4; break;
	default:
		SecOff = GetSecOff((DW_FORM)Form, pData);
		if (SecOff == ~(TSIZE)0
		  || (BaseAddr = DieGetCuBaseAddr(pDieHdl)) == ~(TADDR)0
		  || (pData = DwlcGetExpr(&pDwi->DwlcHdl, &Size, SecOff, Addr - BaseAddr)) == NULL)
			return 1;
		pSegName = ".debug_loc";
	}
	OFFVAL_U8(pVal, 0) = 0;
	return DwEvalExpr(pData, Size, pVal, pRegAry, RegMask, pSegName, pDwi->pFpHdl->pPath);
}

/*************************************************************************
* DESCRIPTION
*	通过地址获取对应路径和行号
*
* PARAMETERS
*	pSymHdl: sym句柄
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
char *DwiAddr2Line(DWI_HDL *pDwiHdl, TUINT *pLineNo, TADDR Addr)
{
	return DwlAddr2Line(&((DWI_INT *)pDwiHdl)->DwlHdl, pLineNo, Addr);
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取子die句柄
*
* PARAMETERS
*	pDieHdl: 父die句柄
*	pCDieHdl: 返回子die句柄, 即使返回NULL也会被修改到
*
* RETURNS
*	NULL: 失败/无子die
*	其他: 子die句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *DieGetChildDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pCDieHdl)
{
	const void *pAttr = pDieHdl->pAttr, *pData = pDieHdl->pData;
	TSIZE Attr, Form;

	if (!OFFVAL_U1(pDieHdl->pAttr, -1)) /* 无子die */
		return NULL;
	AbbIsLen8B = pDieHdl->CuFlag&CU_8B_LEN;
	do {
		pAttr = Getleb128(&Form, Getleb128(&Attr, pAttr));
		pData = SkipAttr(Form, pData);
	} while (Attr); /* 跳过父die */
	return BuildDieHdl((DWI_INT *)pDieHdl->pDwiHdl, pCDieHdl, pDieHdl->pCuBase, pData);
}

/*************************************************************************
* DESCRIPTION
*	获取下1个兄弟die句柄
*
* PARAMETERS
*	pDieHdl: 兄弟die句柄
*	pSDieHdl: 返回下1个兄弟die句柄, 即使返回NULL也会被修改到
*
* RETURNS
*	NULL: 失败/无兄弟die句柄
*	其他: 下1个兄弟die句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *DieGetSiblingDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pSDieHdl)
{
	const void *pAttr = pDieHdl->pAttr, *pData = pDieHdl->pData;
	DWI_INT * const pDwi = (DWI_INT *)pDieHdl->pDwiHdl;
	TSIZE Off, Attr, Form;
	DIE_HDL CDieHdl;

	AbbIsLen8B = pDieHdl->CuFlag&CU_8B_LEN;
	do {
		pAttr = Getleb128(&Form, Getleb128(&Attr, pAttr));
		if ((DW_AT)Attr == DW_AT_sibling && OFFVAL_U1(pDieHdl->pAttr, -1)/* 有子die */) {
			switch (Form) {
			case DW_FORM_ref1: Off = OFFVAL_U1(pData, 0); break;
			case DW_FORM_ref2: Off = OFFVAL_U2(pData, 0); break;
			case DW_FORM_ref4: Off = OFFVAL_U4(pData, 0); break;
#ifdef SST64
			case DW_FORM_ref8: Off = OFFVAL_U8(pData, 0); break;
#endif
			case DW_FORM_ref_udata: Getleb128(&Off, pData); break;
			default: SSTERR(RS_UNKNOWN_TYPE, "Sibling Form", (DW_FORM)Form); return NULL;
			}
			pData = (char *)pDieHdl->pCuBase + Off;
			goto _OUT;
		}
		pData = SkipAttr(Form, pData);
	} while (Attr);
	if (OFFVAL_U1(pDieHdl->pAttr, -1)) { /* 有子die却无sibling, 可能缺失DW_AT_sibling或已结尾 */
		if (!BuildDieHdl(pDwi, &CDieHdl, pDieHdl->pCuBase, pData)) /* 子die */
			return NULL;
		while (DieGetSiblingDieHdl(&CDieHdl, &CDieHdl)) {} /* 遍历 */
		pData = CDieHdl.pData;
	}
_OUT:
	return BuildDieHdl(pDwi, pSDieHdl, pDieHdl->pCuBase, pData);
}

/*************************************************************************
* DESCRIPTION
*	获取类型die句柄
*
* PARAMETERS
*	pDieHdl: die句柄
*	pTDieHdl: 返回类型die句柄, 即使返回NULL也会被修改到
*
* RETURNS
*	NULL: 失败/无类型die句柄
*	其他: 类型die句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *DieGetTypeDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pTDieHdl)
{
	TSIZE Form;
	const void *pCuBase, *pData = Attr2DataBuf(pDieHdl, &Form, DW_AT_type, 1);
	
	pData = GetRefBuf(pDieHdl, Form, &pCuBase, pData);
	return (pData ? BuildDieHdl((DWI_INT *)pDieHdl->pDwiHdl, pTDieHdl, pCuBase, pData) : NULL);
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取cu die句柄(根句柄)
*
* PARAMETERS
*	pDwiHdl: dwi句柄
*	pCDieHdl: 返回cu die句柄, 即使返回NULL也会被修改到
*	Addr: 函数地址, 包含THUMB bit
*
* RETURNS
*	NULL: 失败/没有cu die句柄
*	其他: cu die句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *Dwi2CuDieHdlByAddr(DWI_HDL *pDwiHdl, DIE_HDL *pCDieHdl, TADDR Addr)
{
	DWI_INT * const pDwi = (DWI_INT *)pDwiHdl;
	TSIZE CuOff, CuAbbOff = 0;

	CuOff = DwarGetCuOff(&pDwi->DwarHdl, Addr);
	if (CuOff == ~(TSIZE)0) /* vmlinux的.debug_aranges正常，不需要遍历cu die(这个行为非常耗时!!!) */
		return NULL;
	GetCuHead((char *)pDwi->pInfBase + CuOff, pCDieHdl, &CuAbbOff);
	if (!pCDieHdl->pData)
		return NULL;
	DwaAddAbbInf(&pDwi->DwaHdl, CuAbbOff);
	return BuildDieHdl(pDwi, pCDieHdl, (char *)pDwi->pInfBase + CuOff, pCDieHdl->pData);
}

/*************************************************************************
* DESCRIPTION
*	根据所在文件名获取die句柄
*
* PARAMETERS
*	pDwiHdl: dwi句柄
*	pDieHdl: 返回die句柄
*	pFileName: 文件名
*
* RETURNS
*	NULL: 失败/没有die句柄
*	其他: die句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *Dwi2DieHdlByFile(DWI_HDL *pDwiHdl, DIE_HDL *pDieHdl, const char *pFileName)
{
	DWI_INT * const pDwi = (DWI_INT *)pDwiHdl;
	const void *pCuh, *pCuBase, * const pEnd = (char *)pDwi->pInfBase + pDwi->InfSz;
	size_t Len, StrLen = (pFileName ? strlen(pFileName) : 0);
	const char *ptFileName;
	TSIZE CuAbbOff = 0;

	for (pCuh = pDwi->pInfBase; pCuh < pEnd; ) {
		pCuh = GetCuHead(pCuBase = pCuh, pDieHdl, &CuAbbOff);
		if (!pDieHdl->pData)
			continue;
		DwaAddAbbInf(&pDwi->DwaHdl, CuAbbOff);
		if (!BuildDieHdl(pDwi, pDieHdl, pCuBase, pDieHdl->pData)) /* 失败? */
			continue;
		if ((ptFileName = Attr2String(pDieHdl, DW_AT_name)) == NULL
		  || (Len = strlen(ptFileName)) < StrLen || strcmp(ptFileName + Len - StrLen, pFileName)) /* pFileName不匹配 */
			continue;
		return pDieHdl;
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	获取dwi句柄
*
* PARAMETERS
*	pFpHdl: fp句柄
*	InfFOff/InfSz: .debug_info偏移地址/大小(字节)
*	AbbFOff/AbbSz: .debug_abbrev偏移地址/大小(字节)
*	StrsFOff/StrsSz: .debug_str偏移地址/大小(字节), 为0则无.debug_str
*	RgeFOff/RgeSz: .debug_ranges偏移地址/大小(字节), 为0则无.debug_ranges
*	LocFOff/LocSz: .debug_loc偏移地址/大小(字节), 为0则无.debug_loc
*	LineFOff/LineSz: .debug_line偏移地址/大小(字节), 为0则无.debug_line
*	ARgeFOff/ARgeSz: .debug_aranges偏移地址/大小(字节), 为0则无.debug_aranges
*
* RETURNS
*	NULL: 失败
*	其他: dwi句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWI_HDL *GetDwiHdl(const void *pFpHdl, TSIZE InfFOff, TSIZE InfSz, TSIZE AbbFOff, TSIZE AbbSz, TSIZE StrsFOff, TSIZE StrsSz
  , TSIZE RgeFOff, TSIZE RgeSz, TSIZE LocFOff, TSIZE LocSz, TSIZE LineFOff, TSIZE LineSz, TSIZE ARgeFOff, TSIZE ARgeSz)
{
	DWI_INT *pDwi;

	pDwi = Calloc(sizeof(*pDwi));
	if (!pDwi)
		goto _ERR2;
	if (!GetDwaHdl(&pDwi->DwaHdl, pFpHdl, AbbFOff, AbbSz))
		goto _ERR1;
	pDwi->pFpHdl = DupFpHdl(pFpHdl);
	pDwi->InfSz = InfSz;
	pDwi->pInfBase = FpMapRo(pDwi->pFpHdl, InfFOff, InfSz);
	if (!pDwi->pInfBase)
		goto _ERR3;
	if (StrsSz) {
		pDwi->StrsSz = StrsSz;
		pDwi->pStrsBase = FpMapRo(pDwi->pFpHdl, StrsFOff, StrsSz);
	}
	if (RgeSz)
		GetDwrHdl(&pDwi->DwrHdl, pFpHdl, RgeFOff, RgeSz);
	if (LocSz)
		GetDwlcHdl(&pDwi->DwlcHdl, pFpHdl, LocFOff, LocSz);
	if (LineSz)
		GetDwlHdl(&pDwi->DwlHdl, pFpHdl, LineFOff, LineSz);
	if (ARgeSz)
		GetDwarHdl(&pDwi->DwarHdl, pFpHdl, ARgeFOff, ARgeSz);
	return (DWI_HDL *)pDwi;
_ERR3:
	PutDwaHdl(&pDwi->DwaHdl, pFpHdl);
	PutFpHdl(pFpHdl);
	Free(pDwi);
	SSTWARN(RS_FILE_MAP, ((FP_HDL *)pFpHdl)->pPath, ".debug_info");
	return NULL;
_ERR2:
	SSTWARN(RS_OOM);
_ERR1:
	Free(pDwi);
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	释放dwi句柄
*
* PARAMETERS
*	pDwiHdl: dwi句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwiHdl(DWI_HDL *pDwiHdl)
{
	DWI_INT * const pDwi = (void *)pDwiHdl;

	FpUnmap(pDwi->pFpHdl, pDwi->pInfBase, pDwi->InfSz);
	if (pDwi->pStrsBase)
		FpUnmap(pDwi->pFpHdl, pDwi->pStrsBase, pDwi->StrsSz);
	if (pDwi->DwrHdl.pBase)
		PutDwrHdl(&pDwi->DwrHdl, pDwi->pFpHdl);
	if (pDwi->DwlcHdl.pBase)
		PutDwlcHdl(&pDwi->DwlcHdl, pDwi->pFpHdl);
	if (pDwi->DwlHdl.pBase)
		PutDwlHdl(&pDwi->DwlHdl, pDwi->pFpHdl);
	if (pDwi->DwarHdl.pBase)
		PutDwarHdl(&pDwi->DwarHdl, pDwi->pFpHdl);
	PutDwaHdl(&pDwi->DwaHdl, pDwi->pFpHdl);
	PutFpHdl(pDwi->pFpHdl);
	Free(pDwi);
}
