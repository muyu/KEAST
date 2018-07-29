/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwinfo.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/6
* Description: .debug_info解析(支持dwarf2/3)
**********************************************************************************/
#ifndef _DWINFO_H
#define _DWINFO_H

#include <ttypes.h>
#include <symfile.h>

typedef const void * DWI_HDL;

typedef enum _DW_TAG
{
	DW_TAG_padding,
	DW_TAG_array_type,
	DW_TAG_class_type,
	DW_TAG_entry_point,
	DW_TAG_enumeration_type,
	DW_TAG_formal_parameter,
	DW_TAG_unused1,
	DW_TAG_unused2,
	DW_TAG_imported_declaration,
	DW_TAG_unused3,
	DW_TAG_label,
	DW_TAG_lexical_block,
	DW_TAG_unused4,
	DW_TAG_member,
	DW_TAG_unused5,
	DW_TAG_pointer_type,
	DW_TAG_reference_type, /* 0x10 */
	DW_TAG_compile_unit,
	DW_TAG_string_type,
	DW_TAG_structure_type,
	DW_TAG_unused6,
	DW_TAG_subroutine_type,
	DW_TAG_typedef,
	DW_TAG_union_type,
	DW_TAG_unspecified_parameters,
	DW_TAG_variant,
	DW_TAG_common_block,
	DW_TAG_common_inclusion,
	DW_TAG_inheritance,
	DW_TAG_inlined_subroutine,
	DW_TAG_module,
	DW_TAG_ptr_to_member_type,
	DW_TAG_set_type, /* 0x20 */
	DW_TAG_subrange_type,
	DW_TAG_with_stmt,
	DW_TAG_access_declaration,
	DW_TAG_base_type,
	DW_TAG_catch_block,
	DW_TAG_const_type,
	DW_TAG_constant,
	DW_TAG_enumerator,
	DW_TAG_file_type,
	DW_TAG_friend,
	DW_TAG_namelist,
	DW_TAG_namelist_item,
	DW_TAG_packed_type,
	DW_TAG_subprogram,
	DW_TAG_template_type_param,
	DW_TAG_template_value_param, /* 0x30 */
	DW_TAG_thrown_type,
	DW_TAG_try_block,
	DW_TAG_variant_part,
	DW_TAG_variable,
	DW_TAG_volatile_type,
	/* DWARF 3 */
	DW_TAG_dwarf_procedure,
	DW_TAG_restrict_type,
	DW_TAG_interface_type,
	DW_TAG_namespace,
	DW_TAG_imported_module,
	DW_TAG_unspecified_type,
	DW_TAG_partial_unit,
	DW_TAG_imported_unit,
	DW_TAG_unused7,
	DW_TAG_condition,
	DW_TAG_shared_type, /* 0x40 */
	/* DWARF 4 */
	DW_TAG_type_unit,
	DW_TAG_rvalue_reference_type,
	DW_TAG_template_alias,
	DW_TAG_lo_user = 0x4080,
	DW_TAG_hi_user = 0xffff,
} DW_TAG;

typedef struct _DIE_HDL
{
	DWI_HDL *pDwiHdl;
	const void *pData, *pAttr, *pCuBase;

#define CU_VER_MASK	0x7
#define CU_8B_LEN	(1 << 3)
#define CU_STR_UTF8	(1 << 4)
	unsigned short Tag, CuFlag;
} DIE_HDL;


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
const char *DieGetName(const DIE_HDL *pDieHdl);

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
const char *DieGetLinkName(const DIE_HDL *pDieHdl);

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
TSIZE DieGetByteSz(const DIE_HDL *pDieHdl);

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
char *DieGetLineInf(const DIE_HDL *pDieHdl, TUINT *pLineNo);

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
TSIZE DieGetOffset(const DIE_HDL *pDieHdl);

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
TADDR DieCheckAddr(const DIE_HDL *pDieHdl, TADDR Addr);

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
BASE_TYPE DieFormatVal(const DIE_HDL *pDieHdl, BASE_VAL *pVal, const void *pPtr);

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
int DieGetValByLoc(const DIE_HDL *pDieHdl, void *pVal, const TSIZE *pRegAry, TSIZE RegMask, TADDR Addr);

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
char *DwiAddr2Line(DWI_HDL *pDwiHdl, TUINT *pLineNo, TADDR Addr);


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
DIE_HDL *DieGetChildDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pCDieHdl);

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
DIE_HDL *DieGetSiblingDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pSDieHdl);

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
DIE_HDL *DieGetTypeDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pTDieHdl);

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
DIE_HDL *Dwi2CuDieHdlByAddr(DWI_HDL *pDwiHdl, DIE_HDL *pCDieHdl, TADDR Addr);

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
DIE_HDL *Dwi2DieHdlByFile(DWI_HDL *pDwiHdl, DIE_HDL *pDieHdl, const char *pFileName);

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
  , TSIZE RgeFOff, TSIZE RgeSz, TSIZE LocFOff, TSIZE LocSz, TSIZE LineFOff, TSIZE LineSz, TSIZE ARgeFOff, TSIZE ARgeSz);

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
void PutDwiHdl(DWI_HDL *pDwiHdl);

#endif
