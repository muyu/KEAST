/*********************************************************************************
* Copyright(C),2015-2017,MediaTek Inc.
* FileName:	dwinfo.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2015/11/6
* Description: .debug_info����(֧��dwarf2/3)
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
*	��ȡdie�������
*
* PARAMETERS
*	pDieHdl: die���
*
* RETURNS
*	NULL: ʧ��/�޸�����
*	����: ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *DieGetName(const DIE_HDL *pDieHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡdie�����������
*
* PARAMETERS
*	pDieHdl: die���
*
* RETURNS
*	NULL: ʧ��/�޸�����
*	����: ��������
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *DieGetLinkName(const DIE_HDL *pDieHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡdie���ռ���ֽ�
*
* PARAMETERS
*	pDieHdl: die���
*
* RETURNS
*	~(TSIZE)0: ʧ��/�޸�����
*	����: ռ���ֽ�
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DieGetByteSz(const DIE_HDL *pDieHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡdie�����Ӧ·�����к�
*
* PARAMETERS
*	pDieHdl: die���
*	pLineNo: �����к�
*
* RETURNS
*	NULL: ʧ��
*	����: ·��������ʱ�����Free()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DieGetLineInf(const DIE_HDL *pDieHdl, TUINT *pLineNo);

/*************************************************************************
* DESCRIPTION
*	��ȡdie���ƫ����(����ṹ���Աƫ����)
*
* PARAMETERS
*	pDieHdl: die���
*
* RETURNS
*	~(TSIZE)0: ʧ��/�޸�����
*	����: ƫ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DieGetOffset(const DIE_HDL *pDieHdl);

/*************************************************************************
* DESCRIPTION
*	ָ����ַ�Ƿ���die��
*
* PARAMETERS
*	pDieHdl: die���
*	Addr: ��ַ, ����THUMB bit
*
* RETURNS
*	0: ����
*	����: �ڲ�����die����׵�ַ, ������THUMB bit
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR DieCheckAddr(const DIE_HDL *pDieHdl, TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	ͨ����ʽ��buffer��ȡdie���������
*
* PARAMETERS
*	pDieHdl: die���
*	pVal: ��������
*	pPtr: ����buffer
*
* RETURNS
*	��������
*
* GLOBAL AFFECTED
*
*************************************************************************/
BASE_TYPE DieFormatVal(const DIE_HDL *pDieHdl, BASE_VAL *pVal, const void *pPtr);

/*************************************************************************
* DESCRIPTION
*	����die�����λ�û�ȡ������
*
* PARAMETERS
*	pDieHdl: die���
*	pVal: ��������(��Ҫ8�ֽ�)
*	pRegAry/RegMask: �Ĵ�����/�Ĵ�����Чλͼ
*	Addr: PC��ַ
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DieGetValByLoc(const DIE_HDL *pDieHdl, void *pVal, const TSIZE *pRegAry, TSIZE RegMask, TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	ͨ����ַ��ȡ��Ӧ·�����к�
*
* PARAMETERS
*	pSymHdl: sym���
*	pLineNo: �����к�
*	Addr: ��ַ, ����THUMB bit
*
* RETURNS
*	NULL: ʧ��
*	����: ·��������ʱ�����Free()�ͷ�!!!
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
*	��ȡ��die���
*
* PARAMETERS
*	pDieHdl: ��die���
*	pCDieHdl: ������die���, ��ʹ����NULLҲ�ᱻ�޸ĵ�
*
* RETURNS
*	NULL: ʧ��/����die
*	����: ��die���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *DieGetChildDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pCDieHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡ��1���ֵ�die���
*
* PARAMETERS
*	pDieHdl: �ֵ�die���
*	pSDieHdl: ������1���ֵ�die���, ��ʹ����NULLҲ�ᱻ�޸ĵ�
*
* RETURNS
*	NULL: ʧ��/���ֵ�die���
*	����: ��1���ֵ�die���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *DieGetSiblingDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pSDieHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡ����die���
*
* PARAMETERS
*	pDieHdl: die���
*	pTDieHdl: ��������die���, ��ʹ����NULLҲ�ᱻ�޸ĵ�
*
* RETURNS
*	NULL: ʧ��/������die���
*	����: ����die���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *DieGetTypeDieHdl(const DIE_HDL *pDieHdl, DIE_HDL *pTDieHdl);

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡcu die���(�����)
*
* PARAMETERS
*	pDwiHdl: dwi���
*	pCDieHdl: ����cu die���, ��ʹ����NULLҲ�ᱻ�޸ĵ�
*	Addr: ������ַ, ����THUMB bit
*
* RETURNS
*	NULL: ʧ��/û��cu die���
*	����: cu die���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *Dwi2CuDieHdlByAddr(DWI_HDL *pDwiHdl, DIE_HDL *pCDieHdl, TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	���������ļ�����ȡdie���
*
* PARAMETERS
*	pDwiHdl: dwi���
*	pDieHdl: ����die���
*	pFileName: �ļ���
*
* RETURNS
*	NULL: ʧ��/û��die���
*	����: die���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DIE_HDL *Dwi2DieHdlByFile(DWI_HDL *pDwiHdl, DIE_HDL *pDieHdl, const char *pFileName);

/*************************************************************************
* DESCRIPTION
*	��ȡdwi���
*
* PARAMETERS
*	pFpHdl: fp���
*	InfFOff/InfSz: .debug_infoƫ�Ƶ�ַ/��С(�ֽ�)
*	AbbFOff/AbbSz: .debug_abbrevƫ�Ƶ�ַ/��С(�ֽ�)
*	StrsFOff/StrsSz: .debug_strƫ�Ƶ�ַ/��С(�ֽ�), Ϊ0����.debug_str
*	RgeFOff/RgeSz: .debug_rangesƫ�Ƶ�ַ/��С(�ֽ�), Ϊ0����.debug_ranges
*	LocFOff/LocSz: .debug_locƫ�Ƶ�ַ/��С(�ֽ�), Ϊ0����.debug_loc
*	LineFOff/LineSz: .debug_lineƫ�Ƶ�ַ/��С(�ֽ�), Ϊ0����.debug_line
*	ARgeFOff/ARgeSz: .debug_arangesƫ�Ƶ�ַ/��С(�ֽ�), Ϊ0����.debug_aranges
*
* RETURNS
*	NULL: ʧ��
*	����: dwi���
*
* GLOBAL AFFECTED
*
*************************************************************************/
DWI_HDL *GetDwiHdl(const void *pFpHdl, TSIZE InfFOff, TSIZE InfSz, TSIZE AbbFOff, TSIZE AbbSz, TSIZE StrsFOff, TSIZE StrsSz
  , TSIZE RgeFOff, TSIZE RgeSz, TSIZE LocFOff, TSIZE LocSz, TSIZE LineFOff, TSIZE LineSz, TSIZE ARgeFOff, TSIZE ARgeSz);

/*************************************************************************
* DESCRIPTION
*	�ͷ�dwi���
*
* PARAMETERS
*	pDwiHdl: dwi���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutDwiHdl(DWI_HDL *pDwiHdl);

#endif
