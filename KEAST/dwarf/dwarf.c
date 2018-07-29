/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	dwarf.c
* Author:		Guangye.Yang
* Version:	V1.3
* Date:		2013/3/11
* Description: dwarf解析(支持dwarf2~4)
**********************************************************************************/
#include "string_res.h"
#include <module.h>
#include <memspace.h>
#include "dwarf.h"


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取无符号LEB128值
*
* PARAMETERS
*	pVal: 返回无符号LEB128值
*	p: 指针
*
* RETURNS
*	更新后的指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *Getleb128(TSIZE *pVal, const void *p)
{
	unsigned Byte, Shift = 0;
	TSIZE Val = 0;

	do {
		Byte = *(unsigned char *)p;
		p = (char *)p + 1;
		Val |= ((TSIZE)Byte&0x7F) << Shift;
		Shift += 7;
	} while (Byte&0x80);
	*pVal = Val;
	return p;
}

/*************************************************************************
* DESCRIPTION
*	获取有符号LEB128值
*
* PARAMETERS
*	pVal: 返回有符号LEB128值
*	p: 指针
*
* RETURNS
*	更新后的指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *Getsleb128(TSSIZE *pVal, const void *p)
{
	unsigned Byte, Shift = 0;
	TSIZE Val = 0;

	do {
		Byte = *(unsigned char *)p;
		p = (char *)p + 1;
		Val |= ((TSIZE)Byte&0x7F) << Shift;
		Shift += 7;
	} while (Byte&0x80);
	if ((Byte&0x40) && Shift < 8 * sizeof(Val))
		Val |= ((TSIZE)-1) << Shift;
	*pVal = Val;
	return p;
}

/*************************************************************************
* DESCRIPTION
*	解析表达式
*
* PARAMETERS
*	p: 指令buffer
*	Len: 指令长度
*	pRt: 返回计算的结果
*	pRegAry/RegMask: 参考的寄存器组/寄存器有效位图
*	pSegName: 段名称, 如.debug_frame/.debug_loc等
*	pFileName: 文件名
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DwEvalExpr(const void *p, TSIZE Len, TSIZE *pRt, const TSIZE *pRegAry, TSIZE RegMask, const char *pSegName, const char *pFileName)
{
#define STK_PUSH(Val)	   do { if (Pos >= ARY_CNT(Stack)) goto _ERR1; Stack[Pos++] = (Val); } while (0)
#define STK_POP(Val)		do { if (!Pos) goto _ERR2; (Val) = Stack[--Pos]; } while (0)
#define STK_PICK(Val, n)	do { if (Pos < (n) + 1) { Pos = (n); goto _ERR6; } (Val) = Stack[Pos - (n) - 1]; } while (0)
	typedef enum _DW_EXPR_OP
	{
		DW_OP_addr	= 0x03,
		DW_OP_deref   = 0x06,
		DW_OP_const1u = 0x08,
		DW_OP_const1s, DW_OP_const2u, DW_OP_const2s, DW_OP_const4u, DW_OP_const4s, DW_OP_const8u, DW_OP_const8s, DW_OP_constu, DW_OP_consts,
		DW_OP_dup, DW_OP_drop, DW_OP_over, DW_OP_pick, DW_OP_swap, DW_OP_rot, DW_OP_xderef,
		DW_OP_abs, DW_OP_and, DW_OP_div, DW_OP_minus, DW_OP_mod, DW_OP_mul, DW_OP_neg, DW_OP_not, DW_OP_or, DW_OP_plus, DW_OP_plus_uconst, DW_OP_shl, DW_OP_shr, DW_OP_shra, DW_OP_xor,
		DW_OP_bra, DW_OP_eq, DW_OP_ge, DW_OP_gt, DW_OP_le, DW_OP_lt, DW_OP_ne, DW_OP_skip,
		DW_OP_lit0,  DW_OP_lit1,  DW_OP_lit2,  DW_OP_lit3,  DW_OP_lit4,  DW_OP_lit5,  DW_OP_lit6,  DW_OP_lit7,
		DW_OP_lit8,  DW_OP_lit9,  DW_OP_lit10, DW_OP_lit11, DW_OP_lit12, DW_OP_lit13, DW_OP_lit14, DW_OP_lit15,
		DW_OP_lit16, DW_OP_lit17, DW_OP_lit18, DW_OP_lit19, DW_OP_lit20, DW_OP_lit21, DW_OP_lit22, DW_OP_lit23,
		DW_OP_lit24, DW_OP_lit25, DW_OP_lit26, DW_OP_lit27, DW_OP_lit28, DW_OP_lit29, DW_OP_lit30, DW_OP_lit31,
		DW_OP_reg0,  DW_OP_reg1,  DW_OP_reg2,  DW_OP_reg3,  DW_OP_reg4,  DW_OP_reg5,  DW_OP_reg6,  DW_OP_reg7,
		DW_OP_reg8,  DW_OP_reg9,  DW_OP_reg10, DW_OP_reg11, DW_OP_reg12, DW_OP_reg13, DW_OP_reg14, DW_OP_reg15,
		DW_OP_reg16, DW_OP_reg17, DW_OP_reg18, DW_OP_reg19, DW_OP_reg20, DW_OP_reg21, DW_OP_reg22, DW_OP_reg23,
		DW_OP_reg24, DW_OP_reg25, DW_OP_reg26, DW_OP_reg27, DW_OP_reg28, DW_OP_reg29, DW_OP_reg30, DW_OP_reg31,
		DW_OP_breg0,  DW_OP_breg1,  DW_OP_breg2,  DW_OP_breg3,  DW_OP_breg4,  DW_OP_breg5,  DW_OP_breg6,  DW_OP_breg7,
		DW_OP_breg8,  DW_OP_breg9,  DW_OP_breg10, DW_OP_breg11, DW_OP_breg12, DW_OP_breg13, DW_OP_breg14, DW_OP_breg15,
		DW_OP_breg16, DW_OP_breg17, DW_OP_breg18, DW_OP_breg19, DW_OP_breg20, DW_OP_breg21, DW_OP_breg22, DW_OP_breg23,
		DW_OP_breg24, DW_OP_breg25, DW_OP_breg26, DW_OP_breg27, DW_OP_breg28, DW_OP_breg29, DW_OP_breg30, DW_OP_breg31,
		DW_OP_regx,
		DW_OP_fbreg,
		DW_OP_bregx,
		DW_OP_piece,
		DW_OP_deref_size,
		DW_OP_xderef_size,
		DW_OP_nop,
		DW_OP_push_object_address,
		DW_OP_call2,
		DW_OP_call4,
		DW_OP_call_ref,
		DW_OP_from_tls_addr,
		DP_OP_call_frame_cfa,
		DW_OP_bit_piece,
		DW_OP_implicit_val,
		DW_OP_stack_val,
		DW_OP_lo_user = 0xE0,
		DW_OP_GNU_uninit = 0xF0,
		DW_OP_GNU_encoded_addr,
		DW_OP_GNU_implicit_ptr,
		DW_OP_GNU_entry_val,
		DW_OP_GNU_const_val,
		DW_OP_GNU_regval_type,
		DW_OP_GNU_deref_type,
		DW_OP_GNU_convert,
		DW_OP_hi_user = 0xFF
	} DW_EXPR_OP;
	TSIZE Val, Val2, Val3, Stack[256];
	TUINT Idx, Op, Pos = 0;
	const void *pEnd;

	STK_PUSH(pRegAry[REG_SP]);
	for (pEnd = (char *)p + Len; p < pEnd; ) {
		Op = *(unsigned char *)p;
		p = (char *)p + 1;
		switch (Op) {
		/* 数值压栈指令 */
		case DW_OP_addr:	Val = OFFVAL_ADDR(p, 0); p = (char *)p + sizeof(TADDR); break;
		case DW_OP_const1u: Val = OFFVAL_U1(p, 0); p = (char *)p + 1; break;
		case DW_OP_const1s: Val = OFFVAL_S1(p, 0); p = (char *)p + 1; break;
		case DW_OP_const2u: Val = OFFVAL_U2(p, 0); p = (char *)p + 2; break;
		case DW_OP_const2s: Val = OFFVAL_S2(p, 0); p = (char *)p + 2; break;
		case DW_OP_const4u: Val = OFFVAL_U4(p, 0); p = (char *)p + 4; break;
		case DW_OP_const4s: Val = OFFVAL_S4(p, 0); p = (char *)p + 4; break;
#ifdef SST64
		case DW_OP_const8u: Val = OFFVAL_U8(p, 0); p = (char *)p + 8; break;
		case DW_OP_const8s: Val = OFFVAL_S8(p, 0); p = (char *)p + 8; break;
#endif
		case DW_OP_constu: p = Getleb128(&Val, p); break;
		case DW_OP_consts: p = Getsleb128((TSSIZE *)&Val, p); break;
		case DW_OP_deref:
		case DW_OP_deref_size:
			STK_POP(Val);
			if (DumpMem(Val, &Val, sizeof(Val)))
				goto _ERR4;
			if (Op != DW_OP_deref) {
				switch (*(char *)p) {
				case 1: Val = Val&0xFF; break;
				case 2: Val = Val&0xFFFF; break;
				case 3: Val = Val&0xFFFFFF; break;
				case 4: Val = Val&0xFFFFFFFF; break;
#ifdef SST64
				case 5: Val = Val&0xFFFFFFFFFF; break;
				case 6: Val = Val&0xFFFFFFFFFFFF; break;
				case 7: Val = Val&0xFFFFFFFFFFFFFF; break;
				case 8: break;
#endif
				default: goto _ERR5;
				}
				p = (char *)p + 1;
			}
			break;
		/* 栈操作指令 */
		case DW_OP_dup:  STK_PICK(Val, 0); break;
		case DW_OP_drop: STK_POP(Val); goto NO_PUSH;
		case DW_OP_over: STK_PICK(Val, 1); break;
		case DW_OP_pick: STK_PICK(Val, (TUINT)*(unsigned char *)p); p = (char *)p + 1; break;
		case DW_OP_swap: STK_POP(Val); STK_POP(Val2); STK_PUSH(Val); STK_PUSH(Val2); goto NO_PUSH;
		case DW_OP_rot:  STK_POP(Val); STK_POP(Val2); STK_POP(Val3); STK_PUSH(Val); STK_PUSH(Val3); STK_PUSH(Val2); goto NO_PUSH;
		/* 算术指令 */
		case DW_OP_abs:  STK_POP(Val); if ((TSSIZE)Val < 0) Val = -(TSSIZE)Val; break;
		case DW_OP_and:  STK_POP(Val); STK_POP(Val2); Val &= Val2; break;
		case DW_OP_div:  STK_POP(Val); STK_POP(Val2); if (Val) Val = (TSSIZE)Val2 / (TSSIZE)Val; break;
		case DW_OP_minus: STK_POP(Val); STK_POP(Val2); Val = Val2 - Val; break;
		case DW_OP_mod:  STK_POP(Val); STK_POP(Val2); if (Val) Val = Val2 % Val; break;
		case DW_OP_mul:  STK_POP(Val); STK_POP(Val2); if (Val) Val = Val2 * Val; break;
		case DW_OP_neg:  STK_POP(Val); Val = -(TSSIZE)Val; break;
		case DW_OP_not:  STK_POP(Val); Val = ~Val; break;
		case DW_OP_or:   STK_POP(Val); STK_POP(Val2); Val |= Val2; break;
		case DW_OP_plus: STK_POP(Val); STK_POP(Val2); Val += Val2; break;
		case DW_OP_plus_uconst: p = Getleb128(&Val, p); STK_POP(Val2); Val += Val2; break;
		case DW_OP_shl:  STK_POP(Val); STK_POP(Val2); Val = Val2 << Val; break;
		case DW_OP_shr:  STK_POP(Val); STK_POP(Val2); Val = Val2 >> Val; break;
		case DW_OP_shra: STK_POP(Val); STK_POP(Val2); Val = (TSSIZE)Val2 >> Val; break;
		case DW_OP_xor:  STK_POP(Val); STK_POP(Val2); Val ^= Val2; break;
		/* 流程控制指令 */
		case DW_OP_bra: STK_POP(Val); p = (char *)p + 2 + (Val ? *(short *)p : 0); goto NO_PUSH;
		case DW_OP_eq:  STK_POP(Val); STK_POP(Val2); Val = (Val2 == Val); break;
		case DW_OP_ge:  STK_POP(Val); STK_POP(Val2); Val = ((TSSIZE)Val2 >= (TSSIZE)Val); break;
		case DW_OP_gt:  STK_POP(Val); STK_POP(Val2); Val = ((TSSIZE)Val2 > (TSSIZE)Val); break;
		case DW_OP_le:  STK_POP(Val); STK_POP(Val2); Val = ((TSSIZE)Val2 <= (TSSIZE)Val); break;
		case DW_OP_lt:  STK_POP(Val); STK_POP(Val2); Val = ((TSSIZE)Val2 < (TSSIZE)Val); break;
		case DW_OP_ne:  STK_POP(Val); STK_POP(Val2); Val = (Val2 != Val); break;
		case DW_OP_skip: p = (char *)p + *(short *)p + 2; goto NO_PUSH;
		/* 常量压栈指令 */
		case DW_OP_lit0:  case DW_OP_lit1:  case DW_OP_lit2:  case DW_OP_lit3:  case DW_OP_lit4:  case DW_OP_lit5:  case DW_OP_lit6:  case DW_OP_lit7:
		case DW_OP_lit8:  case DW_OP_lit9:  case DW_OP_lit10: case DW_OP_lit11: case DW_OP_lit12: case DW_OP_lit13: case DW_OP_lit14: case DW_OP_lit15:
		case DW_OP_lit16: case DW_OP_lit17: case DW_OP_lit18: case DW_OP_lit19: case DW_OP_lit20: case DW_OP_lit21: case DW_OP_lit22: case DW_OP_lit23:
		case DW_OP_lit24: case DW_OP_lit25: case DW_OP_lit26: case DW_OP_lit27: case DW_OP_lit28: case DW_OP_lit29: case DW_OP_lit30: case DW_OP_lit31:
			Val = Op - DW_OP_lit0;
			break;
		/* 值返回指令(保存在寄存器中) */
		case DW_OP_reg0:  case DW_OP_reg1:  case DW_OP_reg2:  case DW_OP_reg3:  case DW_OP_reg4:  case DW_OP_reg5:  case DW_OP_reg6:  case DW_OP_reg7:
		case DW_OP_reg8:  case DW_OP_reg9:  case DW_OP_reg10: case DW_OP_reg11: case DW_OP_reg12: case DW_OP_reg13: case DW_OP_reg14: case DW_OP_reg15:
		case DW_OP_reg16: case DW_OP_reg17: case DW_OP_reg18: case DW_OP_reg19: case DW_OP_reg20: case DW_OP_reg21: case DW_OP_reg22: case DW_OP_reg23:
		case DW_OP_reg24: case DW_OP_reg25: case DW_OP_reg26: case DW_OP_reg27: case DW_OP_reg28: case DW_OP_reg29: case DW_OP_reg30: case DW_OP_reg31:
		case DW_OP_regx:
			if (Op == DW_OP_regx) {
				p = Getleb128(&Val, p);
				Idx = (TUINT)Val;
			} else {
				Idx = Op - DW_OP_reg0;
			}
			if (!(RegMask&((TSIZE)1 << Idx)))
				return 1;
			*pRt = pRegAry[Idx];
			return 0;
		/* 寄存器压栈指令 */
		case DW_OP_breg0:  case DW_OP_breg1:  case DW_OP_breg2:  case DW_OP_breg3:  case DW_OP_breg4:  case DW_OP_breg5:  case DW_OP_breg6:  case DW_OP_breg7:
		case DW_OP_breg8:  case DW_OP_breg9:  case DW_OP_breg10: case DW_OP_breg11: case DW_OP_breg12: case DW_OP_breg13: case DW_OP_breg14: case DW_OP_breg15:
		case DW_OP_breg16: case DW_OP_breg17: case DW_OP_breg18: case DW_OP_breg19: case DW_OP_breg20: case DW_OP_breg21: case DW_OP_breg22: case DW_OP_breg23:
		case DW_OP_breg24: case DW_OP_breg25: case DW_OP_breg26: case DW_OP_breg27: case DW_OP_breg28: case DW_OP_breg29: case DW_OP_breg30: case DW_OP_breg31:
		case DW_OP_bregx:
			if (Op == DW_OP_regx) {
				p = Getleb128(&Val, p);
				Idx = (TUINT)Val;
			} else {
				Idx = Op - DW_OP_breg0;
			}
			if (!(RegMask&((TSIZE)1 << Idx)))
				return 1;
			p = Getsleb128((TSSIZE *)&Val, p);
			Val += pRegAry[Idx];
			break;
		case DW_OP_nop: goto NO_PUSH;
		case DW_OP_GNU_entry_val:
		case DW_OP_fbreg:
		case DW_OP_stack_val:
			return 1;
		default:
			SSTWARN(RS_UNKNOWN_DWOP, pFileName, pSegName, Op);
			return 1;
		}
		STK_PUSH(Val);
NO_PUSH:;
	}
	STK_POP(*pRt);
	return 0;
_ERR6:
	SSTWARN(RS_INVALID_IDX, pFileName, pSegName, GetCStr(RS_STACK), Pos);
	return 1;
_ERR5:
	SSTWARN(RS_INVALID_IDX, pFileName, pSegName, GetCStr(RS_SIZE), *(char *)p);
	return 1;
_ERR4:
	SSTWARN(RS_EADDR, pFileName, pSegName, Val);
	return 1;
_ERR2:
	SSTWARN(RS_DWEXPROP_UNDERFLOW, pFileName, pSegName);
	return 1;
_ERR1:
	SSTWARN(RS_DWEXPROP_OVERFLOW, pFileName, pSegName);
	return 1;
}
