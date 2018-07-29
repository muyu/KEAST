/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	armdis.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/11/13
* Description: arm反汇编
**********************************************************************************/
#include <string.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include <armdis.h>


#define VAL_VAL(Type, Val, SBit, Width)	(((Val) >> (SBit))&(((Type)1 << (Width)) - 1))
#define VAL_SVAL(Type, Val, SBit, Width) ((Type)((Val) << (sizeof(Val) * 8 - (SBit) - (Width))) >> (sizeof(Val) * 8 - (Width)))
#define FLAG_VAL(Flag, SBit, Width)		((Flag)&((((TUINT)1 << (Width)) - 1) << (SBit)))
#define INST_VAL(SBit, Width)	VAL_VAL(TUINT, Inst, SBit, Width)
#define INST_SVAL(SBit, Width)	VAL_SVAL(TINT, Inst, SBit, Width)
#define INST_FLAG(Bit, Flag)	FLAG_TRANS(Inst, (TUINT)1 << (Bit), Flag) /* 对应bit为1时返回flag，否则为0 */
#define INST_NFLAG(Bit, Flag)	(FLAG_TRANS(Inst, (TUINT)1 << (Bit), Flag)^(Flag)) /* 对应bit为0时返回flag，否则为0 */
#define INST_BIT(Bit)			(Inst&(1 << Bit))
#define SEXT_VAL(Val, Width)	VAL_SVAL(TSSIZE, Val, 0, Width)
#define SET_BIT(Val, Bit)		((Val) |= ((TSIZE)1 << (Bit)))
#define CLR_BIT(Val, Bit)		((Val) &= ~((TSIZE)1 << (Bit)))
#define TST_BIT(Val, Bit)		((Val)&((TSIZE)1 << (Bit)))

/* 通用标志位 */
#define REG_ZR	REG_NUM /* 0寄存器 */
/* Rd标志和尺寸 */
#define RDS_B	4
#define RDS_SZ	(Flag&0x7)
#define RDS_1B	0 /* 目标寄存器宽度 */
#define RDS_2B	1
#define RDS_4B	2
#define RDS_8B	3
#define RDS_16B	4
#define RDS_VFP	(1 << 3) /* 目标寄存器类型 */
#define RDS_VB	(RDS_VFP|RDS_1B)
#define RDS_VH	(RDS_VFP|RDS_2B)
#define RDS_VS	(RDS_VFP|RDS_4B)
#define RDS_VD	(RDS_VFP|RDS_8B)
#define RDS_VQ	(RDS_VFP|RDS_16B)
#ifdef SST64
#define REG_DEFB	RDS_8B
#else
#define REG_DEFB	RDS_4B
#endif
/* Rm标志 */
#define RMS_B	2
#define RMS_S	RDS_B
#define RMS_SZ	VAL_VAL(TUINT, Flag, RMS_S, RMS_B)
#define RMS_1B	(0 << RMS_S)
#define RMS_2B	(1 << RMS_S)
#define RMS_4B	(2 << RMS_S)
#define RMS_8B	(3 << RMS_S)
#define RDM_4B	(RDS_4B|RMS_4B)
#define RDM_8B	(RDS_8B|RMS_8B)
/* Rm操作符 */
#define RMO_B		4
#define RMO_S		(RMS_S + RMS_B)
#define RMO_VAL		FLAG_VAL(Flag, RMO_S, RMO_B)
#define RMO_IDX		VAL_VAL(TUINT, Flag, RMO_S, RMO_B)
#define RMO_NULL	(0 << RMO_S) /* 无操作 */
#define RMO_LSL		(1 << RMO_S)
#define RMO_LSR		(2 << RMO_S)
#define RMO_ASR		(3 << RMO_S)
#define RMO_ROR		(4 << RMO_S)
#define RMO_RRX		(5 << RMO_S) /* 仅存在于A32 */
#define RMO_UXTB	(6 << RMO_S)
#define RMO_UXTH	(7 << RMO_S)
#define RMO_UXTW	(8 << RMO_S)
#define RMO_SXTB	(9 << RMO_S)
#define RMO_SXTH	(10 << RMO_S)
#define RMO_SXTW	(11 << RMO_S)

/* 条件执行 */
#define PSR_B	4
#define PSR_S	(RMO_S + RMO_B)
#define PSR_VAL	FLAG_VAL(Flag, PSR_S, PSR_B)
#define PSR_IDX	VAL_VAL(TUINT, Flag, PSR_S, PSR_B)
#define PSR_EQ	(0 << PSR_S)
#define PSR_NE	(1 << PSR_S)
#define PSR_CS	(2 << PSR_S)
#define PSR_CC	(3 << PSR_S)
#define PSR_MI	(4 << PSR_S)
#define PSR_PL	(5 << PSR_S)
#define PSR_VS	(6 << PSR_S)
#define PSR_VC	(7 << PSR_S)
#define PSR_HI	(8 << PSR_S)
#define PSR_LS	(9 << PSR_S)
#define PSR_GE	(10 << PSR_S)
#define PSR_LT	(11 << PSR_S)
#define PSR_GT	(12 << PSR_S)
#define PSR_LE	(13 << PSR_S)
#define PSR_AL	(14 << PSR_S)
#define PSR_NV	(15 << PSR_S)


typedef const struct _DC_INSTFUN
{
	int (*Undef)(TUINT Inst, int IsUnImpl, void *pArg);

	/*************************************************************************
	* DESCRIPTION
	*	处理数据指令:
	*		$DP Rd, Rb, Rm, RmOp, Val
	*		$DP Rd, Rb, Rm, RmOp, Rs
	*		$DP Rb, Rm, RmOp, Val; 只更新CPSR
	*
	* PARAMETERS
	*	Flag: 标志
	*	Rd: 目标寄存器, 有DPF_PSR时可能为REG_ZR
	*	Rb: 源寄存器, 可能为REG_ZR
	*	Val: 立即数, 有DPF_RM时为Rm寄存器
	*	Val2: 第2立即数: 仅用于DPO_MVK/DPO_BTST/DPO_CCMN, 有DPF_RS时为Rs寄存器
	*	pArg: 参数
	*
	* RETURNS
	*	0: 正常
	*	1: 异常
	*
	* LOCAL AFFECTED
	*
	*************************************************************************/
/* 数据标记 */
#define DPF_B	9
#define DPF_S	(PSR_S + PSR_B)
#define DPF_PSR	(1 << (DPF_S + 0)) /* 更新CPSR */
#define DPF_LINK	(1 << (DPF_S + 1)) /* call with link */
#define DPF_XCHANGE	(1 << (DPF_S + 2)) /* A32/T32切换 */
#define DPF_CRY	(1 << (DPF_S + 3)) /* 加减法进位 */
#define DPF_SIG	(1 << (DPF_S + 4)) /* 符号位扩展 */
#define DPF_RM	(1 << (DPF_S + 5)) /* Val为Rm */
#define DPF_RS	(1 << (DPF_S + 6)) /* Val2为Rs, 尺寸和Rd一样 */
#define DPF_INV	(1 << (DPF_S + 7)) /* Rm运算结果取反 */
#define DPF_INC	(1 << (DPF_S + 8)) /* Rm运算结果+1 */
#define DPF_NEG	(DPF_INC|DPF_INV) /* Rm运算结果取负, 仅用于DPO_ADD/DPO_CSEL */
/* 数据操作符 */
#define DPO_B		6
#define DPO_S		(DPF_S + DPF_B)
#define DPO_VAL		FLAG_VAL(Flag, DPO_S, DPO_B)
#define DPO_IDX		VAL_VAL(TUINT, Flag, DPO_S, DPO_B)
#define DPO_NULL	(0 << DPO_S)
#define DPO_ADD		(1 << DPO_S)
#define DPO_RSB		(2 << DPO_S) /* 仅存在于A32, Rb取负 */
#define DPO_AND		(3 << DPO_S)
#define DPO_ORR		(4 << DPO_S)
#define DPO_EOR		(5 << DPO_S)
#define DPO_ADRP	(6 << DPO_S)
#define DPO_MVK		(7 << DPO_S)
#define DPO_LSL		(8 << DPO_S)
#define DPO_LSR		(9 << DPO_S)
#define DPO_ASR		(10 << DPO_S)
#define DPO_ROR		(11 << DPO_S)
#define DPO_EXT		(12 << DPO_S)
#define DPO_SXTB	(13 << DPO_S)
#define DPO_SXTH	(14 << DPO_S)
#define DPO_SXTW	(15 << DPO_S)
#define DPO_UXTB	(16 << DPO_S)
#define DPO_UXTH	(17 << DPO_S)
#define DPO_BFM		(18 << DPO_S)
#define DPO_SBFM	(19 << DPO_S)
#define DPO_UBFM	(20 << DPO_S)
#define DPO_CCMN	(21 << DPO_S)
#define DPO_CSEL	(22 << DPO_S)
#define DPO_RBIT	(23 << DPO_S)
#define DPO_REV		(24 << DPO_S)
#define DPO_REV16	(25 << DPO_S)
#define DPO_REV32	(26 << DPO_S)
#define DPO_CLZ		(27 << DPO_S)
#define DPO_CLS		(28 << DPO_S)
#define DPO_MADD	(29 << DPO_S)
#define DPO_MULH	(30 << DPO_S)
#define DPO_BCPR	(31 << DPO_S)
#define DPO_BTST	(32 << DPO_S)
/* 别名指令, 仅用于反汇编 */
#define DPO_MOV		(33 << DPO_S)
#define DPO_ADC		(34 << DPO_S)
#define DPO_SUB		(35 << DPO_S)
#define DPO_SBC		(36 << DPO_S)
#define DPO_BIC		(37 << DPO_S)
#define DPO_ORN		(38 << DPO_S)
#define DPO_EON		(39 << DPO_S)
#define DPO_CMN		(40 << DPO_S)
#define DPO_CMP		(41 << DPO_S)
#define DPO_TST		(42 << DPO_S)
#define DPO_TEQ		(43 << DPO_S)
#define DPO_MVN		(44 << DPO_S)
#define DPO_NEG		(45 << DPO_S)
#define DPO_NGC		(46 << DPO_S)
#define DPO_CCMP	(47 << DPO_S)
#define DPO_CSINC	(48 << DPO_S)
#define DPO_CSINV	(49 << DPO_S)
#define DPO_CSNEG	(50 << DPO_S)
#define DPO_CSET	(51 << DPO_S)
#define DPO_CSETM	(52 << DPO_S)
#define DPO_MUL		(53 << DPO_S)
#define DPO_MNEG	(54 << DPO_S)
#define DPO_MSUB	(55 << DPO_S)
	int (*SetReg)(TUINT Flag, TINT Rd, TINT Rb, TSSIZE Val, TUINT Val2, void *pArg);

	/*************************************************************************
	* DESCRIPTION
	*	处理存取指令:
	*		LDR/STR Rd, [Rb], +/-Rm, RmOp Val
	*		LDR/STR Rd, [Rb, +/-Rm, RmOp Val]
	*		LDM/STM Rd, {Val}
	*
	* PARAMETERS
	*	Flag: 标志
	*	Rd: 目标寄存器, 无LSF_LD时可能为REG_ZR, 存在LSF_BLK则为寄存器位图!
	*	Rd2: 目标寄存器, 可能为REG_ZR
	*	Rb: 源寄存器
	*	Rm: 第2寄存器, 可能为REG_ZR
	*	Val: 立即数
	*	pArg: 参数
	*
	* RETURNS
	*	0: 正常
	*	1: 异常
	*
	* LOCAL AFFECTED
	*
	*************************************************************************/
/* 存取标记 */
#define LSF_B	7
#define LSF_S	(PSR_S + PSR_B)
#define LSF_LD	(1 << (LSF_S + 0)) /* 加载 */
#define LSF_WB	(1 << (LSF_S + 1)) /* 回写, post index没有该标志也要回写! */
#define LSF_SIG	(1 << (LSF_S + 2)) /* 符号位扩展 */
#define LSF_POST (1 << (LSF_S + 3)) /* post index, 否则为pre index */
#define LSF_NEG	(1 << (LSF_S + 4)) /* 仅存在于A32, Rm运算结果取负 */
#define LSF_EX	(1 << (LSF_S + 5)) /* EX, Rm做状态寄存器 */
#define LSF_BLK	(1 << (LSF_S + 6)) /* 块存取, LDMDA/DB/IA/IB, STMDA/DB/IA/IB */
#define LSF_USR	(1 << (LSF_S + 5)) /* 仅用于存在LSF_BLK时, 存取user mode寄存器 */
#define LSF_BKD	(1 << (LSF_S + 4)) /* 仅用于存在LSF_BLK时, 存取递减 */
#define LSF_BKB	(1 << (LSF_S + 3)) /* 仅用于存在LSF_BLK时, 先加减再存取 */
/* 存取类型 */
#define LST_B	4
#define LST_S	(LSF_S + LSF_B)
#define LST_VAL	FLAG_VAL(Flag, LST_S, LST_B)
#define LST_IDX	VAL_VAL(TUINT, Flag, LST_S, LST_B)
#define LST_NULL (0 << LST_S)
#define LST_EX	(1 << LST_S) /* EX, Rm做状态寄存器 */
#define LST_AX	(2 << LST_S) /* AcquireRelease EX, Rm做状态寄存器 */
#define LST_AR	(3 << LST_S) /* AcquireRelease */
#define LST_PM	(4 << LST_S) /* 预取 */
#define LST_NT	(5 << LST_S) /* 仅存在于A64, non-temporal */
#define LST_UI	(6 << LST_S) /* 仅存在于A64, unscaled immediate */
#define LST_UR	(7 << LST_S) /* 仅存在于A64/A32/T32, unprivileged */
#define LST_DA	(8 << LST_S) /* 仅存在于A32/T32/T16, LDMDA/STMDA */
#define LST_DB	(9 << LST_S) /* 仅存在于A32/T32/T16, LDMDB/STMDB */
#define LST_IA	(10 << LST_S) /* 仅存在于A32/T32/T16, LDMIA/STMIA */
#define LST_IB	(11 << LST_S) /* 仅存在于A32/T32/T16, LDMIB/STMIB */
#define LST_TB	(12 << LST_S) /* 仅存在于T32, TBB/TBH */
/* 数据尺寸 */
#define LSS_B	3
#define LSS_S	(LST_S + LST_B)
#define LSS_SZ	VAL_VAL(TUINT, Flag, LSS_S, LSS_B)
#define LSS_1B	(0 << LSS_S)
#define LSS_2B	(1 << LSS_S)
#define LSS_4B	(2 << LSS_S)
#define LSS_8B	(3 << LSS_S)
#define LSS_16B	(4 << LSS_S)
#define LSS_UB	(RDS_4B|LSS_1B)
#define LSS_SB	(RDS_4B|LSS_1B|LSF_SIG)
#define LSS_UH	(RDS_4B|LSS_2B)
#define LSS_SH	(RDS_4B|LSS_2B|LSF_SIG)
#define LSS_UW	(RDS_4B|LSS_4B)
#define LSS_SW	(RDS_8B|LSS_4B|LSF_SIG)
#define LSS_UX	(RDS_8B|LSS_8B)
#define LSS_VB	(RDS_VB|LSS_1B)
#define LSS_VH	(RDS_VH|LSS_2B)
#define LSS_VS	(RDS_VS|LSS_4B)
#define LSS_VD	(RDS_VD|LSS_8B)
#define LSS_VQ	(RDS_VQ|LSS_16B)
	int (*LoadStore)(TUINT Flag, TINT Rd, TINT Rd2, TINT Rb, TINT Rm, TINT Val, void *pArg);

	/*************************************************************************
	* DESCRIPTION
	*	处理杂项指令:
	*		$MIT #immed
	*		......
	*
	* PARAMETERS
	*	Flag: 标志
	*	Val: 立即数
	*	pArg: 参数
	*
	* RETURNS
	*	0: 正常
	*	1: 异常
	*
	* LOCAL AFFECTED
	*
	*************************************************************************/
/* 杂项类型 */
#define MIT_B		5
#define MIT_S		(PSR_S + PSR_B)
#define MIT_VAL		FLAG_VAL(Flag, MIT_S, MIT_B)
#define MIT_IDX		VAL_VAL(TUINT, Flag, MIT_S, MIT_B)
#define MIT_SVC		(0 << MIT_S)
#define MIT_HVC		(1 << MIT_S)
#define MIT_SMC		(2 << MIT_S)
#define MIT_BRK		(3 << MIT_S)
#define MIT_HLT		(4 << MIT_S)
#define MIT_DCP1	(5 << MIT_S)
#define MIT_DCP2	(6 << MIT_S)
#define MIT_DCP3	(7 << MIT_S)
#define MIT_NOP		(8 << MIT_S)
#define MIT_YIELD	(9 << MIT_S)
#define MIT_WFE		(10 << MIT_S)
#define MIT_WFI		(11 << MIT_S)
#define MIT_SEV		(12 << MIT_S)
#define MIT_SEVL	(13 << MIT_S)
#define MIT_CLREX	(14 << MIT_S)
#define MIT_DSB		(15 << MIT_S)
#define MIT_DMB		(16 << MIT_S)
#define MIT_ISB		(17 << MIT_S)
#define MIT_MSR1	(18 << MIT_S) /* MSR SPSel */
#define MIT_MSR2	(19 << MIT_S) /* MSR DAIFSet */
#define MIT_MSR3	(20 << MIT_S) /* MSR DAIFClr */
#define MIT_SETEND	(21 << MIT_S)
#define MIT_IT		(22 << MIT_S)
#define MIT_DRPS	(23 << MIT_S)
#define MIT_ERET	(24 << MIT_S)
	int (*Misc)(TUINT Flag, TUINT Val, void *pArg);
} DC_INSTFUN;

typedef const struct _INST_DISPATCH
{
	TUINT Mask, Val, tFlag;
	int (*InstDec)(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg);
} INST_DISPATCH;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	解码未完成指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int InstUnImplement(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	(void)tFlag;
	return pExeIFun->Undef(Inst, 1, pArg);
}

static INST_DISPATCH ArmOpUnImpl[] =
{
	{0x00000000, 0x00000000, 0, InstUnImplement},
};
static const TUINT RmFMap2[] = {RMO_LSL, RMO_LSR, RMO_ASR, RMO_ROR};

#ifdef SST64
#define A64_SREG(sbit) INST_VAL(sbit, 5)
#define A64_REG(sbit)  A64RegMap[A64_SREG(sbit)]

static TINT A64RegMap[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,REG_ZR};
static const TUINT A64DpAS[4] = {RDM_4B|DPO_ADD, RDM_4B|DPO_ADD|DPF_NEG, RDM_8B|DPO_ADD, RDM_8B|DPO_ADD|DPF_NEG};
static const TUINT A64DpSz[2] = {RDM_4B, RDM_8B};
static const TUINT A64DpLg[8] =
{
	RDM_4B|DPO_AND, RDM_4B|DPO_ORR, RDM_4B|DPO_EOR, RDM_4B|DPO_AND|DPF_PSR,
	RDM_8B|DPO_AND, RDM_8B|DPO_ORR, RDM_8B|DPO_EOR, RDM_8B|DPO_AND|DPF_PSR,
};
static const TUINT A64LsSz[2][2][2][4] =
{
	{{{LSS_UB, LSS_UH, LSS_UW, LSS_UX}, {LSF_LD|RDS_8B|LSS_1B|LSF_SIG, LSF_LD|RDS_8B|LSS_2B|LSF_SIG, LSF_LD|LSS_SW, LSF_LD|LSS_UX|LST_PM}},
	{{LSS_VB, LSS_VH, LSS_VS, LSS_VD}, {LSS_VQ}}},
	{{{LSF_LD|LSS_UB, LSF_LD|LSS_UH, LSF_LD|LSS_UW, LSF_LD|LSS_UX}, {LSF_LD|LSS_SB, LSF_LD|LSS_SH, 0, LSF_LD|LSS_UW|LST_PM}},
	{{LSF_LD|LSS_VB, LSF_LD|LSS_VH, LSF_LD|LSS_VS, LSF_LD|LSS_VD}, {LSF_LD|LSS_VQ}}},
};

/*************************************************************************
* DESCRIPTION
*	解码PC-rel. addressing指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcPC(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TINT Val = (INST_SVAL(5, 19) << 2)|INST_VAL(29, 2);

	if (INST_BIT(31)) {
		Val <<= PAGE_BITS;
		tFlag |= DPO_ADRP;
	} else {
		tFlag |= DPO_ADD;
	}
	return pExeIFun->SetReg(tFlag, A64_REG(0), REG_PC, Val, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Add/subtract (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcADI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TUINT Val = INST_VAL(10, 12);
	
	if (INST_BIT(23))
		return pExeIFun->Undef(Inst, 0, pArg);
	if (INST_BIT(22))
		Val <<= PAGE_BITS;
	return pExeIFun->SetReg(tFlag|INST_FLAG(29, DPF_PSR)|A64DpAS[INST_VAL(30, 2)]
	  , INST_BIT(29) ? A64_REG(0) : (TINT)A64_SREG(0), A64_SREG(5), Val, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Logical (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcLI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TUINT ImmS = INST_VAL(10, 6), ImmR = INST_VAL(16, 6), RWidth = 32 << INST_VAL(31, 1), Width, Mask;
	TSIZE Val;
	
	if (INST_BIT(22)) {
		Width = 64;
	} else {
		for (Width = 32; Width >= 2 && (ImmS&Width); Width >>= 1);
	}
	Mask = Width - 1;
	ImmR &= Mask;
	ImmS &= Mask;
	if (ImmS == Mask || RWidth < Width)
		return pExeIFun->Undef(Inst, 0, pArg);
	Val = ((TSIZE)1 << (ImmS + 1)) - 1;
	Val = (Val >> ImmR)|((Val&(((TSIZE)1 << ImmR) - 1)) << (Width - ImmR));
	for (; Width < RWidth; Width <<= 1) {
		Val |= (Val << Width);
	}
	tFlag |= A64DpLg[INST_VAL(29, 3)];
	return pExeIFun->SetReg(tFlag, tFlag&DPF_PSR ? A64_REG(0) : (TINT)A64_SREG(0), A64_REG(5), Val, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Move wide (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcMW(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TUINT Val2 = INST_VAL(21, 2) << 4;
	TSIZE Val = (TSIZE)INST_VAL(5, 16) << Val2;
	TINT Rb;

	switch (INST_VAL(29, 2)) {
	case 0:
		Val = ~Val;
		if (!INST_BIT(31))
			Val &= 0xFFFFFFFF;
		/* 进入下一个case */
	case 2:
		Rb = REG_ZR;
		tFlag |= DPO_ADD;
		Val2 = 0;
		break;
	case 3:
		Val >>= Val2;
		Rb = A64_REG(0);
		tFlag |= DPO_MVK;
		break;
	default:
		return pExeIFun->Undef(Inst, 0, pArg);
	}
	if (!INST_BIT(31) && INST_BIT(22))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(tFlag|A64DpSz[INST_VAL(31, 1)], A64_REG(0), Rb, Val, Val2, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Bitfield指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcBF(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT FMapDpBF[4] = {DPO_SBFM, DPO_BFM, DPO_UBFM};
	const TUINT i = INST_VAL(29, 2), ImmS = INST_VAL(10, 6);
	TUINT ImmR = INST_VAL(16, 6), Flag = FMapDpBF[i];

	if (!Flag || INST_VAL(31, 1) != INST_VAL(22, 1) || (!INST_BIT(31) && (INST_BIT(15)|INST_BIT(21))))
		return pExeIFun->Undef(Inst, 0, pArg);
	if (Flag == DPO_SBFM) {
		if (ImmS == (0x1F|(INST_VAL(31, 1) << 5))) {
			Flag = DPO_ASR;
		} else if (!ImmR) {
			if (ImmS == 0x07)
				Flag = DPO_SXTB;
			else if (ImmS == 0x0F)
				Flag = DPO_SXTH;
			else if (ImmS == 0x1F)
				Flag = DPO_SXTW;
		}
	} else if (Flag == DPO_UBFM) {
		if (ImmS == (0x1F|(INST_VAL(31, 1) << 5))) {
			Flag = DPO_LSR;
		} else if (ImmS + 1 == ImmR) {
			Flag = DPO_LSL;
			ImmR = (32 << INST_VAL(31, 1)) - ImmR;
		} else if (!ImmR) {
			if (ImmS == 0x07)
				Flag = DPO_UXTB;
			else if (ImmS == 0x0F)
				Flag = DPO_UXTH;
		}
	}
	return pExeIFun->SetReg(tFlag|Flag|A64DpSz[INST_VAL(31, 1)], A64_REG(0), A64_REG(5), ImmR, Flag == FMapDpBF[i] ? ImmS : 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Extract指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcEX(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TINT Rm = A64_REG(16);
	TINT Rb = A64_REG(5);
	
	if ((Inst&0x60200000) || INST_VAL(31, 1) != INST_VAL(22, 1) || (!INST_BIT(31) && INST_BIT(15)))
		return pExeIFun->Undef(Inst, 0, pArg);
	tFlag |= A64DpSz[INST_VAL(31, 1)];
	if (Rb == Rm)
		return pExeIFun->SetReg(DPO_ROR|tFlag, A64_REG(0), Rb, INST_VAL(10, 6), 0, pArg);
	return pExeIFun->SetReg(DPO_EXT|DPF_RM|tFlag, A64_REG(0), Rb, Rm, INST_VAL(10, 6), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Unconditional branch (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64BranchUI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->SetReg(tFlag|INST_FLAG(31, DPF_LINK), REG_PC, REG_PC, INST_SVAL(0, 26) << 2, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Compare & branch (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64BranchCI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->SetReg(tFlag|(INST_BIT(24) ? PSR_NE : PSR_EQ)|A64DpSz[INST_VAL(31, 1)]
	  , REG_PC, A64_SREG(0), INST_SVAL(5, 19) << 2, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Test & branch (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64BranchTI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->SetReg(tFlag|(INST_BIT(24) ? PSR_NE : PSR_EQ)|A64DpSz[INST_VAL(31, 1)]
	  , REG_PC, A64_SREG(0), INST_SVAL(5, 14) << 2, INST_VAL(19, 5)|(INST_VAL(31, 1) << 5), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Conditional branch (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64BranchCI2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	if (Inst&0x01000010)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(tFlag|(INST_VAL(0, 4) << PSR_S), REG_PC, REG_PC, INST_SVAL(5, 19) << 2, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Exception generation指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64BranchEG(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	switch (INST_VAL(0, 5)|(INST_VAL(21, 3) << 5)) {
	case 0x01: tFlag |= MIT_SVC; break;
	case 0x02: tFlag |= MIT_HVC; break;
	case 0x03: tFlag |= MIT_SMC; break;
	case 0x20: tFlag |= MIT_BRK; break;
	case 0x40: tFlag |= MIT_HLT; break;
	case 0xA1: tFlag |= MIT_DCP1; break;
	case 0xA2: tFlag |= MIT_DCP2; break;
	case 0xA3: tFlag |= MIT_DCP3; break;
	default: return pExeIFun->Undef(Inst, 0, pArg);
	}
	return pExeIFun->Misc(tFlag, INST_VAL(5, 16), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码System指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64BranchSYS(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TUINT Val = INST_VAL(8, 4);

	switch (Inst&0x3FF0FF) {
	case 0x0040BF: tFlag |= MIT_MSR1; break;
	case 0x0340DF: tFlag |= MIT_MSR2; break;
	case 0x0340FF: tFlag |= MIT_MSR3; break;
	case 0x0320DF:
	case 0x0320FF:
	case 0x03201F: tFlag |= MIT_NOP; Val = 0; break;
	case 0x03203F: tFlag |= MIT_YIELD; Val = 0; break;
	case 0x03205F: tFlag |= MIT_WFE; Val = 0; break;
	case 0x03207F: tFlag |= MIT_WFI; Val = 0; break;
	case 0x03209F: tFlag |= MIT_SEV; Val = 0; break;
	case 0x0320BF: tFlag |= MIT_SEVL; Val = 0; break;
	case 0x03305F: tFlag |= MIT_CLREX; break;
	case 0x03309F: tFlag |= MIT_DSB; break;
	case 0x0330BF: tFlag |= MIT_DMB; break;
	case 0x0330DF: tFlag |= MIT_ISB; break;
	default: return pExeIFun->Undef(Inst, 1, pArg);
	}
	return pExeIFun->Misc(tFlag, Val, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Unconditional branch (register)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64BranchUR(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64BUr[8] = {DPO_ADD, DPO_ADD|DPF_LINK, DPO_ADD, 0, MIT_ERET, MIT_DRPS};
	const TUINT i = INST_VAL(21, 3), Flag = A64BUr[i];

	if (!Flag || (Inst&0x011FFC1F) != 0x001F0000)
		return pExeIFun->Undef(Inst, 0, pArg);
	if (i >= 4)
		return pExeIFun->Misc(tFlag, 0, pArg);
	return pExeIFun->SetReg(tFlag|Flag, REG_PC, A64_REG(5), 0, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/store exclusive指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64LoadStoreEX(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64LsExSz[4] = {LSS_UB, LSS_UH, LSS_UW, LSS_UX};
	static const TUINT A64LsExOp[2][2] = {{LSF_EX|LST_EX, LSF_EX|LST_AX}, {0, LST_AR}};

	return pExeIFun->LoadStore(tFlag|INST_FLAG(22, LSF_LD)|A64LsExOp[INST_VAL(23, 1)][INST_VAL(15, 1)]|A64LsExSz[INST_VAL(30, 2)]
	  , A64_REG(0), A64_REG(10), A64_SREG(5), A64_REG(16), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load register (literal)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64LoadRegPC(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64LsPcSz[2][4] = {{LSS_UW, LSS_UX, LSS_SW, LSS_UX|LST_PM}, {LSS_VS, LSS_VD, LSS_VQ}};
	const TUINT Flag = A64LsPcSz[INST_VAL(26, 1)][INST_VAL(30, 2)];

	if (!Flag)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(Flag|tFlag, A64_REG(0), REG_ZR, REG_PC, REG_ZR, INST_SVAL(5, 19) << 2, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/store no-allocate pair (offset)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64LoadStoreNT(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64LsNtSz[2][4] = {{LSS_UW, 0, LSS_UX}, {LSS_VS, LSS_VD, LSS_VQ}};
	const TUINT Flag = A64LsNtSz[INST_VAL(26, 1)][INST_VAL(30, 2)];
	
	if (!Flag)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(INST_FLAG(22, LSF_LD)|Flag|tFlag
	  , A64_REG(0), A64_REG(10), A64_SREG(5), REG_ZR, INST_SVAL(15, 7) << LSS_SZ, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/store register pair (pre-indexed)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64LoadStorePR(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64LsPrSz[2][2][4] =
	{
		{{LSS_UW, 0, LSS_UX}, {LSS_VS, LSS_VD, LSS_VQ}},
		{{LSF_LD|LSS_UW, LSF_LD|LSS_SW, LSF_LD|LSS_UX}, {LSF_LD|LSS_VS, LSF_LD|LSS_VD, LSF_LD|LSS_VQ}},
	};
	const TUINT Flag = A64LsPrSz[INST_VAL(22, 1)][INST_VAL(26, 1)][INST_VAL(30, 2)];
	
	if (!Flag)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(Flag|tFlag, A64_REG(0), A64_REG(10), A64_SREG(5), REG_ZR, INST_SVAL(15, 7) << LSS_SZ, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/store register (immediate pre-indexed)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64LoadStoreIM(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Flag = A64LsSz[INST_VAL(22, 1)][INST_VAL(26, 1)][INST_VAL(23, 1)][INST_VAL(30, 2)], Lst = FLAG_VAL(tFlag, LST_S, LST_B);

	if (!Flag)
		return pExeIFun->Undef(Inst, 0, pArg);
	if (Lst == LST_UI && LST_VAL)
		tFlag = tFlag^LST_UI;
	else if (LST_VAL || (Lst == LST_UR && INST_BIT(26)))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(Flag|tFlag, A64_REG(0), REG_ZR, A64_SREG(5), REG_ZR, INST_SVAL(12, 9), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/store register (register offset)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64LoadStoreRO(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64LsRoRmOp[2][4] =
	{
		{RMS_4B, RMS_4B|RMO_UXTW, RMS_8B, RMS_8B|RMO_LSL},
		{RMS_4B|RMO_SXTW, RMS_4B|RMO_SXTW, RMS_8B, RMS_8B|RMO_LSL},
	};
	const TUINT SzIdx = INST_VAL(30, 2), Flag = A64LsSz[INST_VAL(22, 1)][INST_VAL(26, 1)][INST_VAL(23, 1)][SzIdx];
	
	if (!Flag || !INST_BIT(14))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(A64LsRoRmOp[INST_VAL(15, 1)][INST_VAL(12, 2)]|Flag|tFlag
	  , A64_REG(0), REG_ZR, A64_SREG(5), A64_REG(16), INST_BIT(12) ? SzIdx : 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/store register (unsigned immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64LoadStoreUI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Flag = A64LsSz[INST_VAL(22, 1)][INST_VAL(26, 1)][INST_VAL(23, 1)][INST_VAL(30, 2)];

	if (!Flag)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(Flag|tFlag, A64_REG(0), REG_ZR, A64_SREG(5), REG_ZR, INST_VAL(10, 12) << LSS_SZ, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Logical (shifted register)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcLR(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Flag = tFlag|RmFMap2[INST_VAL(22, 2)]|A64DpLg[INST_VAL(29, 3)]|INST_FLAG(21, DPF_INV);
	
	if (!INST_BIT(31) && INST_BIT(15))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(Flag, A64_REG(0), A64_REG(5), A64_REG(16), INST_VAL(10, 6), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Add/subtract (shifted register)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcADS(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT RmFlag = RmFMap2[INST_VAL(22, 2)], Flag = tFlag|INST_FLAG(29, DPF_PSR)|RmFlag|A64DpAS[INST_VAL(30, 2)];
	
	if ((!INST_BIT(31) && INST_BIT(15)) || RmFlag == RMO_ROR)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(Flag, A64_REG(0), A64_REG(5), A64_REG(16), INST_VAL(10, 6), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Add/subtract (extended register)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcADE(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64DpAdeRm[8] =
	{
		RMS_4B|RMO_UXTB, RMS_4B|RMO_UXTH, RMS_4B|RMO_LSL, RMS_8B|RMO_LSL,
		RMS_4B|RMO_SXTB, RMS_4B|RMO_SXTH, RMS_4B|RMO_SXTW, RMS_8B|RMO_LSL,
	};
	const TUINT Val = INST_VAL(10, 3);
	
	if (Val > 4 || INST_VAL(22, 2))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(INST_FLAG(29, DPF_PSR)|tFlag|A64DpAdeRm[INST_VAL(13, 3)]
	  |(A64DpAS[INST_VAL(30, 2)]&~(((1 << RMS_B) - 1) << RMS_S)) /* 清除RM尺寸位 */
	  , INST_BIT(29) ? A64_REG(0) : (TINT)A64_SREG(0), A64_SREG(5), A64_REG(16), Val, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Add/subtract (with carry)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcADC(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Flag = INST_FLAG(29, DPF_PSR)|tFlag|A64DpAS[INST_VAL(30, 2)];
	
	if (INST_VAL(10, 6))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(Flag, A64_REG(0), A64_REG(5), A64_REG(16), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Conditional compare指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcCC(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64DpCcSz[4] = {RDM_4B, RDM_4B|DPF_NEG, RDM_8B, RDM_8B|DPF_NEG};
	const TUINT Flag = tFlag|(INST_VAL(12, 4) << PSR_S)|A64DpCcSz[INST_VAL(30, 2)];

	if ((Inst&0x20000410) != 0x20000000)
		return pExeIFun->Undef(Inst, 0, pArg);
	if (INST_BIT(11))
		return pExeIFun->SetReg(Flag, REG_ZR, A64_REG(5), INST_VAL(16, 5), INST_VAL(0, 4), pArg);
	return pExeIFun->SetReg(Flag|DPF_RM, REG_ZR, A64_REG(5), A64_REG(16), INST_VAL(0, 4), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Conditional select指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcCS(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64DpCsRmSz[4][2] =
	{
		{RDM_4B, RDM_4B|DPF_INC}, {RDM_4B|DPF_INV, RDM_4B|DPF_NEG}, {RDM_8B, RDM_8B|DPF_INC}, {RDM_8B|DPF_INV, RDM_8B|DPF_NEG},
	};

	if (Inst&0x20000800)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(tFlag|(INST_VAL(12, 4) << PSR_S)|A64DpCsRmSz[INST_VAL(30, 2)][INST_VAL(10, 1)]
	  , A64_REG(0), A64_REG(5), A64_REG(16), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Data-processing (1 source)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcSrc1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64DpS1[2][8] =
	{
		{RDM_4B|DPO_RBIT, RDM_4B|DPO_REV16, RDM_4B|DPO_REV, 0, RDM_4B|DPO_CLZ, RDM_4B|DPO_CLS, 0, 0},
		{RDM_8B|DPO_RBIT, RDM_8B|DPO_REV16, RDM_8B|DPO_REV32, RDM_8B|DPO_REV, RDM_8B|DPO_CLZ, RDM_8B|DPO_CLS, 0, 0},
	};

	if (Inst&0x201FE000)
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->SetReg(tFlag|A64DpS1[INST_VAL(31, 1)][INST_VAL(10, 3)], A64_REG(0), A64_REG(5), 0, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Data-processing (2 source)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcSrc2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A64DpS2[4] = { DPO_LSL, DPO_LSR, DPO_ASR, DPO_ROR };

	if (Inst&0x20008000)
		return pExeIFun->Undef(Inst, 0, pArg);
	if ((Inst&0xF000) != 0x2000)
		return pExeIFun->Undef(Inst, 1, pArg);
	return pExeIFun->SetReg(tFlag|A64DpS2[INST_VAL(10, 2)]|A64DpSz[INST_VAL(31, 1)], A64_REG(0), A64_REG(5), A64_REG(16), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Data-processing (3 source)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A64DataProcSrc3(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT A64DpS3[8] = {DPO_MADD|RMS_8B, DPF_SIG|DPO_MADD|RMS_4B, DPF_SIG|DPO_MULH|RMS_8B, 0, 0, DPO_MADD|RMS_4B, DPO_MULH|RMS_8B};
	const TUINT i = INST_VAL(21, 3), Flag = A64DpS3[i];
	
	if (INST_VAL(29, 2) || !Flag)
		return pExeIFun->Undef(Inst, 0, pArg);
	if (INST_BIT(31)) {
		tFlag |= Flag;
	} else {
		if (i)
			return pExeIFun->Undef(Inst, 0, pArg);
		tFlag |= DPO_MADD|RMS_4B;
	}
	return pExeIFun->SetReg(tFlag|(INST_BIT(31) ? RDS_8B : RDS_4B)|(INST_BIT(15) ? DPF_NEG : 0)
	  , A64_REG(0), A64_REG(5), A64_REG(16), A64_REG(10), pArg);
}

/*************************************************************************
* DESCRIPTION
*	A64指令解码
*
* PARAMETERS
*	Inst: 指令
*	pInstSz: 返回指令长度
*	pExeIFun: 回调函数结构体
*	pArg: 参数
*
* RETURNS
*	回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ArmInstDec(TUINT Inst, int *pInstSz, DC_INSTFUN *pExeIFun, void *pArg)
{
	static INST_DISPATCH A64OpDPI[] =
	{
		{0x1D000000, 0x10000000, PSR_AL|REG_DEFB|RMS_4B, A64DataProcPC}, /* PC-rel. addressing */
		{0x1D000000, 0x11000000, PSR_AL, A64DataProcADI}, /* Add/subtract (immediate) */
	};
	static INST_DISPATCH A64OpDPI2[] =
	{
		{0x1D800000, 0x10000000, PSR_AL, A64DataProcLI}, /* Logical (immediate) */
		{0x1D800000, 0x10800000, PSR_AL, A64DataProcMW}, /* Move wide (immediate) */
		{0x1D800000, 0x11000000, PSR_AL, A64DataProcBF}, /* Bitfield */
		{0x1D800000, 0x11800000, PSR_AL, A64DataProcEX}, /* Extract */
	};
	static INST_DISPATCH A64OpBL[] =
	{
		{0x7C000000, 0x14000000, PSR_AL|REG_DEFB|RMS_4B|DPO_ADD, A64BranchUI}, /* Unconditional branch (immediate) */
		{0x7C000000, 0x34000000, DPO_BCPR, A64BranchCI}, /* Compare & branch (immediate) */
		{0xFC000000, 0x54000000, REG_DEFB|RMS_4B|DPO_ADD, A64BranchCI2}, /* Conditional branch (immediate) */
		{0xFD000000, 0xD4000000, PSR_AL, A64BranchEG}, /* Exception generation */
		{0xFDC00000, 0xD5000000, PSR_AL, A64BranchSYS}, /* System */
	};
	static INST_DISPATCH A64OpBL2[] =
	{
		{0x7C000000, 0x14000000, PSR_AL|REG_DEFB|RMS_4B|DPO_ADD, A64BranchUI}, /* Unconditional branch (immediate) */
		{0x7C000000, 0x34000000, DPO_BTST, A64BranchTI}, /* Test & branch (immediate) */
		{0xFC000000, 0xD4000000, PSR_AL|REG_DEFB|RMS_4B, A64BranchUR}, /* Unconditional branch (register) */
	};
	static INST_DISPATCH A64OpLS[] =
	{
		{0x2F000000, 0x08000000, PSR_AL|RMS_4B, A64LoadStoreEX}, /* Load/store exclusive */
		{0x2B800000, 0x28000000, PSR_AL|RMS_8B|LST_NT, A64LoadStoreNT}, /* Load/store no-allocate pair (offset) */
		{0x2B800000, 0x28800000, PSR_AL|RMS_8B|LSF_POST, A64LoadStorePR}, /* Load/store register pair (post-indexed) */
		{0x2B800000, 0x29000000, PSR_AL|RMS_8B, A64LoadStorePR}, /* Load/store register pair (offset) */
		{0x2B800000, 0x29800000, PSR_AL|RMS_8B|LSF_WB, A64LoadStorePR}, /* Load/store register pair (pre-indexed) */
		{0xAFBF0000, 0x0C000000, PSR_AL, InstUnImplement}, /* Advanced SIMD load/store multiple structures */
		{0xAFA00000, 0x0C800000, PSR_AL, InstUnImplement}, /* Advanced SIMD load/store multiple structures (post-indexed) */
		{0xAF9F0000, 0x0D000000, PSR_AL, InstUnImplement}, /* Advanced SIMD load/store single structure */
		{0xAF800000, 0x0D800000, PSR_AL, InstUnImplement}, /* Advanced SIMD load/store single structure (post-indexed) */
	};
	static INST_DISPATCH A64OpLS2[] =
	{
		{0x2B000000, 0x08000000, PSR_AL|RMS_8B|LSF_LD, A64LoadRegPC}, /* Load register (literal) */
		{0x2B200C00, 0x28000000, PSR_AL|RMS_8B|LST_UI, A64LoadStoreIM}, /* Load/store register (unscaled immediate) */
		{0x2B200C00, 0x28000400, PSR_AL|RMS_8B|LSF_POST, A64LoadStoreIM}, /* Load/store register (immediate post-indexed) */
		{0x2B200C00, 0x28000800, PSR_AL|RMS_8B|LST_UR, A64LoadStoreIM}, /* Load/store register (unprivileged) */
		{0x2B200C00, 0x28000C00, PSR_AL|RMS_8B|LSF_WB, A64LoadStoreIM}, /* Load/store register (immediate pre-indexed) */
		{0x2B200C00, 0x28200800, PSR_AL, A64LoadStoreRO}, /* Load/store register (register offset) */
		{0x2B000000, 0x29000000, PSR_AL|RMS_8B, A64LoadStoreUI}, /* Load/store register (unsigned immediate) */
	};
	static INST_DISPATCH A64OpDPR[] =
	{
		{0x0F000000, 0x0A000000, PSR_AL|DPF_RM, A64DataProcLR}, /* Logical (shifted register) */
		{0x0F200000, 0x0B000000, PSR_AL|DPF_RM, A64DataProcADS}, /* Add/subtract (shifted register) */
		{0x0F200000, 0x0B200000, PSR_AL|DPF_RM, A64DataProcADE}, /* Add/subtract (extended register) */
	};
	static INST_DISPATCH A64OpDPR2[] =
	{
		{0x0FE00000, 0x0A000000, PSR_AL|DPF_CRY|DPF_RM, A64DataProcADC}, /* Add/subtract (with carry) */
		{0x0FE00800, 0x0A400000, DPO_CCMN, A64DataProcCC}, /* Conditional compare (register) */
		{0x0FE00800, 0x0A400800, DPO_CCMN, A64DataProcCC}, /* Conditional compare (immediate) */
		{0x0FE00000, 0x0A800000, DPO_CSEL|DPF_RM, A64DataProcCS}, /* Conditional select */
		{0x0F000000, 0x0B000000, PSR_AL|DPF_RM|DPF_RS, A64DataProcSrc3}, /* Data-processing (3 source) */
		{0x4FE00000, 0x0AC00000, PSR_AL|DPF_RM, A64DataProcSrc2}, /* Data-processing (2 source) */
		{0x4FE00000, 0x4AC00000, PSR_AL, A64DataProcSrc1}, /* Data-processing (1 source) */
	};
	static const struct
	{
		INST_DISPATCH *pDisp;
		unsigned Cnt;
	} A64OpCodes[] =
	{
		{NULL, 0}, /* UNALLOCATED */
		{NULL, 0},
		{NULL, 0},
		{NULL, 0},
		{A64OpLS, ARY_CNT(A64OpLS)},
		{A64OpDPR, ARY_CNT(A64OpDPR)},
		{A64OpLS, ARY_CNT(A64OpLS)},
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)}, /* Data processing - SIMD and floating point */
		{A64OpDPI, ARY_CNT(A64OpDPI)},
		{A64OpDPI2, ARY_CNT(A64OpDPI2)},
		{A64OpBL, ARY_CNT(A64OpBL)},
		{A64OpBL2, ARY_CNT(A64OpBL2)},
		{A64OpLS2, ARY_CNT(A64OpLS2)},
		{A64OpDPR2, ARY_CNT(A64OpDPR2)},
		{A64OpLS2, ARY_CNT(A64OpLS2)},
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)}, /* Data processing - SIMD and floating point */
	};
	unsigned i = INST_VAL(25, 4), Cnt;
	INST_DISPATCH *pDisp;

	for (*pInstSz = 4, pDisp = A64OpCodes[i].pDisp, Cnt = A64OpCodes[i].Cnt, i = 0; i < Cnt; i++) {
		if ((Inst&pDisp[i].Mask) == pDisp[i].Val)
			return pDisp[i].InstDec(Inst, pDisp[i].tFlag, pExeIFun, pArg);
	}
	return pExeIFun->Undef(Inst, 0, pArg);
}
#else

#define A32_REG(sbit)   INST_VAL(sbit, 4)
#define A32_XREG(sbit)  A32RegMap[A32_REG(sbit)]

static TINT A32RegMap[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,REG_ZR};
static const TUINT A32DpR1[] =
{
	DPO_AND, DPO_EOR, DPO_ADD|DPF_NEG, DPO_RSB, DPO_ADD, DPO_ADD|DPF_CRY, DPO_ADD|DPF_NEG|DPF_CRY, DPO_RSB|DPF_CRY,
	DPO_AND, DPO_EOR, DPO_ADD|DPF_NEG, DPO_ADD, DPO_ORR, DPO_ADD, DPO_AND|DPF_INV, DPO_ORR|DPF_INV,
};
static const TUINT A32LsBk[4] = {0, LSF_BLK|LST_IA, LSF_BLK|LSF_BKD|LSF_BKB|LST_DB};
static const TINT RdRevR1[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0};
static const TINT RbRevR1[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1};

/*************************************************************************
* DESCRIPTION
*	计算带有偏移的立即数
*
* PARAMETERS
*	pFlag: 返回标记
*	Imm5: 立即数
*	Op: 偏移类型
*
* RETURNS
*	立即数
*
* LOCAL AFFECTED
*
*************************************************************************/
static TUINT CalcImmShift(TUINT *pFlag, TUINT Imm5, TUINT Op)
{
	switch (Op) {
	case 0: *pFlag |= RMO_LSL; break;
	case 1: *pFlag |= RMO_LSR; if (!Imm5) Imm5 = 32; break;
	case 2: *pFlag |= RMO_ASR; if (!Imm5) Imm5 = 32; break;
	default: *pFlag |= (Imm5 ? RMO_ROR : RMO_RRX);
	}
	return Imm5;
}

/*************************************************************************
* DESCRIPTION
*	解码Data-processing (register)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32DataProcR1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Val = CalcImmShift(&tFlag, INST_VAL(7, 5), INST_VAL(5, 2)), i = INST_VAL(21, 4);

	return pExeIFun->SetReg(tFlag|A32DpR1[i]|INST_FLAG(20, DPF_PSR)
	  , RdRevR1[i] ? REG_ZR : A32_REG(12), RbRevR1[i] ? REG_ZR : A32_REG(16), A32_REG(0), Val, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Data-processing (register)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32DataProcR2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT i = INST_VAL(21, 4);

	return pExeIFun->SetReg(tFlag|A32DpR1[i]|INST_FLAG(20, DPF_PSR)|RmFMap2[INST_VAL(5, 2)]
	  , RdRevR1[i] ? REG_ZR : A32_REG(12), RbRevR1[i] ? REG_ZR : A32_REG(16), A32_REG(0), A32_REG(8), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Data-processing (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32DataProcI1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Val = INST_VAL(0, 8), Shift = INST_VAL(8, 4) << 1, i = INST_VAL(21, 4);

	return pExeIFun->SetReg(tFlag|A32DpR1[i]|INST_FLAG(20, DPF_PSR)
	  , RdRevR1[i] ? REG_ZR : A32_REG(12), RbRevR1[i] ? REG_ZR : A32_REG(16), (Val >> Shift)|(Val << (32 - Shift)), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码MOV, MOVS, MOVK (immediate)指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32DataProcI2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Val = INST_VAL(0, 12)|(INST_VAL(16, 4) << 12);

	if (INST_BIT(22))
		return pExeIFun->SetReg(tFlag|DPO_MVK, A32_REG(12), A32_REG(12), Val, 16, pArg);
	return pExeIFun->SetReg(tFlag|DPO_ADD, A32_REG(12), REG_ZR, Val, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码杂项指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32DataProcM1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A32DpM1[4] = {MIT_HLT, MIT_BRK, MIT_HVC, MIT_SMC};
	
	if ((Inst&0x00600070) == 0x00200010) { /* BX on page F7-2701 */
		return pExeIFun->SetReg(tFlag|DPO_ADD, REG_PC, A32_REG(0), 0, 0, pArg);
	} else if ((Inst&0x00600070) == 0x00600010) { /* CLZ on page F7-2708 */
		return pExeIFun->SetReg(tFlag|DPO_CLZ, A32_REG(12), A32_REG(0), 0, 0, pArg);
	} else if ((Inst&0x00600070) == 0x00200030) { /* BLX (register) on page F7-2699 */
		return pExeIFun->SetReg(tFlag|DPO_ADD|DPF_LINK, REG_PC, A32_REG(0), 0, 0, pArg);
	} else if ((Inst&0x00600070) == 0x00600060) { /* ERET on page F7-2743 */
		return pExeIFun->Misc(tFlag|MIT_ERET, 0, pArg);
	} else if ((Inst&0x00000070) == 0x00000070) {
		return pExeIFun->Misc(tFlag|A32DpM1[INST_VAL(21, 2)], INST_VAL(0, 4)|(INST_VAL(8, 12) << 4), pArg);
	}
	return pExeIFun->Undef(Inst, 1, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码杂项指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32Misc2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A32Mi2[6] = {MIT_NOP, MIT_YIELD, MIT_WFE, MIT_WFI, MIT_SEV, MIT_SEVL};
	const TUINT i = INST_VAL(0, 8);

	if (Inst&0x004F0000)
		return pExeIFun->Undef(Inst, 1, pArg);
	if (i < ARY_CNT(A32Mi2))
		return pExeIFun->Misc(tFlag|A32Mi2[i], 0, pArg);
	return pExeIFun->Undef(Inst, (i >> 4) == 15, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Branch, branch with link指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32BranchIM(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	if (INST_VAL(28, 4) == (PSR_NV >> PSR_S))
		return pExeIFun->SetReg(tFlag|DPF_LINK|DPF_XCHANGE, REG_PC, REG_PC, (INST_SVAL(0, 24) << 2)|(INST_VAL(24, 1) << 1), 0, pArg);
	return pExeIFun->SetReg(tFlag|INST_FLAG(24, DPF_LINK), REG_PC, REG_PC, INST_SVAL(0, 24) << 2, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Synchronization primitives指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32LoadStoreEX(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A32LsExSz[4] = {LSS_UW, LSS_UW, LSS_UB, LSS_UH};
	static const TUINT A32LsExOp[4] = {LST_AR, 0, LSF_EX|LST_AX, LSF_EX|LST_EX};
	const TUINT i = INST_VAL(21, 2);

	if (!INST_BIT(23))
		return pExeIFun->Undef(Inst, 0, pArg);
	tFlag |= A32LsExOp[INST_VAL(8, 2)]|A32LsExSz[i];
	if (INST_BIT(20))
		return pExeIFun->LoadStore(tFlag|LSF_LD, A32_REG(12), i == 1 ? A32_REG(12) + 1 : REG_ZR, A32_REG(16), REG_ZR, 0, pArg);
	return pExeIFun->LoadStore(tFlag, A32_REG(0), i == 1 ? A32_REG(0) + 1 : REG_ZR
	  , A32_REG(16), INST_BIT(9) ? A32_REG(12) : REG_ZR, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Extra load/store指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32LoadStoreM1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A32LsM1Sz[4][2] = {{0, 0}, {LSS_UH, LSS_UH|LSF_LD}, {LSS_UW|LSF_LD, LSS_SB|LSF_LD}, {LSS_UW, LSS_SH|LSF_LD}};
	TUINT Flag = tFlag|A32LsM1Sz[INST_VAL(5, 2)][INST_VAL(20, 1)]|INST_NFLAG(24, LSF_POST);
	const TINT Rd = A32_REG(12);
	TINT Val;

	if (LST_VAL != LST_UR)
		Flag |= INST_FLAG(21, LSF_WB);
	if (INST_BIT(22)) { /* immed */
		Val = INST_VAL(0, 4)|(INST_VAL(8, 4) << 4);
		return pExeIFun->LoadStore(Flag, Rd, LSS_SZ == 2 ? Rd + 1 : REG_ZR, A32_REG(16), REG_ZR, INST_BIT(23) ? Val : -Val, pArg);
	}
	return pExeIFun->LoadStore(Flag|INST_NFLAG(23, LSF_NEG), Rd, LSS_SZ == 2 ? Rd + 1 : REG_ZR
	  , A32_REG(16), A32_REG(0), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/store word and unsigned byte指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32LoadStoreRI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A32LsRiSz[2] = {LSS_UW, LSS_UB};
	TINT Val;

	if (FLAG_VAL(tFlag, LST_S, LST_B) != LST_UR)
		tFlag |= INST_FLAG(21, LSF_WB);
	tFlag |= A32LsRiSz[INST_VAL(22, 1)]|INST_FLAG(20, LSF_LD)|INST_NFLAG(24, LSF_POST);
	if (INST_BIT(25)) { /* register */
		Val = CalcImmShift(&tFlag, INST_VAL(7, 5), INST_VAL(5, 2));
		return pExeIFun->LoadStore(tFlag|INST_NFLAG(23, LSF_NEG), A32_REG(12), REG_ZR, A32_REG(16), A32_REG(0), Val, pArg);
	}
	Val = INST_VAL(0, 12);
	return pExeIFun->LoadStore(tFlag, A32_REG(12), REG_ZR, A32_REG(16), REG_ZR, INST_BIT(23) ? Val : -Val, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码多媒体指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32MediaI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	(void)tFlag;
	return pExeIFun->Undef(Inst, !(Inst == 0xE7F001F2), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码block data transfer指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32LoadStoreBK(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT A32LsBk2[8] =
	{
		LSF_BKD|LST_DA, LST_IA|LSF_USR, LST_IA, LST_IA|LSF_USR, LSF_BKD|LSF_BKB|LST_DB, LST_IA|LSF_USR, LSF_BKB|LST_IB, LST_IA|LSF_USR
	};
	
	return pExeIFun->LoadStore(tFlag|A32LsBk2[INST_VAL(22, 3)]|INST_FLAG(20, LSF_LD)|INST_FLAG(21, LSF_WB)
	  , INST_VAL(0, 16), REG_ZR, A32_REG(16), REG_ZR, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码SIMD block data transfer指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32LoadStoreVBK(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TUINT Vd = A32_REG(12);
	const TINT Imm8 = INST_VAL(0, 8);
	TSIZE RegList;

	if (INST_BIT(8)) {
		tFlag |= RDS_8B|LSS_UX;
		Vd |= (INST_VAL(22, 1) << 4);
	} else {
		tFlag |= RDS_4B|LSS_UW;
		Vd = (Vd << 1)|INST_VAL(22, 1);
	}
	if ((Inst&0x01200000) == 0x01000000) /* LDR/STR */
		return pExeIFun->LoadStore(tFlag|INST_FLAG(20, LSF_LD), Vd, REG_ZR, A32_REG(16), REG_ZR
		  , INST_BIT(23) ? -(Imm8 << 2) : Imm8 << 2, pArg);
	RegList = (((TSIZE)1 << (Imm8 >> INST_VAL(8, 1))) - 1) << Vd;
	return pExeIFun->LoadStore(tFlag|A32LsBk[INST_VAL(23, 2)]|INST_FLAG(20, LSF_LD)|INST_FLAG(21, LSF_WB)
	  , RegList, REG_ZR, A32_REG(16), REG_ZR, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码协处理器指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32CoProcM2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	if (INST_BIT(24))
		return pExeIFun->Misc(tFlag|MIT_SVC, INST_VAL(0, 24), pArg);
	return pExeIFun->Undef(Inst, 1, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码杂项指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int A32ChangePE(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	if ((Inst&0x01FFFE20) == 0x01080000)
		return pExeIFun->Misc(tFlag|MIT_MSR3, INST_VAL(6, 4), pArg);
	else if ((Inst&0x01FFFE20) == 0x010C0000)
		return pExeIFun->Misc(tFlag|MIT_MSR2, INST_VAL(6, 4), pArg);
	return pExeIFun->Undef(Inst, 1, pArg);
}

/*************************************************************************
* DESCRIPTION
*	A32指令解码
*
* PARAMETERS
*	Inst: 指令
*	pInstSz: 返回指令长度
*	pExeIFun: 回调函数结构体
*	pArg: 参数
*
* RETURNS
*	回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ArmInstDec(TUINT Inst, int *pInstSz, DC_INSTFUN *pExeIFun, void *pArg)
{
	static INST_DISPATCH A32OpDPR[] =
	{
		{0x01000010, 0x00000000, RDM_4B|DPF_RM, A32DataProcR1}, /* Data-processing (register) */
		{0x01900010, 0x01100000, RDM_4B|DPF_RM|DPF_PSR, A32DataProcR1}, /* Data-processing (register) */
		{0x01800010, 0x01800000, RDM_4B|DPF_RM, A32DataProcR1}, /* Data-processing (register) */
		{0x01000090, 0x00000010, RDM_4B|DPF_RM|DPF_RS, A32DataProcR2}, /* Data-processing (register) */
		{0x01900090, 0x01100010, RDM_4B|DPF_RM|DPF_RS, A32DataProcR2}, /* Data-processing (register) */
		{0x01800090, 0x01800010, RDM_4B|DPF_RM|DPF_RS, A32DataProcR2}, /* Data-processing (register) */
		{0x01900080, 0x01000000, RDM_4B, A32DataProcM1}, /* Miscellaneous instructions on page F4-2576 */
		{0x01900090, 0x01000080, RDM_4B, InstUnImplement}, /* Halfword multiply and multiply accumulate on page F4-2572 */
		{0x010000F0, 0x00000090, RDM_4B, InstUnImplement}, /* Multiply and multiply accumulate on page F4-2571 */
		{0x010000F0, 0x01000090, RDM_4B, A32LoadStoreEX}, /* Synchronization primitives on page F4-2574 */
		{0x012000F0, 0x000000B0, RDM_4B, A32LoadStoreM1}, /* Extra load/store instructions on page F4-2572 */
		{0x012000D0, 0x000000D0, RDM_4B, A32LoadStoreM1},
		{0x010000F0, 0x010000B0, RDM_4B, A32LoadStoreM1},
		{0x010000D0, 0x010000D0, RDM_4B, A32LoadStoreM1},
		{0x013000D0, 0x002000D0, RDM_4B, A32LoadStoreM1},
		{0x012000F0, 0x002000B0, RDM_4B|LST_UR, A32LoadStoreM1}, /* Extra load/store instructions, unprivileged on page F4-2573 */
		{0x013000D0, 0x003000D0, RDM_4B|LST_UR, A32LoadStoreM1},
	};
	static INST_DISPATCH A32OpDPI[] =
	{
		{0x01000000, 0x00000000, RDM_4B, A32DataProcI1}, /* Data-processing (immediate) */
		{0x01900000, 0x01100000, RDM_4B|DPF_PSR, A32DataProcI1},
		{0x01800000, 0x01800000, RDM_4B, A32DataProcI1},
		{0x01B00000, 0x01000000, RDM_4B, A32DataProcI2}, /* MOV, MOVK (immediate) */
		{0x01B00000, 0x01200000, RDM_4B, A32Misc2}, /* MSR (immediate), and hints on page F4-2575 */
	};
	static INST_DISPATCH A32OpBL[] =
	{
		{0x00000000, 0x00000000, RDM_4B|DPO_ADD, A32BranchIM},
	};
	static INST_DISPATCH A32OpLSI[] =
	{
		{0x01200000, 0x00000000, RDM_4B, A32LoadStoreRI},
		{0x01000000, 0x01000000, RDM_4B, A32LoadStoreRI},
		{0x01200000, 0x00200000, RDM_4B|LST_UR, A32LoadStoreRI},
	};
	static INST_DISPATCH A32OpLSR[] =
	{
		{0x01200010, 0x00000000, RDM_4B, A32LoadStoreRI},
		{0x01000010, 0x01000000, RDM_4B, A32LoadStoreRI},
		{0x01200010, 0x00200000, RDM_4B|LST_UR, A32LoadStoreRI},
		{0x00000010, 0x00000010, RDM_4B, A32MediaI}, /* Media instructions on page F4-2578 */
	};
	static INST_DISPATCH A32OpLSB[] =
	{
		{0x00000000, 0x00000000, RDM_4B|LSS_UW|LSF_BLK, A32LoadStoreBK},
	};
	static INST_DISPATCH A32OpCP1[] =
	{
		{0x01A00E00, 0x01200A00, RMS_4B|RDS_VFP, A32LoadStoreVBK}, /* VLDM/VSTM */
		{0x01800E00, 0x00800A00, RMS_4B|RDS_VFP, A32LoadStoreVBK}, /* VLDM/VSTM */
		{0x01200E00, 0x01000A00, RMS_4B|RDS_VFP, A32LoadStoreVBK}, /* VSTR/VLDR */
		{0x01E00E00, 0x00400A00, RDM_4B, InstUnImplement}, /* 64-bit transfers accessing the SIMD and floating-point register file */
		{0x00000800, 0x00000000, RDM_4B, InstUnImplement}, /* Coprocessor */
		{0x00000E00, 0x00000800, RDM_4B, InstUnImplement}, /* Coprocessor */
		{0x00000C00, 0x00000C00, RDM_4B, InstUnImplement}, /* Coprocessor */
	};
	static INST_DISPATCH A32OpCP2[] =
	{
		{0x00000000, 0x00000000, RDM_4B, A32CoProcM2},
	};
	static INST_DISPATCH A32OpCPE[] =
	{
		{0x01F00000, 0x01000000, RDM_4B, A32ChangePE},
		{0x01F00000, 0x01200000, RDM_4B, A32ChangePE}
	};
	static const struct
	{
		INST_DISPATCH *pDisp;
		unsigned Cnt;
	} A32OpCodes[] =
	{
		{A32OpDPR, ARY_CNT(A32OpDPR)}, /* Data processing - register */
		{A32OpDPI, ARY_CNT(A32OpDPI)}, /* Data processing - immediate */
		{A32OpLSI, ARY_CNT(A32OpLSI)}, /* load and stroe - immediate */
		{A32OpLSR, ARY_CNT(A32OpLSR)}, /* load and stroe - register */
		{A32OpLSB, ARY_CNT(A32OpLSB)}, /* load and stroe - block */
		{A32OpBL, ARY_CNT(A32OpBL)}, /* Branch, exception generation and system instructions */
		{A32OpCP1, ARY_CNT(A32OpCP1)},
		{A32OpCP2, ARY_CNT(A32OpCP2)},
		{A32OpCPE, ARY_CNT(A32OpCPE)}, /* Change PE State */
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)}, /* Advanced SIMD data-processing instructions on page F5-2597 */
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)}, /* Advanced SIMD element or structure load/store instructions on page F5-2613 */
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)}, /* Preload Instruction */
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)}, /* Exception Return */
		{A32OpBL, ARY_CNT(A32OpBL)}, /* Branch with Link and Exchange */
		{NULL, 0},
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)}, /* Coprocessor operations */
	};
	const TUINT Cond = INST_VAL(28, 4);
	unsigned i = INST_VAL(25, 3)|((Cond == 15) << 3), Cnt;
	INST_DISPATCH *pDisp;

	for (*pInstSz = 4, pDisp = A32OpCodes[i].pDisp, Cnt = A32OpCodes[i].Cnt, i = 0; i < Cnt; i++) {
		if ((Inst&pDisp[i].Mask) == pDisp[i].Val)
			return pDisp[i].InstDec(Inst, pDisp[i].tFlag|(Cond << PSR_S), pExeIFun, pArg);
	}
	return pExeIFun->Undef(Inst, 0, pArg);
}

#ifdef SUPPORT_THUMB

#define T16_REG(sbit)  INST_VAL(sbit, 3)

/*************************************************************************
* DESCRIPTION
*	解码block data transfer指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32LoadStoreBK(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->LoadStore(tFlag|A32LsBk[INST_VAL(7, 2)]|INST_FLAG(4, LSF_LD)|INST_FLAG(5, LSF_WB)
	  , INST_VAL(16, 16), REG_ZR, A32_REG(0), REG_ZR, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码SIMD block data transfer指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32LoadStoreVBK(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return A32LoadStoreVBK((Inst >> 16)|(Inst << 16), tFlag, pExeIFun, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码load/store immediate 8指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32LoadStoreI8(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TINT Val = INST_VAL(16, 8) << 2;

	if (Inst&0x1A0)
		return pExeIFun->LoadStore(tFlag|INST_FLAG(4, LSF_LD)|INST_FLAG(5, LSF_WB)|INST_NFLAG(8, LSF_POST)
		  , A32_REG(28), A32_REG(24), A32_REG(0), REG_ZR, INST_BIT(7) ? Val : -Val, pArg);
	return pExeIFun->LoadStore(tFlag|INST_FLAG(4, LSF_LD), A32_REG(28), REG_ZR, A32_REG(0), INST_BIT(4) ? REG_ZR : A32_REG(24), Val, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Synchronization primitives指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32LoadStoreEX(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T32LsEx[16] =
	{
		LST_TB|LSS_UB, LST_TB|LSS_UH|RMO_LSL, 0, 0, LSF_EX|LST_EX|LSS_UB, LSF_EX|LST_EX|LSS_UH, 0, LSF_EX|LST_EX|LSS_UW,
		LST_AR|LSS_UB, LST_AR|LSS_UH, LST_AR|LSS_UW, 0, LSF_EX|LST_AX|LSS_UB, LSF_EX|LST_AX|LSS_UH, LSF_EX|LST_AX|LSS_UW, LSF_EX|LST_AX|LSS_UW
	};
	const TUINT i = INST_VAL(20, 4), Flag = T32LsEx[i];

	if (!Flag || (i <= 1 && !INST_BIT(4)))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(tFlag|Flag|INST_FLAG(4, LSF_LD), A32_REG(28), i <= 1 ? REG_ZR : A32_XREG(24)
	  , A32_REG(0), A32_XREG(16), i == 1, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Store single data指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32LoadStoreSD(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T32LsSdSz[4] = {LSS_UB, LSS_UH, LSS_UW};
	TINT Val, Rd = A32_REG(28), Flag = T32LsSdSz[INST_VAL(5, 2)];
	const TINT Rb = A32_REG(0);

	if (!Flag || (!INST_BIT(4) && Rb == REG_PC))
		return pExeIFun->Undef(Inst, 0, pArg);
	tFlag = Flag = Flag|tFlag|INST_FLAG(4, LSF_LD)|INST_FLAG(8, LSF_SIG);
	if (Rd == REG_PC && INST_BIT(4)) {
		Flag = (Flag&~(LSF_SIG|(((1 << LSS_B) - 1) << LSS_S)))|LST_PM|LSS_UW;
		Rd = INST_FLAG(8, 1 << 3/* exec */)|INST_FLAG(5, 1 << 4/* write */);
	}
	if (INST_BIT(7) || Rb == REG_PC) { /* immed12 */
		Val = INST_VAL(16, 12);
		return pExeIFun->LoadStore(Flag, Rd, REG_ZR, Rb, REG_ZR, INST_BIT(7) ? Val : -Val, pArg);
	}
	if (INST_BIT(27)) { /* immed8 */
		if (!(Inst&0x05000000)) /* bit24/bit26 */
			return pExeIFun->Undef(Inst, 0, pArg);
		Val = INST_VAL(16, 8);
		if (LST_VAL == LST_PM) {
			if (INST_VAL(24, 3) == 4)
				return pExeIFun->LoadStore(Flag, Rd, REG_ZR, Rb, REG_ZR, -Val, pArg);
			Rd = REG_PC;
			Flag = tFlag;
		}
		return pExeIFun->LoadStore(Flag|INST_NFLAG(26, LSF_POST)|INST_FLAG(24, LSF_WB)|(INST_VAL(24, 3) == 6 ? LST_UR : 0)
		  , Rd, REG_ZR, Rb, REG_ZR, INST_BIT(25) ? Val : -Val, pArg);
	}
	if (Inst&(0x3F << 22))
		return pExeIFun->Undef(Inst, 0, pArg);
	return pExeIFun->LoadStore(Flag|RMO_LSL, Rd, REG_ZR, Rb, A32_REG(16), INST_VAL(20, 2), pArg);
}

/*************************************************************************
* DESCRIPTION
*	计算带T32的立即数
*
* PARAMETERS
*	Type: 类型
*	Imm8: 立即数
*
* RETURNS
*	立即数
*
* LOCAL AFFECTED
*
*************************************************************************/
static TUINT T32ExpandImm(TUINT Type, TUINT Imm8)
{
	if (Type&0xC) {
		Type = (Type << 1)|((Imm8 >> 7)&1);
		Imm8 |= 0x80;
		return (Imm8 >> Type)|(Imm8 << (32 - Type));
	}
	switch (Type) {
	case 0: return Imm8;
	case 1: return Imm8|(Imm8 << 16);
	case 2: return (Imm8 << 8)|(Imm8 << 24);
	default: return Imm8|(Imm8 << 8)|(Imm8 << 16)|(Imm8 << 24);
	}
}

/*************************************************************************
* DESCRIPTION
*	解码算术指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32DataProcLR(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T32DpLr[16] =
	{
		DPO_AND, DPO_AND|DPF_INV, DPO_ORR, DPO_ORR|DPF_INV, DPO_EOR, 0, 0/* PKHBT */, 0, 
		DPO_ADD, 0, DPO_ADD|DPF_CRY, DPO_ADD|DPF_NEG|DPF_CRY, 0, DPO_ADD|DPF_NEG, DPO_RSB
	};
	static const char T32ZerosLr[] = {1, 0, 2, 2, 1, 0, 4/* PKHBT */, 0, 1, 0, 0, 0, 0, 1, 0, 0};
	const TUINT i = INST_VAL(5, 4), Flag = T32DpLr[i];
	TINT Val, Rd = A32_REG(24), Rb = A32_REG(0);

	if (!Flag)
		return pExeIFun->Undef(Inst, !INST_BIT(12) && (T32ZerosLr[i]&4), pArg);
	if (Rd == REG_PC && INST_BIT(4) && (T32ZerosLr[i]&1))
		Rd = REG_ZR;
	if (Rb == REG_PC && (T32ZerosLr[i]&2))
		Rb = REG_ZR;
	tFlag |= (Flag|INST_FLAG(4, DPF_PSR));
	if (INST_BIT(12))
		return pExeIFun->SetReg(tFlag, Rd, Rb, T32ExpandImm(INST_VAL(28, 3)|(INST_VAL(10, 1) << 3), INST_VAL(16, 8)), 0, pArg);
	Val = CalcImmShift(&tFlag, INST_VAL(22, 2)|(INST_VAL(28, 3) << 3), INST_VAL(20, 2));
	return pExeIFun->SetReg(tFlag, Rd, Rb, A32_REG(16), Val, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码算术指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32DataProcLI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Val = INST_VAL(16, 8)|(INST_VAL(28, 3) << 8)|(INST_VAL(10, 1) << 11), Rd = A32_REG(24);
	
	switch (INST_VAL(4, 5)) {
	case 0x00: return pExeIFun->SetReg(tFlag|DPO_ADD, Rd, A32_REG(0), Val, 0, pArg);
	case 0x04: return pExeIFun->SetReg(tFlag|DPO_ADD, Rd, REG_ZR, Val|(INST_VAL(0, 4) << 12), 0, pArg);
	case 0x0A: return pExeIFun->SetReg(tFlag|DPO_ADD|DPF_NEG, Rd, A32_REG(0), Val, 0, pArg);
	case 0x0C: return pExeIFun->SetReg(tFlag|DPO_MVK, Rd, Rd, Val|(INST_VAL(0, 4) << 12), 16, pArg);
	case 0x10: case 0x12: case 0x14: case 0x16: case 0x18: case 0x1A: case 0x1C: return pExeIFun->Undef(Inst, 1, pArg);
	default: return pExeIFun->Undef(Inst, 0, pArg);
	}
}

/*************************************************************************
* DESCRIPTION
*	解码MOV指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32DataProcMV(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->SetReg(tFlag|RmFMap2[INST_VAL(5, 2)]|INST_FLAG(4, DPF_PSR), A32_REG(24), REG_ZR, A32_REG(0), A32_REG(16), pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码算术指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32DataProcLR2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static TUINT T32DpLr2[8] = {DPO_SXTH, DPO_UXTH, 0, 0, DPO_SXTB, DPO_UXTB};
	const TUINT Flag = T32DpLr2[INST_VAL(4, 3)];
	
	if (!Flag || INST_BIT(7) || INST_VAL(0, 4) != 15)
		return pExeIFun->Undef(Inst, !INST_BIT(7), pArg);
	return pExeIFun->SetReg(tFlag|Flag, A32_REG(24), REG_ZR, A32_REG(16), INST_VAL(20, 2) << 3, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码杂项指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32Misc1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T32Mi1[4] = {DPO_REV, DPO_REV16, DPO_RBIT, 0/* REVSH */};
	TUINT Flag;
	
	switch (INST_VAL(4, 3)) {
	case 1:
		Flag = T32Mi1[INST_VAL(20, 2)];
		if (!Flag)
			return pExeIFun->Undef(Inst, 1, pArg);
		return pExeIFun->SetReg(tFlag|Flag, A32_REG(24), A32_REG(0), 0, 0, pArg);
	case 3: return pExeIFun->SetReg(tFlag|DPO_CLZ, A32_REG(24), A32_REG(0), 0, 0, pArg);
	case 6: case 7: return pExeIFun->Undef(Inst, 0, pArg);
	default: return pExeIFun->Undef(Inst, 1, pArg);
	}
}

/*************************************************************************
* DESCRIPTION
*	解码杂项指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32Misc2(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	(void)tFlag;
	return pExeIFun->Undef(Inst, 1, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码跳转指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T32Branch(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TSIZE Val = INST_VAL(16, 11) << 1, SBit = INST_VAL(10, 1), I = INST_VAL(29, 1), J = INST_VAL(27, 1);
	
	if (Inst&0x50000000) {
		if ((Inst&0x10010000) == 0x00010000)
			return pExeIFun->Undef(Inst, 0, pArg);
		I = (I^SBit)^1;
		J = (J^SBit)^1;
		Val |= ((SBit << 24)|(I << 23)|(J << 22)|(INST_VAL(0, 10) << 12));
		return pExeIFun->SetReg(tFlag|INST_FLAG(30, DPF_LINK)|INST_NFLAG(28, DPF_XCHANGE), REG_PC, REG_PC, SEXT_VAL(Val, 25), 0, pArg);
	}
	Val |= ((SBit << 20)|(J << 19)|(I << 18)|(INST_VAL(0, 6) << 12));
	return pExeIFun->SetReg(tFlag|(INST_VAL(6, 4) << PSR_B), REG_PC, REG_PC, SEXT_VAL(Val, 21), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码MOV指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcMV(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Val = CalcImmShift(&tFlag, INST_VAL(6, 5), INST_VAL(11, 2));
	
	return pExeIFun->SetReg(tFlag, T16_REG(0), REG_ZR, T16_REG(3), Val, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码ADD/SUB指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcAS(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT DpFlagAs = (INST_BIT(9) ? DPF_NEG : 0);

	if (INST_BIT(10)) /* immediate */
		return pExeIFun->SetReg(tFlag|DpFlagAs, T16_REG(0), T16_REG(3), INST_VAL(6, 3), 0, pArg);
	return pExeIFun->SetReg(tFlag|DpFlagAs|DPF_RM, T16_REG(0), T16_REG(3), T16_REG(6), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码ADD/SUB immediate 7指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcI7(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->SetReg(tFlag|(INST_BIT(7) ? DPF_NEG : 0), REG_SP, REG_SP, INST_VAL(0, 7) << 2, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码ADD/SUB immediate 8指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcI8(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TINT Val = INST_VAL(0, 8), Rb = T16_REG(8);

	switch (INST_VAL(12, 4)) {
	case 2:
		if (INST_BIT(11))
			return pExeIFun->SetReg(tFlag|DPF_NEG, REG_ZR, Rb, Val, 0, pArg);
		return pExeIFun->SetReg(tFlag, Rb, REG_ZR, Val, 0, pArg);
	case 3: return pExeIFun->SetReg(tFlag|(INST_BIT(11) ? DPF_NEG : 0), Rb, Rb, Val, 0, pArg);
	case 9: return pExeIFun->LoadStore(tFlag|INST_FLAG(11, LSF_LD), Rb, REG_ZR, REG_SP, REG_ZR, Val << 2, pArg);
	default: return pExeIFun->SetReg(tFlag, Rb, INST_BIT(11) ? REG_SP : REG_PC, Val << 2, 0, pArg);
	}
}

/*************************************************************************
* DESCRIPTION
*	解码算术指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcLR(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T16DpLr[16] =
	{
		DPO_AND, DPO_EOR, 0, 0, 0, DPO_ADD|DPF_CRY, DPO_ADD|DPF_NEG|DPF_CRY, 0,
		DPO_AND, DPO_ADD|DPF_NEG, DPO_ADD|DPF_NEG, DPO_ADD, DPO_ORR, DPO_MADD|DPF_RS, DPO_AND|DPF_INV, DPO_ORR|DPF_INV
	};
	static const char Types[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 1, 1, 0, 0, 0, 2};
	const TUINT i = INST_VAL(6, 4), Flag = T16DpLr[i];

	if (!Flag)
		return pExeIFun->SetReg(tFlag|DPO_ADD|DPF_RS|RmFMap2[INST_VAL(6, 1)|(INST_VAL(8, 1) << 1)]
		  , T16_REG(0), REG_ZR, T16_REG(0), T16_REG(3), pArg);
	switch (Types[i]) {
	case 0: return pExeIFun->SetReg(tFlag|Flag, T16_REG(0), T16_REG(0), T16_REG(3), Flag&DPF_RS ? REG_ZR : 0, pArg);
	case 1: return pExeIFun->SetReg(tFlag|Flag, REG_ZR, T16_REG(0), T16_REG(3), 0, pArg);
	default: return pExeIFun->SetReg(tFlag|Flag, T16_REG(0), REG_ZR, T16_REG(3), 0, pArg);
	}
}

/*************************************************************************
* DESCRIPTION
*	解码ADD/SUB register指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcASR(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	const TUINT Rd = T16_REG(0)|(INST_VAL(7, 1) << 3);
	
	switch (INST_VAL(8, 2)) {
	case 0: return pExeIFun->SetReg(tFlag|DPF_RM, Rd, Rd, A32_REG(3), 0, pArg);
	case 1: return pExeIFun->SetReg(tFlag|DPF_RM|DPF_NEG|DPF_PSR, REG_ZR, Rd, A32_REG(3), 0, pArg);
	case 2: return pExeIFun->SetReg(tFlag, Rd, A32_REG(3), 0, 0, pArg);
	default: return pExeIFun->SetReg(tFlag|INST_FLAG(7, DPF_LINK), REG_PC, A32_REG(3), 0, 0, pArg);
	}
}

/*************************************************************************
* DESCRIPTION
*	解码Load from Literal Pool指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16LoadStoreL1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->LoadStore(tFlag, T16_REG(8), REG_ZR, REG_PC, REG_ZR, INST_VAL(0, 8) << 2, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/Store Register指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16LoadStoreR1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T16R1Sz[8] = {LSS_UW, LSS_UH, LSS_UB, LSS_SB|LSF_LD, LSS_UW|LSF_LD, LSS_UH|LSF_LD, LSS_UB|LSF_LD, LSS_SH|LSF_LD};

	return pExeIFun->LoadStore(tFlag|T16R1Sz[INST_VAL(9, 3)], T16_REG(0), REG_ZR, T16_REG(3), T16_REG(6), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Load/Store immediate指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16LoadStoreI1(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT SzFMapI1[9] = {0, 0, 0, 0, 0, 0, LSS_UW, LSS_UB, LSS_UH};
	const TUINT Flag = SzFMapI1[INST_VAL(12, 4)];

	return pExeIFun->LoadStore(tFlag|Flag|INST_FLAG(11, LSF_LD), T16_REG(0), REG_ZR, T16_REG(3), REG_ZR, INST_VAL(6, 5) << LSS_SZ, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码block data transfer指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16LoadStoreBK(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TUINT RegList = INST_VAL(0, 8);

	if (!INST_BIT(12))
		return pExeIFun->LoadStore(tFlag|INST_FLAG(11, LSF_LD), RegList, REG_ZR, T16_REG(8), REG_ZR, 0, pArg);
	if (INST_BIT(11)) {
		tFlag |= (LST_IA|LSF_LD);
		RegList |= INST_FLAG(8, 1 << REG_PC);
	} else {
		tFlag |= (LST_DB|LSF_BKD|LSF_BKB);
		RegList |= INST_FLAG(8, 1 << REG_LR);
	}
	return pExeIFun->LoadStore(tFlag, RegList, REG_ZR, REG_SP, REG_ZR, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Branch, branch with link指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16BranchIM(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	TUINT Cond;
	
	if (INST_BIT(12)) { /* cond */
		Cond = INST_VAL(8, 4);
		if (Cond == 14)
			return pExeIFun->Undef(Inst, 0, pArg);
		if (Cond == 15)
			return pExeIFun->Misc(tFlag|MIT_SVC|PSR_AL, INST_VAL(0, 8), pArg);
		return pExeIFun->SetReg(tFlag|(Cond << PSR_S)|DPO_ADD, REG_PC, REG_PC, INST_SVAL(0, 8) << 1, 0, pArg);
	}
	return pExeIFun->SetReg(tFlag|DPO_ADD, REG_PC, REG_PC, INST_SVAL(0, 11) << 1, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码CBNZ, CBZ指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16BranchCI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	return pExeIFun->SetReg(tFlag|(INST_BIT(11) ? PSR_NE : PSR_EQ), REG_PC, T16_REG(0), (INST_VAL(3, 5) << 1)|(INST_VAL(9, 1) << 6), 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码Bitfield指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcBF(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T16DpBf[4] = {DPO_SXTH, DPO_SXTB, DPO_UXTH, DPO_UXTB};

	return pExeIFun->SetReg(tFlag|T16DpBf[INST_VAL(6, 2)], T16_REG(0), T16_REG(3), 0, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	解码杂项指令
*
* PARAMETERS
*	Inst: 指令
*	tFlag: 标志
*	pExeIFun: 回调函数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int T16DataProcMI(TUINT Inst, TUINT tFlag, DC_INSTFUN *pExeIFun, void *pArg)
{
	static const TUINT T16DpMi[8] = {MIT_NOP, MIT_YIELD, MIT_WFE, MIT_WFI, MIT_SEV, MIT_SEVL};
	TUINT Flag;
	
	switch (Inst&0x0F00) {
	case 0x06:
		if ((Inst&0x0FE0) == 0x0660)
			return pExeIFun->Misc(tFlag|(INST_BIT(4) ? MIT_MSR2 : MIT_MSR3), INST_VAL(0, 4), pArg);
		else if ((Inst&0x0FE0) == 0x0640)
			return pExeIFun->Misc(tFlag|MIT_SETEND, INST_VAL(3, 1), pArg);
		return pExeIFun->Undef(Inst, 0, pArg);
	case 0x0A:
		switch (INST_VAL(6, 2)) {
		case 0: return pExeIFun->SetReg(tFlag|DPO_REV, T16_REG(0), T16_REG(3), 0, 0, pArg);
		case 1: return pExeIFun->SetReg(tFlag|DPO_REV16, T16_REG(0), T16_REG(3), 0, 0, pArg);
		case 2: return pExeIFun->Misc(tFlag|MIT_HLT, INST_VAL(0, 6), pArg);
		default: return pExeIFun->Undef(Inst, 1, pArg); /* REVSH */
		}
	case 0x0E: return pExeIFun->Misc(tFlag|MIT_BRK, INST_VAL(0, 8), pArg);
	default:;
	}
	if (INST_VAL(0, 4))
		return pExeIFun->Misc(tFlag|MIT_IT|(INST_VAL(4, 4) << PSR_S), INST_VAL(0, 8), pArg);
	Flag = T16DpMi[INST_VAL(4, 3)];
	if (Flag && !INST_BIT(7))
		return pExeIFun->Misc(tFlag|Flag, 0, pArg);
	return pExeIFun->Undef(Inst, 0, pArg);
}

/*************************************************************************
* DESCRIPTION
*	T16/T32指令解码
*
* PARAMETERS
*	Inst: 指令
*	pInstSz: 返回指令长度
*	pExeIFun: 回调函数结构体
*	pArg: 参数
*
* RETURNS
*	回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ThumbInstDec(TUINT Inst, int *pInstSz, DC_INSTFUN *pExeIFun, void *pArg)
{
	static INST_DISPATCH T32OpLS1[] =
	{
		{0x000001C0, 0x00000000, RDM_4B|PSR_AL, InstUnImplement}, /* SRS, SRSDA, SRSDB, SRSIA, SRSIB/RFE, RFEDA, RFEDB, RFEIA, RFEIB */
		{0x000001C0, 0x00000080, RDM_4B|PSR_AL|LSS_UW, T32LoadStoreBK}, /* STMIA/LDMIA */
		{0x000001C0, 0x00000100, RDM_4B|PSR_AL|LSS_UW, T32LoadStoreBK}, /* STMDB/LDMDB */
		{0x000001C0, 0x00000180, RDM_4B|PSR_AL, InstUnImplement}, /* SRS, SRSDA, SRSDB, SRSIA, SRSIB/RFE, RFEDA, RFEDB, RFEIA, RFEIB */
		{0x000001E0, 0x00000040, RDM_4B|PSR_AL|LSS_UW|LSF_EX|LST_EX, T32LoadStoreI8}, /* Load/store exclusive */
		{0x000001E0, 0x00000060, RDM_4B|PSR_AL|LSS_UW, T32LoadStoreI8},
		{0x00000140, 0x00000140, RDM_4B|PSR_AL|LSS_UW, T32LoadStoreI8},
		{0x000001E0, 0x000000C0, RDM_4B|PSR_AL, T32LoadStoreEX},
	};
	static INST_DISPATCH T32OpLS2[] = /* Load/store single */
	{
		{0x00000100, 0x00000000, RDM_4B|PSR_AL, T32LoadStoreSD},
		{0x00000110, 0x00000110, RDM_4B|PSR_AL, T32LoadStoreSD},
		{0x00000110, 0x00000100, RDM_4B|PSR_AL, InstUnImplement}, /* SIMD */
	};
	static INST_DISPATCH T32OpDP1[] = /* Data-processing (shifted register) */
	{
		{0x00000000, 0x00000000, RDM_4B|PSR_AL|DPF_RM, T32DataProcLR},
	};
	static INST_DISPATCH T32OpDP2[] = /* Data-processing (register) on page F3-2553 */
	{
		{0x00F001E0, 0x00000000, RDM_4B|PSR_AL|DPO_ADD|DPF_RM|DPF_RS, T32DataProcMV},
		{0x00800180, 0x00800000, RDM_4B|PSR_AL|RMO_ROR, T32DataProcLR2},
		{0x00800180, 0x00000080, RDM_4B|PSR_AL, InstUnImplement}, /* Parallel addition and subtraction */
		{0x00C00180, 0x00800080, RDM_4B|PSR_AL, T32Misc1},
		{0x00000180, 0x00000100, RDM_4B|PSR_AL, InstUnImplement}, /* Multiply, multiply accumulate, and absolute difference */
		{0x00000180, 0x00000180, RDM_4B|PSR_AL, InstUnImplement}, /* Long multiply and divide */
	};
	static INST_DISPATCH T32BLDP[] =
	{
		{0x80000200, 0x00000000, RDM_4B|PSR_AL, T32DataProcLR},
		{0x80000200, 0x00000200, RDM_4B|PSR_AL, T32DataProcLI},
		{0xD0000200, 0x80000000, RDM_4B|DPO_ADD, T32Branch},
		{0xD0000300, 0x80000200, RDM_4B|DPO_ADD, T32Branch},
		{0xD0000300, 0x80000300, RDM_4B|DPO_ADD, T32Branch},
		{0xD0000000, 0x90000000, RDM_4B|PSR_AL|DPO_ADD, T32Branch},
		{0xC0000000, 0xC0000000, RDM_4B|PSR_AL|DPO_ADD, T32Branch},
		{0xD0000380, 0x80000380, RDM_4B|PSR_AL, T32Misc2},
	};
	static INST_DISPATCH T32OpVLS[] = /* VFP/SIMD load/store */
	{
		{0x0E0001A0, 0x0A000120, RMS_4B|RDS_VFP|PSR_AL, T32LoadStoreVBK}, /* VLDM/VSTM */
		{0x0E000180, 0x0A000080, RMS_4B|RDS_VFP|PSR_AL, T32LoadStoreVBK}, /* VLDM/VSTM */
		{0x0E000120, 0x0A000100, RMS_4B|RDS_VFP|PSR_AL, T32LoadStoreVBK}, /* VSTR/VLDR */
		{0x0E0001E0, 0x0A000040, RDM_4B|PSR_AL, InstUnImplement}, /* 64-bit transfers accessing the SIMD and floating-point register file */
		{0x08000000, 0x00000000, RDM_4B|PSR_AL, InstUnImplement}, /* Coprocessor */
		{0x0E000000, 0x08000000, RDM_4B|PSR_AL, InstUnImplement}, /* Coprocessor */
		{0x0C000000, 0x0C000000, RDM_4B|PSR_AL, InstUnImplement}, /* Coprocessor */
	};
	static const struct
	{
		INST_DISPATCH *pDisp;
		unsigned Cnt;
	} T32OpCodes[] =
	{
		{NULL, 0}, /* 16bit */
		{NULL, 0},
		{NULL, 0},
		{NULL, 0},
		{T32OpLS1, ARY_CNT(T32OpLS1)},
		{T32OpDP1, ARY_CNT(T32OpDP1)},
		{T32OpVLS, ARY_CNT(T32OpVLS)},
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)},
		{T32BLDP, ARY_CNT(T32BLDP)},
		{T32BLDP, ARY_CNT(T32BLDP)},
		{T32BLDP, ARY_CNT(T32BLDP)},
		{T32BLDP, ARY_CNT(T32BLDP)},
		{T32OpLS2, ARY_CNT(T32OpLS2)},
		{T32OpDP2, ARY_CNT(T32OpDP2)},
		{NULL, 0},
		{ArmOpUnImpl, ARY_CNT(ArmOpUnImpl)},
	};
	static INST_DISPATCH T16OpDP1[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL|DPF_PSR|DPF_RM|DPO_ADD, T16DataProcMV},
	};
	static INST_DISPATCH T16OpDP2[] =
	{
		{0x0800, 0x0000, RDM_4B|PSR_AL|DPF_PSR|DPF_RM|DPO_ADD, T16DataProcMV},
		{0x0800, 0x0800, RDM_4B|PSR_AL|DPF_PSR|DPO_ADD, T16DataProcAS},
	};
	static INST_DISPATCH T16OpDP3[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL|DPF_PSR|DPO_ADD, T16DataProcI8},
	};
	static INST_DISPATCH T16OpMI1[] =
	{
		{0x0C00, 0x0000, RDM_4B|PSR_AL|DPF_PSR|DPF_RM, T16DataProcLR},
		{0x0C00, 0x0400, RDM_4B|PSR_AL|DPO_ADD, T16DataProcASR},
		{0x0800, 0x0800, RDM_4B|PSR_AL|LSF_LD|LSS_UW, T16LoadStoreL1},
	};
	static INST_DISPATCH T16OpLS1[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL, T16LoadStoreR1},
	};
	static INST_DISPATCH T16OpLS2[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL, T16LoadStoreI1},
	};
	static INST_DISPATCH T16OpLS3[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL|LSS_UW, T16DataProcI8},
	};
	static INST_DISPATCH T16OpDP4[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL|DPO_ADD, T16DataProcI8},
	};
	static INST_DISPATCH T16OpMI2[] =
	{
		{0x0F00, 0x0000, RDM_4B|PSR_AL|DPO_ADD, T16DataProcI7},
		{0x0500, 0x0100, RDM_4B|DPO_BCPR, T16BranchCI},
		{0x0F00, 0x0200, RDM_4B|PSR_AL, T16DataProcBF},
		{0x0600, 0x0400, RDM_4B|PSR_AL|LSS_UW|LSF_WB|LSF_BLK, T16LoadStoreBK},
		{0x0F00, 0x0A00, RDM_4B|PSR_AL, T16DataProcMI},
		{0x0F00, 0x0600, RDM_4B|PSR_AL, T16DataProcMI},
		{0x0E00, 0x0E00, RDM_4B|PSR_AL, T16DataProcMI},
	};
	static INST_DISPATCH T16OpLSB[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL|LST_IA|LSS_UW|LSF_WB|LSF_BLK, T16LoadStoreBK},
	};
	static INST_DISPATCH T16OpBL1[] =
	{
		{0x0000, 0x0000, RDM_4B, T16BranchIM},
	};
	static INST_DISPATCH T16OpBL2[] =
	{
		{0x0000, 0x0000, RDM_4B|PSR_AL, T16BranchIM},
	};
	static const struct
	{
		INST_DISPATCH *pDisp;
		unsigned Cnt;
	} T16OpCodes[] =
	{
		{T16OpDP1, ARY_CNT(T16OpDP1)}, /* Shift (immediate), add, subtract, move, and compare on page F3-2532 */
		{T16OpDP2, ARY_CNT(T16OpDP2)},
		{T16OpDP3, ARY_CNT(T16OpDP3)},
		{T16OpDP3, ARY_CNT(T16OpDP3)},
		{T16OpMI1, ARY_CNT(T16OpMI1)}, /* Data-processing on page F3-2533 */
		{T16OpLS1, ARY_CNT(T16OpLS1)}, /* Load/store single data item on page F3-2535 */
		{T16OpLS2, ARY_CNT(T16OpLS2)},
		{T16OpLS2, ARY_CNT(T16OpLS2)},
		{T16OpLS2, ARY_CNT(T16OpLS2)},
		{T16OpLS3, ARY_CNT(T16OpLS3)},
		{T16OpDP4, ARY_CNT(T16OpDP4)}, /* Generate SP/PC-relative address */
		{T16OpMI2, ARY_CNT(T16OpMI2)}, /* Miscellaneous 16-bit instructions on page F3-2536 */
		{T16OpLSB, ARY_CNT(T16OpLSB)}, /* Load/Store multiple registers */
		{T16OpBL1, ARY_CNT(T16OpBL1)}, /* Conditional branch, and Supervisor Call on page F3-2537 */
		{T16OpBL2, ARY_CNT(T16OpBL2)}, /* Unconditional Branch */
	};
	INST_DISPATCH *pDisp;
	unsigned i, Cnt;

	if ((Inst&0xF800) > 0xE000) {
		*pInstSz = 4;
		pDisp = T32OpCodes[i = INST_VAL(9, 4)].pDisp;
		Cnt = T32OpCodes[i].Cnt;
	} else {
		Inst &= 0xFFFF;
		*pInstSz = 2;
		pDisp = T16OpCodes[i = INST_VAL(12, 4)].pDisp;
		Cnt = T16OpCodes[i].Cnt;
	}
	for (i = 0; i < Cnt; i++) {
		if ((Inst&pDisp[i].Mask) == pDisp[i].Val)
			return pDisp[i].InstDec(Inst, pDisp[i].tFlag, pExeIFun, pArg);
	}
	return pExeIFun->Undef(Inst, 0, pArg);
}
#endif
#endif

#define FUNC_ADDR(Addr) Addr

typedef struct _ARMEX_ARG
{
	TSIZE RegAry[REG_NUM + 1/* REG_ZR */], Cpsr, RegMask; /* 推导时动态有效位图, REG_ZR有效! */
	TSIZE OrgSp, *pVal, *pRegAry, rRegMask, RegMask2/* 忽略位图, 包括REG_ZR */;
	TADDR *pFaultAddr;

#define ARMEX_EVJMP 1
	unsigned Event;
	FILE *fp;
} ARMEX_ARG;

typedef struct _ARMJMP_ARG
{
	TUINT *pBitMap, (*pJmpTbl)[2];
	const void *pInst;
	TSIZE Pc, Size;
	TSSIZE Off;
	TUINT TblCnt, PcOff, InstShift, IsAlJmp;
} ARMJMP_ARG;

typedef struct _ARMMASK_ARG
{
	TSIZE RegMask;
	TUINT *pBitInf, IsFirst;
} ARMMASK_ARG;

static const char * const CondStr[] = {"EQ", "NE", "CS", "CC", "MI", "PL", "VS", "VC", "HI", "LS", "GE", "LT", "GT", "LE", "", ""};

/*************************************************************************
* DESCRIPTION
*	显示前缀信息(文件名/行号)
*
* PARAMETERS
*	fp: 文件句柄
*	pSymHdl: sym句柄
*	Addr: 地址
*	pIsFirst: 是否是第1次调用
*
* RETURNS
*	需要释放的内存
*
* LOCAL AFFECTED
*
*************************************************************************/
static char *DispPreAsmInf(FILE *fp, SYM_HDL *pSymHdl, TADDR Addr, int *pIsFirst)
{
	static char *pFnBk;
	static TUINT LnBk;
	TUINT LineNo;
	char *pPath;

	if (*pIsFirst) {
		*pIsFirst = 0;
		pFnBk = NULL;
		LnBk  = 0;
	}
	pPath = SymAddr2Line(pSymHdl, &LineNo, Addr);
	if (pPath) {
		if (!pFnBk || strcmp(pFnBk, pPath)) {
			Free(pFnBk);
			pFnBk = pPath;
			LOGC(fp, "\t%s\n", pPath);
		} else {
			Free(pPath);
		}
		if (LnBk != LineNo)
			LOGC(fp, "\t%-4d: ", LineNo);
		else
			LOGC(fp, "\t      ");
		LnBk  = LineNo;
	} else {
		LOGC(fp, "\t      ");
	}
	LOGC(fp, TADDRFMT2":  ", Addr);
	return pFnBk;
}

/*************************************************************************
* DESCRIPTION
*	显示寄存器名字
*
* PARAMETERS
*	fp: 文件句柄
*	pStr: 前置字符串
*	Flag: 标志
*	Idx: 索引
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispRegName(FILE *fp, const char *pStr, TUINT Flag, TINT Idx)
{
	static const char PreVfp[] = {'B', 'H', 'S', 'D', 'Q'};
#ifdef SST64
	static const char PreGp[] = {'?', '?', 'W', 'X', '?'};
#else
	static const char PreGp[] = {'?', '?', 'R', '?', '?'};
#endif
	const TUINT i = RDS_SZ;

	if (PreGp[i] == '?' || Idx == REG_ZR) {
		LOGC(fp, "%s??", pStr);
		DBGERR("寄存器宽度或索引溢出\n");
		return;
	}
	if (Flag&RDS_VFP) {
		LOGC(fp, "%s%c%d", pStr, PreVfp[i], Idx);
		return;
	}
	if (Idx == REG_PC) {
		LOGC(fp, "%sPC", pStr);
	} else if (Idx == REG_SP) {
#ifdef SST64
		LOGC(fp, RDS_SZ == 2 ? "%sWSP" : "%sSP", pStr);
#else
		LOGC(fp, "%sSP", pStr);
#endif
	} else {
		LOGC(fp, "%s%c%d", pStr, PreGp[i], Idx);
	}
}

/*************************************************************************
* DESCRIPTION
*	显示数据处理反汇编码
*
* PARAMETERS
*	fp: 文件句柄
*	Flag: 标志
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispDPOperaton(FILE *fp, TUINT Flag)
{
	static const char * const DpOpStrs[] =
	{
		"???", "ADD", "RSB", "AND", "ORR", "EOR", "ADRP", "MOVK", "LSL", "LSR", "ASR", "ROR", "EXTR", "SXTB", "SXTH", "SXTW", "UXTB",
		"UXTH", "BFM", "SBFM", "UBFM", "CCMN", "CSEL", "RBIT", "REV", "REV16", "REV32", "CLZ", "CLS", "MADD", "MULH", "???", "???",
		"MOV", "ADC", "SUB", "SBC", "BIC", "ORN", "EON", "CMN", "CMP", "TST", "TEQ", "MVN", "NEG", "NGC", "CCMP", "CSINC", "CSINV",
		"CSNEG", "CSET", "CSETM", "MUL", "MNEG", "MSUB",
	};

	if (Flag&DPF_SIG)
		LOGC(fp, "S");
	LOGC(fp, "%s", DpOpStrs[DPO_IDX]);
#ifdef _DEBUG
	if (DpOpStrs[DPO_IDX][0] == '?')
		DBGERR("错误的DP操作符: %d\n", DPO_IDX);
#endif
	if (Flag&DPF_PSR)
		LOGC(fp, "S");
	LOGC(fp, "%s", CondStr[PSR_IDX]);
	return;
}

/*************************************************************************
* DESCRIPTION
*	提升Rm操作符为数据处理符号
*	前提是Rb == REG_ZR, Rm有值, DpOp为ADD/ORR/EOR
*
* PARAMETERS
*	Flag: 标志
*	Val: 立即数, 有DPF_RS时为Rs寄存器
*
* RETURNS
*	转换后的标志
*
* LOCAL AFFECTED
*
*************************************************************************/
static TUINT RmOper2DpOper(TUINT Flag, TSSIZE Val)
{
	const TUINT tFlag = Flag&~(((((TUINT)1 << RMO_B) - 1) << RMO_S)|((((TUINT)1 << DPO_B) - 1) << DPO_S));

	switch (RMO_VAL) {
	case RMO_LSL: return tFlag|(Val || (tFlag&DPF_RS) ? DPO_LSL : DPO_MOV);
	case RMO_LSR: return tFlag|(Val || (tFlag&DPF_RS) ? DPO_LSR : DPO_MOV);
	case RMO_ASR: return tFlag|(Val || (tFlag&DPF_RS) ? DPO_ASR : DPO_MOV);
	case RMO_ROR: return tFlag|(Val || (tFlag&DPF_RS) ? DPO_ROR : DPO_MOV);
	case RMO_UXTB: return tFlag|DPO_UXTB;
	case RMO_UXTH: return tFlag|DPO_UXTH;
	case RMO_SXTB: return tFlag|DPO_SXTB;
	case RMO_SXTH: return tFlag|DPO_SXTH;
	case RMO_SXTW: return tFlag|DPO_SXTW;
	default: return Flag;
	}
}

/*************************************************************************
* DESCRIPTION
*	转换数据处理别名指令
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 有DPF_PSR时可能为REG_ZR
*	Rb: 源寄存器, 可能为REG_ZR
*	Rm: 第2寄存器, 可能为REG_ZR
*	Val: 立即数, 有DPF_RS时为Rs寄存器
*
* RETURNS
*	转换后的标志
*
* LOCAL AFFECTED
*
*************************************************************************/
static TUINT ConvertDpAlias(TUINT Flag, TINT Rd, TINT Rb, TINT Rm, TSSIZE Val)
{
	const TUINT DpOp = DPO_VAL, tFlag = Flag^DpOp;

	switch (DpOp) {
	case DPO_ADD:
		if ((tFlag&DPF_NEG) == DPF_NEG) {
			if (tFlag&DPF_CRY)
				return tFlag|(Rb == REG_ZR ? DPO_NGC : DPO_SBC);
			if (Rd == REG_ZR && (tFlag&DPF_PSR))
				return (tFlag^DPF_PSR)|DPO_CMP;
			if (Rb == REG_ZR)
				return tFlag|DPO_NEG;
			if (Rm == REG_ZR && !Val)
				return tFlag|DPO_MOV;
			return tFlag|DPO_SUB;
		}
		if (tFlag&DPF_CRY)
			return tFlag|DPO_ADC;
		if (Rd == REG_ZR && (tFlag&DPF_PSR))
			return (tFlag^DPF_PSR)|DPO_CMN;
		if (Rm == REG_ZR && (Rb == REG_ZR || !Val))
			return tFlag|DPO_MOV;
		if (Rb == REG_ZR)
			return RmOper2DpOper(Flag, Val);
		break;
	case DPO_AND:
		if (Rd == REG_ZR && (tFlag&DPF_PSR))
			return (tFlag^DPF_PSR)|DPO_TST;
		if (tFlag&DPF_INV)
			return tFlag|DPO_BIC;
		break;
	case DPO_ORR:
		if (tFlag&DPF_INV)
			return tFlag|(Rb == REG_ZR ? DPO_MVN : DPO_ORN);
		if (Rb == REG_ZR)
			return Rm == REG_ZR ? tFlag|DPO_MOV : RmOper2DpOper(Flag, Val);
		break;
	case DPO_EOR:
		if (Rd == REG_ZR && (tFlag&DPF_PSR))
			return (tFlag^DPF_PSR)|DPO_TEQ;
		if (tFlag&DPF_INV)
			return tFlag|DPO_EON;
		if (Rb == REG_ZR)
			return Rm == REG_ZR ? tFlag|DPO_MOV : RmOper2DpOper(Flag, Val);
		break;
	case DPO_CCMN:
		if ((tFlag&DPF_NEG) == DPF_NEG)
			return tFlag|DPO_CCMP;
		break;
	case DPO_CSEL:
		switch (tFlag&DPF_NEG) {
		case DPF_INC: return tFlag|(Rb == REG_ZR && Rm == REG_ZR && (PSR_IDX >> 1) != 7 ? DPO_CSET : DPO_CSINC);
		case DPF_INV: return tFlag|(Rb == REG_ZR && Rm == REG_ZR && (PSR_IDX >> 1) != 7 ? DPO_CSETM : DPO_CSINV);
		case DPF_NEG: return tFlag|DPO_CSNEG;
		default:;
		}
		break;
	case DPO_MADD:
		if (Val == REG_ZR)
			return tFlag|((tFlag&DPF_NEG) == DPF_NEG ? DPO_MNEG : DPO_MUL);
		if ((tFlag&DPF_NEG) == DPF_NEG)
			return tFlag|DPO_MSUB;
		break;
	default:;
	}
	return Flag;
}

/*************************************************************************
* DESCRIPTION
*	显示PC跳转指令
*
* PARAMETERS
*	Flag: 标志
*	Rb: 源寄存器
*	Val/Val2: 立即数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispSetPc(TUINT Flag, TINT Rb, TSSIZE Val, TUINT Val2, void *pArg)
{
	ARMEX_ARG * const pArmExArg = pArg;
	FILE * const fp = pArmExArg->fp;
	
	switch (DPO_VAL) {
	case DPO_ADD:
		if (Rb == REG_LR && PSR_VAL == PSR_AL) {
			LOGC(fp, "RET");
			break;
		}
		LOGC(fp, "B");
		if (Flag&DPF_LINK)
			LOGC(fp, "L");
		if ((Flag&DPF_XCHANGE) || Rb != REG_PC) {
			if (Flag&DPF_XCHANGE)
				pArmExArg->RegAry[REG_PC] &= ~(TSIZE)3;
#ifdef SST64
			LOGC(fp, "R");
#else
			LOGC(fp, "X");
#endif
		}
		LOGC(fp, "%s ", CondStr[PSR_IDX]);
		if (Rb != REG_PC) {
			DispRegName(fp, "", Flag, Rb);
			break;
		}
		Val += pArmExArg->RegAry[REG_PC];
		if (Flag&DPF_LINK) {
			ShowSymVal(fprintf, fp, Val);
			break;
		}
		LOGC(fp, TADDRFMT, Val);
		break;
	case DPO_BCPR:
		LOGC(fp, "CB%sZ", PSR_VAL == PSR_EQ ? "" : "N");
		DispRegName(fp, " ", Flag, Rb);
		LOGC(fp, ", "TADDRFMT, pArmExArg->RegAry[REG_PC] + Val);
		break;
	case DPO_BTST:
		LOGC(fp, "TB%sZ", PSR_VAL == PSR_EQ ? "" : "N");
		DispRegName(fp, " ", Flag, Rb);
		LOGC(fp, ", #%d, "TADDRFMT, Val2, pArmExArg->RegAry[REG_PC] + Val);
		break;
	default: LOGC(fp, "???"); DBGERR("未知的PC跳转指令\n");
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	显示存取反汇编类型
*
* PARAMETERS
*	fp: 文件句柄
*	Flag: 标志
*	pRegFStr: 寄存器标记字符串: "P"/"R"/""
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispLSType(FILE *fp, TUINT Flag, const char *pRegFStr)
{
	static const char *LTStrs[] = {"", "X", "AX", "A", "PRFM", "N", "U", "T", "MDA", "MDB", "MIA", "MIB", "TB"};
	static const char *STStrs[] = {"", "X", "LX", "L", "PRFM", "N", "U", "T", "MDA", "MDB", "MIA", "MIB", "TB"};
	static const char SpShows[] = {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1};
	static const char LsSzType[] = {'B', 'H', 'W', 'X', 'Q'};
	const TUINT i = LST_IDX;

	if (Flag&LSF_LD) {
		if (SpShows[i])
			LOGC(fp, "%s", LTStrs[i]);
		else
			LOGC(fp, "LD%s%s", LTStrs[i], pRegFStr);
	} else {
		LOGC(fp, "ST%s%s", STStrs[i], pRegFStr);
	}
	if (Flag&LSF_SIG)
		LOGC(fp, "S");
	if (RDS_SZ != LSS_SZ)
		LOGC(fp, "%c", LsSzType[LSS_SZ]);
	LOGC(fp, "%s", CondStr[PSR_IDX]);
}

#ifndef SST64
/*************************************************************************
* DESCRIPTION
*	显示块存取指令
*
* PARAMETERS
*	fp: 文件句柄
*	Flag: 标志
*	RegList: 寄存器列表
*	Rb: 源寄存器
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispLsBlock(FILE *fp, TUINT Flag, TINT RegList, TINT Rb)
{
	const char *pStr = ", {";
	TUINT i;

	DispLSType(fp, Flag, "");
	DispRegName(fp, " ", REG_DEFB, Rb);
	if (Flag&LSF_WB)
		LOGC(fp, "!");
	for (i = 0; RegList; i++) {
		if (!TST_BIT(RegList, i))
			continue;
		CLR_BIT(RegList, i);
		DispRegName(fp, pStr, REG_DEFB, i);
		pStr = ", ";
	}
	LOGC(fp, "}");
	if (Flag&LSF_USR)
		LOGC(fp, "^");
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	从数值里获取1的个数
*
* PARAMETERS
*	n: 32位值
*
* RETURNS
*	bit count
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static TSSIZE GetBitCnt(TSIZE n)
{
	n = (n&0x55555555) + ((n >> 1)&0x55555555);
	n = (n&0x33333333) + ((n >> 2)&0x33333333);
	n = (n&0x0f0f0f0f) + ((n >> 4)&0x0f0f0f0f);
	n = (n&0x00ff00ff) + ((n >> 8)&0x00ff00ff);
	n = (n&0x0000ffff) + ((n >> 16)&0x0000ffff);
	return n;
}

/*************************************************************************
* DESCRIPTION
*	获取T16/T32指令识别位图
*
* PARAMETERS
*	pInst/Size: 指令buffer/大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: 指令识别位图(bit0表示是否无效)
*
* LOCAL AFFECTED
*
*************************************************************************/
static TUINT *GetT32InstMap(const void *pInst, size_t Size)
{
	TUINT *pIMap;
	size_t i;

	pIMap = Calloc((Size << 1) + sizeof(TUINT)/* 预留一格, Size有可能在4字节指令中间!!! */);
	if (pIMap) {
		for (i = 0; i < Size; i += 2) {
			if ((OFFVAL_U2(pInst, i)&0xF800) > 0xE000) { /* T32 */
				i += 2;
				pIMap[i >> 1] = 1;
			}
		}
	}
	return pIMap;
}
#endif

/*************************************************************************
* DESCRIPTION
*	记录数据处理指令的跳转信息
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 有DPF_PSR时可能为REG_ZR
*	Rb: 源寄存器, 可能为REG_ZR
*	Val: 立即数, 有DPF_RM时为Rm寄存器
*	Val2: 第2立即数: 仅用于DPO_MVK/DPO_BTST/DPO_CCMN, 有DPF_RS时为Rs寄存器
*	pArg: 参数
*
* RETURNS
*	0: 记录
*	1: 不记录
*
* LOCAL AFFECTED
*
*************************************************************************/
static int JmpSetReg(TUINT Flag, TINT Rd, TINT Rb, TSSIZE Val, TUINT Val2, void *pArg)
{
	ARMJMP_ARG * const pArmJmpArg = pArg;

	(void)Val2;
	if (Rd != REG_PC || (Flag&(RDS_VFP|DPF_LINK|DPF_RM)))
		return 1;
	pArmJmpArg->Off = (TSIZE)1 << (8 * sizeof(TSIZE) - 1);
	if (Flag&DPF_XCHANGE) /* 终止 */
		return 0;
	pArmJmpArg->IsAlJmp = (PSR_VAL >= PSR_AL);
	switch (DPO_VAL) {
	case DPO_ADD:
		if (Rb == REG_PC)
			pArmJmpArg->Off = Val + pArmJmpArg->PcOff;
		else
			pArmJmpArg->Off = (TSIZE)1 << (8 * sizeof(TSIZE) - 1);
		break;
	case DPO_BCPR: case DPO_BTST:
		pArmJmpArg->Off = Val + pArmJmpArg->PcOff;
		break;
	default:;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	记录存取指令的跳转信息
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 无LSF_LD时可能为REG_ZR, 存在LSF_BLK则为寄存器位图!
*	Rd2: 目标寄存器, 可能为REG_ZR
*	Rb: 源寄存器
*	Rm: 第2寄存器, 可能为REG_ZR
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	0: 记录
*	1: 不记录
*
* LOCAL AFFECTED
*
*************************************************************************/
static int JmpLoadStore(TUINT Flag, TINT Rd, TINT Rd2, TINT Rb, TINT Rm, TINT Val, void *pArg)
{
	ARMJMP_ARG * const pArmJmpArg = pArg;

	(void)Rd2, (void)Rb, (void)Rm, (void)Val;
	if (PSR_VAL < PSR_AL || LST_VAL == LST_PM || (Flag&RDS_VFP))
		return 1;
#ifndef SST64
	if (Flag&LSF_BLK) {
		if (!(Rd&((TSIZE)1 << REG_PC)))
			return 1;
	} else
#endif
	{
		if (Rd != REG_PC)
			return 1;
	}
	pArmJmpArg->Off = (TSIZE)1 << (8 * sizeof(TSIZE) - 1);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	记录数据处理指令源寄存器位图
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 有DPF_PSR时可能为REG_ZR
*	Rb: 源寄存器, 可能为REG_ZR
*	Val: 立即数, 有DPF_RM时为Rm寄存器
*	Val2: 第2立即数: 仅用于DPO_MVK/DPO_BTST/DPO_CCMN, 有DPF_RS时为Rs寄存器
*	pArg: 参数
*
* RETURNS
*	0: 记录
*	1: 不记录
*
* LOCAL AFFECTED
*
*************************************************************************/
static int RecSetReg(TUINT Flag, TINT Rd, TINT Rb, TSSIZE Val, TUINT Val2, void *pArg)
{
	ARMMASK_ARG * const pArmMaskArg = pArg;
	const TSIZE DstMask = (TSIZE)1 << Rd;

	if (Flag&RDS_VFP)
		return 1;
	if ((pArmMaskArg->RegMask&DstMask) || pArmMaskArg->IsFirst) { /* Rd在源寄存器位图中 */
		pArmMaskArg->RegMask = (pArmMaskArg->RegMask&~DstMask)|((TSIZE)1 << Rb)|(Flag&DPF_RM ? (TSIZE)1 << Val : 0)|(Flag&DPF_RS ? (TSIZE)1 << Val2 : 0);
		*pArmMaskArg->pBitInf = (Rb == REG_PC && !(Flag&(DPF_RM|DPF_RS)) ? 0xA : 0x2);
		return 0;
	}
	if ((Flag&DPF_LINK) && Rd == REG_PC && (pArmMaskArg->RegMask&(((TSIZE)1 << REG_PNUM) - 1))) {
		pArmMaskArg->RegMask &= ~TMPREG_MASK;
		*pArmMaskArg->pBitInf = 0x12;
		return 0;
	}
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	记录存取指令源寄存器位图
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 无LSF_LD时可能为REG_ZR, 存在LSF_BLK则为寄存器位图!
*	Rd2: 目标寄存器, 可能为REG_ZR
*	Rb: 源寄存器
*	Rm: 第2寄存器, 可能为REG_ZR
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	0: 记录
*	1: 不记录
*
* LOCAL AFFECTED
*
*************************************************************************/
static int RecLoadStore(TUINT Flag, TINT Rd, TINT Rd2, TINT Rb, TINT Rm, TINT Val, void *pArg)
{
	ARMMASK_ARG * const pArmMaskArg = pArg;
	TSIZE DstMask;

	(void)Val;
	if (LST_VAL == LST_PM || (Flag&RDS_VFP))
		return 1;
	if (Flag&LSF_LD) {
#ifndef SST64
		if (Flag&LSF_BLK) {
			DstMask = Rd;
			if ((DstMask&((TSIZE)1 << REG_PC)) && !pArmMaskArg->IsFirst) /* 忽略POP PC */
				return 1;
		} else
#endif
		{
			DstMask = ((TSIZE)1 << Rd)|((TSIZE)1 << Rd2);
		}
		if ((pArmMaskArg->RegMask&DstMask) || pArmMaskArg->IsFirst) { /* Rd在源寄存器位图中 */
			pArmMaskArg->RegMask = (pArmMaskArg->RegMask&~DstMask)|((TSIZE)1 << Rb)|((TSIZE)1 << Rm);
			if (Rb == REG_SP && !pArmMaskArg->IsFirst) {
				pArmMaskArg->RegMask &= ~((TSIZE)1 << REG_SP);
				*pArmMaskArg->pBitInf = 0xE;
				return 0;
			}
			*pArmMaskArg->pBitInf = (pArmMaskArg->RegMask&((TSIZE)1 << REG_PC) ? 0xA : 0x2);
			return 0;
		}
	} else if (pArmMaskArg->IsFirst) {
		pArmMaskArg->RegMask |= ((TSIZE)1 << Rb)|(Flag&LSF_EX ? 0 : (TSIZE)1 << Rm);
		*pArmMaskArg->pBitInf = 0x2;
		return 0;
	}
	if ((Flag&(LSF_WB|LSF_POST)) && (pArmMaskArg->RegMask&((TSIZE)1 << Rb))) { /* Rb在源寄存器位图中 */
		*pArmMaskArg->pBitInf = 0x2;
		return 0;
	}
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	显示未定义指令
*
* PARAMETERS
*	Inst: 指令
*	IsUnImpl: 是否是未完成解码的指令
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispArmUndef(TUINT Inst, int IsUnImpl, void *pArg)
{
	ARMEX_ARG * const pArmExArg = pArg;

	LOGC(pArmExArg->fp, "%s %08X", IsUnImpl ? "UNIMPL" : "UNDEF", Inst);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	显示数据处理指令
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 有DPF_PSR时可能为REG_ZR
*	Rb: 源寄存器, 可能为REG_ZR
*	Val: 立即数, 有DPF_RM时为Rm寄存器
*	Val2: 第2立即数: 仅用于DPO_MVK/DPO_BTST/DPO_CCMN, 有DPF_RS时为Rs寄存器
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char * const RmOpStrs[] = {"", "LSL", "LSR", "ASR", "ROR", "RRX", "UXTB", "UXTH", "UXTW", "SXTB", "SXTH", "SXTW"};
static int DispSetReg(TUINT Flag, TINT Rd, TINT Rb, TSSIZE Val, TUINT Val2, void *pArg)
{
	ARMEX_ARG * const pArmExArg = pArg;
	FILE * const fp = pArmExArg->fp;
	const char *pStr = " ";
	TINT Rm = REG_ZR;

	if (Flag&DPF_RM) {
		Rm = (TINT)Val;
		Val = Val2;
		Val2 = 0;
	}
	if (Rd == REG_PC) {
		if (Rm != REG_ZR)
			DBGERR("错误的PC跳转指令: %d\n", Rm);
		return DispSetPc(Flag, Rb, Val, Val2, pArg);
	}
	Flag = ConvertDpAlias(Flag, Rd, Rb, Rm, Val);
	DispDPOperaton(fp, Flag);
	if (Rd != REG_ZR) {
		DispRegName(fp, pStr, Flag, Rd);
		pStr = ", ";
	}
	if (DPO_VAL == DPO_ADRP) {
		LOGC(fp, ", "TADDRFMT, (pArmExArg->RegAry[REG_PC]&~(TSIZE)0xFFF) + Val);
		return 0;
	}
	if (Rb != REG_ZR)
		DispRegName(fp, pStr, DPO_VAL == DPO_MADD ? RMS_SZ : Flag, Rb);
	if (Rm != REG_ZR) {
		DispRegName(fp, ", ", RMS_SZ, Rm);
		if (RMO_VAL >= RMO_RRX || (((Flag&DPF_RS) || Val) && RMO_VAL != RMO_NULL)) {
			LOGC(fp, ", %s", RmOpStrs[RMO_IDX]);
			if (Val)
				LOGC(fp, " #%d", (TUINT)Val);
			return 0;
		}
	}
	if (Flag&DPF_RS)
		Val != REG_ZR ? DispRegName(fp, ", ", Flag, (TINT)Val) : (void)0;
	else if (Val < 0)
		LOGC(fp, ", #-"TSIZEFMT2, -Val);
	else if (Val > 0)
		LOGC(fp, ", #"TSIZEFMT2, Val);
	else if ((Rb == REG_ZR && Rm == REG_ZR) || ((Rb == REG_ZR || Rm == REG_ZR) && (Rd == REG_ZR || (Flag&DPF_CRY))))
		LOGC(fp, ", #0");
	if (Val2)
		LOGC(fp, ", #%d", Val2);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	显示存取指令
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 无LSF_LD时可能为REG_ZR, 存在LSF_BLK则为寄存器位图!
*	Rd2: 目标寄存器, 可能为REG_ZR
*	Rb: 源寄存器
*	Rm: 第2寄存器, 可能为REG_ZR
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispLoadStore(TUINT Flag, TINT Rd, TINT Rd2, TINT Rb, TINT Rm, TINT Val, void *pArg)
{
	ARMEX_ARG * const pArmExArg = pArg;
	FILE * const fp = pArmExArg->fp;
	const char *pStr = " ";
	TADDR Addr;

#ifndef SST64
	if (Flag&LSF_BLK)
		return DispLsBlock(fp, Flag, Rd, Rb);
#endif
	DispLSType(fp, Flag, Rd2 == REG_ZR ? "R" : "P");
	if ((Flag&(LSF_LD|LSF_EX)) == LSF_EX) {
		DispRegName(fp, pStr, RMS_SZ, Rm);
		Rm = REG_ZR;
		pStr = ", ";
	}
	if (Rd == REG_ZR && !(Flag&LSF_LD))
		LOGC(fp, "%s#0", pStr);
	else if (LST_VAL == LST_PM)
		LOGC(fp, "%s#%d", pStr, Rd);
	else
		DispRegName(fp, pStr, Flag, Rd);
	if (Rd2 != REG_ZR)
		DispRegName(fp, ", ", Flag, Rd2);
	DispRegName(fp, ", [", REG_DEFB, Rb);
	if (Flag&LSF_POST)
		LOGC(fp, "]");
	if (Rm != REG_ZR) {
		DispRegName(fp, Flag&LSF_NEG ? ", -" : ", ", RMS_SZ, Rm);
		if (RMO_VAL >= RMO_RRX || (Val && RMO_VAL != RMO_NULL)) {
			LOGC(fp, ", %s", RmOpStrs[RMO_IDX]);
			if (Val)
				LOGC(fp, " #%d", Val);
			goto _OUT;
		}
	}
	if (Val < 0)
		LOGC(fp, ", #-0x%X", -Val);
	else if (Val > 0)
		LOGC(fp, ", #0x%X", Val);
_OUT:
	if (!(Flag&LSF_POST))
		LOGC(fp, "]");
	if (Flag&LSF_WB)
		LOGC(fp, "!");
	if (Rb == REG_PC && Rm == REG_ZR && !DumpMem((pArmExArg->RegAry[REG_PC]&~(TSIZE)3) + (TSSIZE)Val, &Addr, sizeof(Addr))) {
		LOGC(fp, "\t; = ");
		ShowSymVal(fprintf, fp, Addr);
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	显示杂项指令
*
* PARAMETERS
*	Flag: 标志
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DispMisc(TUINT Flag, TUINT Val, void *pArg)
{
	static const char * const MiOps[] =
	{
		"SVC", "HVC", "SMC", "BRK", "HLT", "DCPS1", "DCPS2", "DCPS3", "NOP", "YIELD", "WFE", "WFI", "SEV", "SEVL",
		"CLREX", "DSB", "DMB", "ISB", "MSR SPSel", "MSR DAIFSet", "MSR DAIFClr", "SETEND", "IT", "DRPS", "ERET",
	};
	ARMEX_ARG * const pArmExArg = pArg;
	FILE * const fp = pArmExArg->fp;

	LOGC(fp, "%s%s", MiOps[MIT_IDX], CondStr[PSR_IDX]);
	if (Val)
		LOGC(fp, " #0x%X", Val);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	对于Rm进行运算
*
* PARAMETERS
*	Flag: 标志
*	RmVal: Rm值
*	CryBit: 进位
*	Val: 立即数
*
* RETURNS
*	运算结果
*
* LOCAL AFFECTED
*
*************************************************************************/
static TSSIZE CalcRmVal(TUINT Flag, TSIZE RmVal, TUINT CryBit, TSSIZE Val)
{
	const TUINT mvBits = 8 << RMS_SZ;

	if (mvBits < sizeof(RmVal) * 8) /* 1 << 32 = 1!!! */
		RmVal &= (((TSIZE)1 << mvBits) - 1);
	switch (RMO_VAL) {
	case RMO_LSL: RmVal <<= Val; break;
	case RMO_LSR: RmVal >>= Val; break;
	case RMO_ASR: RmVal = SEXT_VAL(RmVal, mvBits) >> Val; break;
	case RMO_ROR: RmVal = (RmVal >> Val)|(RmVal << (mvBits - Val)); break;
	case RMO_RRX: RmVal = (RmVal >> 1)|((TSIZE)CryBit << (mvBits - 1)); break;
	case RMO_UXTB: RmVal = (RmVal&0xFF) << Val; break;
	case RMO_UXTH: RmVal = (RmVal&0xFFFF) << Val; break;
	case RMO_UXTW: RmVal = (RmVal&0xFFFFFFFF) << Val; break;
	case RMO_SXTB: RmVal = SEXT_VAL(RmVal, 8) << Val; break;
	case RMO_SXTH: RmVal = SEXT_VAL(RmVal, 16) << Val; break;
	case RMO_SXTW: RmVal = SEXT_VAL(RmVal, 32) << Val; break;
	default: RmVal += Val;
	}
	return (mvBits < sizeof(RmVal) * 8 ? RmVal&(((TSIZE)1 << mvBits) - 1) : RmVal);
}

/*************************************************************************
* DESCRIPTION
*	寄存器内容和内存地址之间的交互
*
* PARAMETERS
*	Flag: 标志
*	RwBytes: 内存宽度
*	Addr: 读写地址
*	Rd: 寄存器索引
*	pArmExArg: 参数
*
* RETURNS
*	异常类型
*
* LOCAL AFFECTED
*
*************************************************************************/
static EX_TYPE LoadRegVal(TUINT Flag, TUINT RwBytes, TADDR Addr, TINT Rd, ARMEX_ARG *pArmExArg)
{
	static const unsigned RwFlags[] = {VMA_R, VMA_W};
	VMA_HDL *pVmaHdl;
	const void *p;
	TSIZE Val;

	pVmaHdl = GetVmaHdl(Addr);
	if (!pVmaHdl || !(RwFlags[!(Flag&LSF_LD)]&pVmaHdl->Flag&VMA_RW)) {
		if (pVmaHdl)
			PutVmaHdl(pVmaHdl);
		return (Flag&LSF_LD ? EX_DATA_EAR : EX_DATA_EAW);
	}
	if ((Flag^LSF_LD)&(LSF_LD|RDS_VFP)) { /* 忽略写/VFP */
		PutVmaHdl(pVmaHdl);
		return EX_NORMAL;
	}
	if (Addr&(RwBytes - 1)) {
		PutVmaHdl(pVmaHdl);
		return EX_DATA_UNA;
	}
	if (sizeof(Val) < RwBytes || (p = Vma2Mem(pVmaHdl, Addr, RwBytes)) == NULL) {
		PutVmaHdl(pVmaHdl);
		if (sizeof(Val) < RwBytes)
			DBGERR("LSSS2溢出: %d\n", RwBytes);
		return EX_FAIL;
	}
	Val = 0;
	memcpy(&Val, p, RwBytes);
	PutVmaHdl(pVmaHdl);
	if (Flag&LSF_SIG) /* 符号扩展 */
		Val = SEXT_VAL(Val, RwBytes * 8);
#ifndef SST64
	else if (LST_VAL == LST_TB)
		Val = pArmExArg->RegAry[Rd] + (Val << 1);
#endif
	pArmExArg->RegAry[Rd] = Val;
	SET_BIT(pArmExArg->RegMask, Rd);
	if (!TST_BIT(pArmExArg->rRegMask|pArmExArg->RegMask2, Rd)) {
		SET_BIT(pArmExArg->rRegMask, Rd);
		pArmExArg->pRegAry[Rd] = Val;
	}
	return EX_NORMAL;
}

/*************************************************************************
* DESCRIPTION
*	执行未定义指令
*
* PARAMETERS
*	Inst: 指令
*	IsUnImpl: 是否是未完成解码的指令
*	pArg: 参数
*
* RETURNS
*	EX_TYPE
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ExecArmUndef(TUINT Inst, int IsUnImpl, void *pArg)
{
	(void)Inst, (void)pArg;
	return IsUnImpl ? EX_FAIL : EX_INST_UND;
}

/*************************************************************************
* DESCRIPTION
*	执行数据处理指令
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 有DPF_PSR时可能为REG_ZR
*	Rb: 源寄存器, 可能为REG_ZR
*	Val: 立即数, 有DPF_RM时为Rm寄存器
*	Val2: 第2立即数: 仅用于DPO_MVK/DPO_BTST/DPO_CCMN, 有DPF_RS时为Rs寄存器
*	pArg: 参数
*
* RETURNS
*	EX_TYPE
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ExecSetReg(TUINT Flag, TINT Rd, TINT Rb, TSSIZE Val, TUINT Val2, void *pArg)
{
	ARMEX_ARG * const pArmExArg = pArg;
	const TUINT dvBits = 8 << RDS_SZ;
	TSIZE SrcMask, dvMask, RbVal;
	TINT Rm = REG_ZR;

	if (Rd == REG_ZR || (Flag&RDS_VFP)) /* cmp/tst/ccmn类指令 */
		return EX_NORMAL;
	if (Rd == REG_PC) {
		if (Flag&DPF_LINK) { /* 不执行BL指令 */
			pArmExArg->RegMask &= ~TMPREG_MASK;
			goto _FAIL;
		}
		pArmExArg->Event = (Rb == REG_PC ? ARMEX_EVJMP : 0);
	}
	if (Flag&DPF_RM) {
		Rm = (TINT)Val;
		Val = Val2;
		Val2 = 0;
	}
	SrcMask = ((TSIZE)1 << Rb)|((TSIZE)1 << Rm);
	if (Flag&DPF_RS) {
		SrcMask |= ((TSIZE)1 << Val);
		Val = pArmExArg->RegAry[Val];
	}
	if ((pArmExArg->RegMask&SrcMask) != SrcMask || RMO_VAL == RMO_RRX)
		goto _FAIL;
	Val = CalcRmVal(Flag, pArmExArg->RegAry[Rm], 0, Val);
	if (Flag&DPF_INV)
		Val = ~Val;
	if (Flag&DPF_INC)
		Val++;
	dvMask = (dvBits < sizeof(TSIZE) * 8 ? ((TSIZE)1 << dvBits) - 1 : ~(TSIZE)0);
	RbVal = pArmExArg->RegAry[Rb]&dvMask;
	switch (DPO_VAL) {
	case DPO_ADD: if (Flag&DPF_CRY) goto _FAIL; Val += RbVal; break;
	case DPO_AND: Val &= RbVal; break;
	case DPO_ORR: Val |= RbVal; break;
	case DPO_EOR: Val ^= RbVal; break;
	case DPO_ADRP: Val = (RbVal&~(TSIZE)0xFFF) + Val; break;
	case DPO_MVK: Val = (RbVal&~((TSIZE)0xFFFF << Val2))|(Val << Val2); break;
	case DPO_LSL: Val = RbVal << Val; break;
	case DPO_LSR: Val = RbVal >> Val; break;
	case DPO_ASR: Val = SEXT_VAL(RbVal, dvBits) >> Val; break;
	case DPO_ROR: Val2 = (TUINT)Val; Val = RbVal; /* 进入下一个case */
	case DPO_EXT: Val = (Val >> Val2)|(RbVal << (dvBits - Val2)); break;
	case DPO_SXTB: Val = SEXT_VAL(RbVal, 8); break;
	case DPO_SXTH: Val = SEXT_VAL(RbVal, 16); break;
	case DPO_SXTW: Val = SEXT_VAL(RbVal, 32); break;
	case DPO_UXTB: Val = RbVal&0xFF; break;
	case DPO_UXTH: Val = RbVal&0xFFFF; break;
	case DPO_SBFM: case DPO_UBFM:
		RbVal = RbVal << (sizeof(RbVal) * 8 - Val2);
		Val2 = (Val2 >= (TUINT)Val ? sizeof(RbVal) * 8 + (TUINT)Val - Val2 : (TUINT)Val - Val2);
		Val = (DPO_VAL == DPO_SBFM ? (TSSIZE)RbVal >> Val2 : (TSSIZE)(RbVal >> Val2));
		break;
	default: goto _FAIL;
	}
	SET_BIT(pArmExArg->RegMask, Rd);
	pArmExArg->RegAry[Rd] = Val&dvMask;
	if (!TST_BIT(pArmExArg->rRegMask|pArmExArg->RegMask2, Rd)) {
		SET_BIT(pArmExArg->rRegMask, Rd);
		pArmExArg->pRegAry[Rd] = Val&dvMask;
	}
	return EX_NORMAL;
_FAIL:
	CLR_BIT(pArmExArg->RegMask, Rd);
	return EX_NORMAL;
}

/*************************************************************************
* DESCRIPTION
*	执行存取指令
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 无LSF_LD时可能为REG_ZR, 存在LSF_BLK则为寄存器位图!
*	Rd2: 目标寄存器, 可能为REG_ZR
*	Rb: 源寄存器
*	Rm: 第2寄存器, 可能为REG_ZR
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	EX_TYPE
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ExecLoadStore(TUINT Flag, TINT Rd, TINT Rd2, TINT Rb, TINT Rm, TINT Val, void *pArg)
{
	const TSIZE SrcMask = ((TSIZE)1 << Rb)|((TSIZE)1 << Rm);
	ARMEX_ARG * const pArmExArg = pArg;
	const TINT RwBytes = 1 << LSS_SZ;
	TADDR Addr, bAddr;
	TUINT i, dvBits;
	EX_TYPE ExType;
	TSSIZE tVal;

	if (LST_VAL == LST_PM)
		return EX_NORMAL;
	if ((pArmExArg->RegMask&SrcMask) != SrcMask) {
		ExType = EX_FAIL;
		goto _FAIL;
	}
	Addr = pArmExArg->RegAry[Rb];
#ifndef SST64
	if (Rb == REG_PC)
		Addr &= ~(TSIZE)3;
	if (Flag&LSF_BLK) {
		const TSSIZE StepSz = (Flag&LSF_BKD ? -RwBytes : RwBytes);
		TSIZE RegList = Rd;

		tVal = GetBitCnt(RegList) << RDS_SZ;
		bAddr = Addr + (Flag&LSF_BKD ? -tVal : tVal);
		for (ExType = EX_FAIL, i = 0; RegList; i++) {
			if (!TST_BIT(RegList, i))
				continue;
			CLR_BIT(RegList, i);
			if (ExType == EX_NORMAL || (Flag&LSF_BKB))
				Addr += StepSz;
			if ((ExType = LoadRegVal(Flag, RwBytes, Addr, i, pArmExArg)) != EX_NORMAL)
				goto _ERR;
		}
	} else
#endif
	{
		tVal = CalcRmVal(Flag, pArmExArg->RegAry[(Flag&(LSF_LD|LSF_EX)) == LSF_EX ? REG_ZR : Rm], VAL_VAL(TUINT, pArmExArg->Cpsr, 29, 1), Val);
		bAddr = Addr + (Flag&LSF_NEG ? -tVal : tVal);
		if (!(Flag&LSF_POST))
			Addr = bAddr;
		if ((ExType = LoadRegVal(Flag, RwBytes, Addr, Rd, pArmExArg)) != EX_NORMAL) {
			i = Rd;
			goto _ERR;
		}
		if (Rd2 != REG_ZR && (ExType = LoadRegVal(Flag, RwBytes, Addr + RwBytes, Rd2, pArmExArg)) != EX_NORMAL) {
			i = Rd2;
			goto _ERR;
		}
	}
	if (pArmExArg->pFaultAddr)
		*pArmExArg->pFaultAddr = Addr;
	if (Flag&(LSF_WB|LSF_POST))
		pArmExArg->RegAry[Rb] = bAddr;
	return EX_NORMAL;
_ERR:
	if (pArmExArg->pFaultAddr) {
		*pArmExArg->pFaultAddr = Addr;
		dvBits = 8 << RDS_SZ;
		*pArmExArg->pVal = pArmExArg->RegAry[i]&(dvBits < sizeof(TSIZE) * 8 ? ((TSIZE)1 << dvBits) - 1 : ~(TSIZE)0);
	}
_FAIL:
	if (Flag&RDS_VFP)
		tVal = 0;
	else
#ifndef SST64
	if (Flag&LSF_BLK)
		tVal = Rd;
	else
#endif
		tVal = (((TSIZE)1 << Rd)|((TSIZE)1 << Rd2))&~((TSIZE)1 << REG_ZR);
	if (Flag&(LSF_WB|LSF_POST))
		SET_BIT(tVal, Rb);
	pArmExArg->RegMask = pArmExArg->RegMask&~tVal;
	return ExType;
}

/*************************************************************************
* DESCRIPTION
*	执行杂项指令
*
* PARAMETERS
*	Flag: 标志
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	EX_TYPE
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ExecMisc(TUINT Flag, TUINT Val, void *pArg)
{
	(void)Flag, (void)Val, (void)pArg;
	return EX_NORMAL;
}

/*************************************************************************
* DESCRIPTION
*	推导未定义指令
*
* PARAMETERS
*	Inst: 指令
*	IsUnImpl: 是否是未完成解码的指令
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DeriArmUndef(TUINT Inst, int IsUnImpl, void *pArg)
{
	(void)Inst, (void)IsUnImpl, (void)pArg;
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	逆向/正向推导数据处理指令
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 有DPF_PSR时可能为REG_ZR
*	Rb: 源寄存器, 可能为REG_ZR
*	Val: 立即数, 有DPF_RM时为Rm寄存器
*	Val2: 第2立即数: 仅用于DPO_MVK/DPO_BTST/DPO_CCMN, 有DPF_RS时为Rs寄存器
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DeriSetReg(TUINT Flag, TINT Rd, TINT Rb, TSSIZE Val, TUINT Val2, void *pArg)
{
	static const char BypassOp[] = {0,0,0,0,0,0,0,0,0,0,0/*10*/,0,0,0,0,0,0,0,0,0,0/*20*/,1,1,0,0,0,0,1,1,1,1/*30*/,1,1};
	ARMEX_ARG * const pArmExArg = pArg;
	const TUINT dvBits = 8 << RDS_SZ;
	TSIZE dvMask, RdVal;
	TINT Rm = REG_ZR;

	if (Rd == REG_PC || Rd == REG_ZR || BypassOp[DPO_IDX])
		return 1;
	if (!TST_BIT(pArmExArg->RegMask, Rd)) {
		ExecSetReg(Flag, Rd, Rb, Val, Val2, pArg);
		CLR_BIT(pArmExArg->RegMask, Rd);
		goto _OUT;
	}
	CLR_BIT(pArmExArg->RegMask, Rd);
	if (Flag&DPF_RM) {
		Rm = (TINT)Val;
		Val = Val2;
		Val2 = 0;
	}
	if (Rm != REG_ZR && (Rb != REG_ZR || (Flag&DPF_RS) || Val || RMO_VAL >= RMO_RRX))
		goto _OUT;
	Rb = (Rb != REG_ZR ? Rb : Rm);
	if (TST_BIT(pArmExArg->RegMask, Rb))
		goto _OUT;
	dvMask = (dvBits < sizeof(TSIZE) * 8 ? ((TSIZE)1 << dvBits) - 1 : ~(TSIZE)0);
	RdVal = pArmExArg->RegAry[Rd]&dvMask;
	switch (DPO_VAL) {
	case DPO_ADD:
		if (Flag&DPF_CRY)
			goto _OUT;
		Val = RdVal + ((Flag&DPF_NEG) == DPF_NEG ? Val : -Val);
		break;
	case DPO_ORR:
		if (Val)
			goto _OUT;
		Val = (Flag&DPF_INV ? ~RdVal : RdVal);
		break;
	case DPO_EOR: Val = (Flag&DPF_INV ? ~RdVal : RdVal)^Val; break;
	case DPO_ROR: Val = (RdVal >> (dvBits - Val))|(RdVal << Val); break;
	default: goto _OUT;
	}
	SET_BIT(pArmExArg->RegMask, Rb);
	pArmExArg->RegAry[Rb] = Val&dvMask;
	if (!TST_BIT(pArmExArg->rRegMask|pArmExArg->RegMask2, Rb)) {
		SET_BIT(pArmExArg->rRegMask, Rb);
		pArmExArg->pRegAry[Rb] = pArmExArg->RegAry[Rb];
	}
_OUT:
	SET_BIT(pArmExArg->RegMask2, Rd);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	逆向/正向推导存取指令
*
* PARAMETERS
*	Flag: 标志
*	Rd: 目标寄存器, 无LSF_LD时可能为REG_ZR, 存在LSF_BLK则为寄存器位图!
*	Rd2: 目标寄存器, 可能为REG_ZR
*	Rb: 源寄存器
*	Rm: 第2寄存器, 可能为REG_ZR
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DeriLoadStore(TUINT Flag, TINT Rd, TINT Rd2, TINT Rb, TINT Rm, TINT Val, void *pArg)
{
	ARMEX_ARG * const pArmExArg = pArg;
	EX_TYPE ExType;
	TSIZE DstMask;

	if (PSR_VAL < PSR_AL)
		return 1;
	ExType = ExecLoadStore(Flag, Rd, Rd2, Rb, Rm, Val, pArg);
	if (LST_VAL != LST_PM && (Flag&(LSF_LD|RDS_VFP)) == LSF_LD) {
		DstMask = ((TSIZE)1 << Rd)|((TSIZE)1 << Rd2);
		pArmExArg->RegMask = (pArmExArg->RegMask&~DstMask)|((TSIZE)1 << REG_ZR)|((TSIZE)1 << REG_PC);
		if (ExType != EX_NORMAL && (Flag&(LSF_WB|LSF_POST)))
			SET_BIT(DstMask, Rb);
		pArmExArg->RegMask2 |= DstMask;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	推导杂项指令
*
* PARAMETERS
*	Flag: 标志
*	Val: 立即数
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DeriMisc(TUINT Flag, TUINT Val, void *pArg)
{
	(void)Flag, (void)Val, (void)pArg;
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	获取到异常点的代码路径
*
* PARAMETERS
*	pArmJmpArg: 参数
*		pArmExArg->pVal为跳转表, pArmExArg->OrgSp为跳转表长度
*		pArmExArg->rRegMask为InstShift
*	pInstDec: 指令解码函数
*	Start: 起始解析偏移量(字节)
*
* RETURNS
*	0: 找到异常点的代码路径
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int GetInstPath(ARMJMP_ARG *pArmJmpArg, int (*pInstDec)(TUINT Inst, int *pInstSz, DC_INSTFUN *pExeIFun, void *pArg), TSIZE Start)
{
	static DC_INSTFUN ExeInstF = {DeriArmUndef, JmpSetReg, JmpLoadStore, DeriMisc};
	int InstSz = 0;
	TSIZE Jmp;
	TUINT i;

	while (Start < pArmJmpArg->Size) {
		if (Start == pArmJmpArg->Pc)
			return 0;
		if (pInstDec(OFFVAL_U4(pArmJmpArg->pInst, Start), &InstSz, &ExeInstF, pArmJmpArg)) {
			Start += InstSz;
			continue;
		}
		Jmp = Start + pArmJmpArg->Off;
		if (Jmp >= pArmJmpArg->Size || (pArmJmpArg->pBitMap[Start >> pArmJmpArg->InstShift]&0x1)) /* 无效或已走过 */
			return 1;
		for (i = 0; i < pArmJmpArg->TblCnt; i++) { /* 记录之前搜索是否存在循环 */
			if (pArmJmpArg->pJmpTbl[i][0] == (TUINT)Start)
				return 1;
		}
		pArmJmpArg->pJmpTbl[i][0] = (TUINT)Start;
		pArmJmpArg->pJmpTbl[i][1] = (TUINT)Jmp;
		pArmJmpArg->TblCnt++;
		if (pArmJmpArg->IsAlJmp) { /* 绝对跳转 */
			if (Start == Jmp) /* 死循环 */
				return 1;
			pArmJmpArg->pJmpTbl[i][1] = (TUINT)Jmp|1/* 标记有效 */;
			Start = Jmp;
			continue;
		}
		if (!GetInstPath(pArmJmpArg, pInstDec, Start + InstSz))
			return 0;
		pArmJmpArg->TblCnt = i + 1; /* reset */
		pArmJmpArg->pJmpTbl[i][1] = (TUINT)Jmp|1/* 标记有效 */;
		pArmJmpArg->pBitMap[Start >> pArmJmpArg->InstShift] |= 0x1; /* 标记无效避免再走过 */
		Start = Jmp;
	}
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	填充跳转信息位图
*
* PARAMETERS
*	pBitMap: 需要显示指令的位图(bit0: 指令是否无效)
*	pInst/Size: 指令buffer和大小(字节)
*	FAddr: 函数起始地址, 包含THUMB bit
*	Pc: 异常PC地址
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FillJmpBitMap(TUINT *pBitMap, const void *pInst, TSIZE Size, TADDR FAddr, TADDR Pc)
{
	int (*pInstDec)(TUINT Inst, int *pInstSz, DC_INSTFUN *pExeIFun, void *pArg);
	ARMJMP_ARG ArmJmpArg;
	TUINT i;

	ArmJmpArg.Pc = Pc - FAddr;
	ArmJmpArg.pInst = (void *)pInst;
	ArmJmpArg.Size = Size;
	ArmJmpArg.pBitMap = pBitMap;
	ArmJmpArg.TblCnt = 0;
#if !defined(SST64) && defined(SUPPORT_THUMB)
	if (THUMB_BIT(FAddr)) { /* thumb code */
		pInstDec = ThumbInstDec;
		ArmJmpArg.InstShift = 1;
		ArmJmpArg.PcOff = 4;
		ArmJmpArg.pJmpTbl = Malloc((size_t)Size << 2);
	} else
#endif
	{
		pInstDec = ArmInstDec;
		ArmJmpArg.InstShift = 2;
#ifdef SST64
		ArmJmpArg.PcOff = 0;
#else
		ArmJmpArg.PcOff = 8;
#endif
		ArmJmpArg.pJmpTbl = Malloc((size_t)Size << 1);
	}
	if (!ArmJmpArg.pJmpTbl)
		return;
	if (!GetInstPath(&ArmJmpArg, pInstDec, 0)) {
		for (i = 0; i < ArmJmpArg.TblCnt; i++) { /* 标记bitmap */
			if (ArmJmpArg.pJmpTbl[i][1]&1) {
				//DBGERR(TADDRFMT" -> "TADDRFMT"\n", FUNC_ADDR(FAddr) + ArmJmpArg.pJmpTbl[i][0], FUNC_ADDR(FAddr) + (ArmJmpArg.pJmpTbl[i][1]&~1));
				pBitMap[ArmJmpArg.pJmpTbl[i][1] >> ArmJmpArg.InstShift] = ArmJmpArg.pJmpTbl[i][0] << 2;
			}
		}
	} else {
		DBGERR("查找路径失败\n");
	}
	Free(ArmJmpArg.pJmpTbl);
}

/*************************************************************************
* DESCRIPTION
*	填充源寄存器树信息位图
*
* PARAMETERS
*	pBitMap: 需要显示指令的位图(bit0: 指令是否无效, bit2~: 跳转)
*	pInst: 指令buffer
*	FAddr: 函数起始地址, 包含THUMB bit
*	Pc: 异常PC地址
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FillRegBitMap(TUINT *pBitMap, const void *pInst, TADDR FAddr, TADDR Pc)
{
	static DC_INSTFUN ExeInstF = {DeriArmUndef, RecSetReg, RecLoadStore, DeriMisc};
	int (*pInstDec)(TUINT Inst, int *pInstSz, DC_INSTFUN *pExeIFun, void *pArg);
	TSIZE i, ExcpIdx, FuncIdx;
	ARMMASK_ARG ArmMaskArg;
	size_t InstShift;
	int InstSz;
	TUINT Jmp;

	ArmMaskArg.RegMask = 0;
	ArmMaskArg.IsFirst = 1;
#if !defined(SST64) && defined(SUPPORT_THUMB)
	if (THUMB_BIT(FAddr)) { /* thumb code */
		pInstDec = ThumbInstDec;
		InstShift = 1;
	} else
#endif
	{
		pInstDec = ArmInstDec;
		InstShift = 2;
	}
	ExcpIdx = FuncIdx = i = (Pc - FAddr) >> InstShift;
	while (1) {
		Jmp = pBitMap[i] >> 2;
		pBitMap[i] = 0; /* 清除 */
		ArmMaskArg.pBitInf = &pBitMap[i];
		if (!pInstDec(OFFVAL_U4(pInst, i << InstShift), &InstSz, &ExeInstF, &ArmMaskArg)) {
			if (i < FuncIdx)
				FuncIdx = i;
		}
		ArmMaskArg.IsFirst = 0;
		ArmMaskArg.RegMask &= ~(((TSIZE)1 << REG_PC)|((TSIZE)1 << REG_ZR)); /* 不追这些寄存器 */
		if (!(i && ArmMaskArg.RegMask))
			break;
		if (Jmp) { /* 逆跳转 */
			if (!pBitMap[i])
				pBitMap[i] = 0x1A;
			do {
				i = Jmp >> InstShift;
				Jmp = pBitMap[i] >> 2;
				pBitMap[i] = 0x2;
				if (i < FuncIdx)
					FuncIdx = i;
			} while (Jmp);
			if (!i)
				break;
		}
		do { i--; } while (pBitMap[i]&0x1); /* 上一条有效指令 */
	}
	ArmMaskArg.RegMask &= (((TSIZE)1 << REG_PNUM) - 1);
	if (ArmMaskArg.RegMask) /* 标记函数头 */
		pBitMap[FuncIdx] = 0x6|((TUINT)ArmMaskArg.RegMask << 5);
	if (!pBitMap[ExcpIdx] || pBitMap[ExcpIdx] == 0x2) /* 标记异常点 */
		pBitMap[ExcpIdx] = 0x16;
}

/*************************************************************************
* DESCRIPTION
*	显示异常相关函数参数信息
*
* PARAMETERS
*	fp: 文件句柄
*	FAddr: 函数起始地址, 包含THUMB bit
*	pFName: 函数名, 可以为NULL
*	SrcMask: 异常相关参数位图
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ShowExcpFuncInf(FILE *fp, TADDR FAddr, const char *pFName, TUINT SrcMask)
{
	int i;
	
	if (!SrcMask)
		return;
	if (pFName && pFName != pUndFuncName)
		LOGC(fp, "\t; %s", pFName);
	else
		LOGC(fp, "\t; "TADDRFMT, FAddr);
	LOGC(fp, "()%s", GetCStr(RS_PARA));
	for (i = 0; SrcMask; i++) {
		if (!(SrcMask&(1 << i)))
			continue;
		SrcMask &= ~(1 << i);
		LOGC(fp, " %d", i + 1);
	}
	LOG(fp, RS_HAS_PROBLEM);
}

/*************************************************************************
* DESCRIPTION
*	根据位图显示反汇编指令
*
* PARAMETERS
*	fp: 文件句柄
*	pSymHdl: sym句柄
*	pBitMap: 需要显示指令的位图
*		bit0: 指令是否无效
*		bit1: 是否显示该指令
*		bit2-4: 0: 正常, 1: 函数头, 2: 全局变量, 3: 局部变量, 4: 函数返回值, 5: 异常点, 6: 仅显示地址
*	pInst/Size: 指令buffer和大小(字节)
*	FAddr: 函数起始地址, 包含THUMB bit
*	pFName: 函数名, 可以为NULL
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispFuncAsmEx(FILE *fp, SYM_HDL *pSymHdl, const TUINT *pBitMap, const void *pInst, TSIZE Size, TADDR FAddr, const char *pFName)
{
	static DC_INSTFUN ExeInstF = {DispArmUndef, DispSetReg, DispLoadStore, DispMisc};
	int (*pInstDec)(TUINT Inst, int *pInstSz, DC_INSTFUN *pExeIFun, void *pArg);
	size_t InstShift, PcOff = 0;
	int IsFirst, InstSz = 0;
	ARMEX_ARG ArmExArg;
	char *pFnBk = NULL;
	TUINT BitInf;
	TSIZE i;

	memset(&ArmExArg, 0, sizeof(ArmExArg));
	ArmExArg.RegMask = ((TSIZE)1 << REG_PC)|((TSIZE)1 << REG_ZR);
	ArmExArg.RegMask2 = ArmExArg.rRegMask = ((TSIZE)1 << ARY_CNT(ArmExArg.RegAry)) - 1;
	ArmExArg.fp = fp;
#ifdef SST64
	LOG(fp, RS_DISAS_INFO, 15, ' ');
#else
	LOG(fp, RS_DISAS_INFO, 7, ' ');
#endif
#if !defined(SST64) && defined(SUPPORT_THUMB)
	if (THUMB_BIT(FAddr)) { /* thumb code */
		FAddr = FUNC_ADDR(FAddr);
		pInstDec = ThumbInstDec;
		PcOff = 4;
		InstShift = 1;
	} else
#endif
	{
		pInstDec = ArmInstDec;
		InstShift = 2;
#ifndef SST64
		PcOff = 8;
#endif
	}
	for (IsFirst = 1, i = 0; i < Size; ) {
		/* 过滤 */
		BitInf = pBitMap[i >> InstShift];
		if (!(BitInf&0x2)) {
			i += ((TSIZE)1 << InstShift);
			continue;
		}
		LOGC(fp, "\n");
		/* 反汇编 */
		pFnBk = DispPreAsmInf(fp, pSymHdl, FAddr + i, &IsFirst);
		if (BitInf == 0x1A) { /* 跳过该指令 */
			i += ((TSIZE)1 << InstShift);
			continue;
		}
		ArmExArg.RegAry[REG_PC] = FAddr + i + PcOff;
		SET_BIT(ArmExArg.RegMask, REG_PC);
		pInstDec(OFFVAL_U4(pInst, i), &InstSz, &ExeInstF, &ArmExArg);
		/* 附加信息 */
		switch (BitInf&0x1F) {
		case 0x06: ShowExcpFuncInf(fp, FAddr, pFName, BitInf >> 5); break;
		case 0x0A: LOG(fp, RS_EXCP_GVAR); LOG(fp, RS_HAS_PROBLEM); break;
		case 0x0E: LOG(fp, RS_EXCP_LVAR); LOG(fp, RS_HAS_PROBLEM); break;
		case 0x12: LOG(fp, RS_EXCP_FUNC); LOG(fp, RS_HAS_PROBLEM); break;
		case 0x16: LOG(fp, RS_EXCP_HERE); break;
		default:;
		}
		i += InstSz;
	}
	Free(pFnBk);
}

/*************************************************************************
* DESCRIPTION
*	显示寄存器内容
*
* PARAMETERS
*	fp: 文件句柄
*	pRegAry/RegMask: 寄存器组/寄存器有效位图
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispRegCntx(FILE *fp, const TSIZE *pRegAry, TSIZE RegMask)
{
#ifdef SST64
#define REG_LCNT  4
#else
#define REG_LCNT  8
#endif
	size_t i, j;

	if (!RegMask)
		return;
	LOG(fp, RS_EXCP_REG);
	for (i = j = 0; i < REG_NUM; i++) {
		if (!TST_BIT(RegMask, i))
			continue;
		if (j&(REG_LCNT - 1))
			LOGC(fp, ",%*c", i <= 10 ? 2 : 1, ' ');
		else if (j)
			LOGC(fp, "\n\t");
		j++;
		DispRegName(fp, "", REG_DEFB, i);
		LOGC(fp, ": "TADDRFMT2, pRegAry[i]);
	}
	LOGC(fp, "\n");
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	显示函数的异常相关反汇编指令
*
* PARAMETERS
*	fp: 文件句柄
*	FAddr/Size: 函数起始地址
*	pRegAry/RegMask: 寄存器组/寄存器有效位图
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DispFuncAsm(FILE *fp, TADDR FAddr, const TSIZE *pRegAry, TSIZE RegMask)
{
	VMA_HDL * const pVmaHdl = GetVmaHdl(pRegAry[REG_PC]);
	SYM_HDL *pSymHdl;
	const char *pName;
	const void *pInst;
	TUINT *pBitMap;
	TSIZE Size;

	if (!pVmaHdl)
		return;
	LOG(fp, RS_DISAS);
	pSymHdl = (VMA_TYPE(pVmaHdl) == VMA_ELF ? pVmaHdl->pHdl : NULL);
	if (!pSymHdl)
		goto _NEXT;
	pName = SymAddr2Name(pSymHdl, &FAddr, pRegAry[REG_PC], TALLFUN);
	if (!pName || pName == pUndFuncName || SymName2Addr(pSymHdl, &Size, pName, TALLFUN) != FAddr)
		Size = pRegAry[REG_PC] - FAddr + 4/* 当前异常指令 */;
	if ((pInst = Vma2Mem(pVmaHdl, FUNC_ADDR(FAddr), Size)) == NULL)
		goto _NEXT;
#if !defined(SST64) && defined(SUPPORT_THUMB)
	if (THUMB_BIT(FAddr)) /* thumb code */
		pBitMap = GetT32InstMap(pInst, (size_t)Size);
	else
#endif
		pBitMap = Calloc((size_t)Size);
	if (!pBitMap)
		goto _NEXT;
	FillJmpBitMap(pBitMap, pInst, Size, FAddr, pRegAry[REG_PC]);
	FillRegBitMap(pBitMap, pInst, FAddr, pRegAry[REG_PC]);
	DispFuncAsmEx(fp, pSymHdl, pBitMap, pInst, Size, FAddr, pName);
	Free(pBitMap);
_NEXT:
	PutVmaHdl(pVmaHdl);
	DispRegCntx(fp, pRegAry, RegMask);
}

/*************************************************************************
* DESCRIPTION
*	获取异常信息
*
* PARAMETERS
*	pVal: 返回存取的数据或指令(为EX_NORMAL时)
*	pFaultAddr: 返回异常地址
*	pRegAry/Cpsr: 异常时刻的寄存器信息
*
* RETURNS
*	异常类型
*
* LOCAL AFFECTED
*
*************************************************************************/
EX_TYPE GetExInf(TADDR *pFaultAddr, TSIZE *pVal, const TSIZE *pRegAry, TSIZE Cpsr)
{
	static DC_INSTFUN ExeInstF = {ExecArmUndef, ExecSetReg, ExecLoadStore, ExecMisc};
	ARMEX_ARG ArmExArg;
	const void *pInst;
	VMA_HDL *pVmaHdl;
	int InstSz;
	
	*pFaultAddr = FUNC_ADDR(pRegAry[REG_PC]);
#if !defined(SST64) && defined(SUPPORT_THUMB)
	if (!THUMB_BIT(pRegAry[REG_PC]))
#endif
		if (*pFaultAddr&3) /* A32/A64需要4字节对齐 */
			return EX_INST_EA;
	pVmaHdl = GetVmaHdl(*pFaultAddr);
	if (!pVmaHdl)
		return EX_INST_EA;
	if (!(pVmaHdl->Flag&VMA_X)) {
		PutVmaHdl(pVmaHdl);
		return EX_INST_EA;
	}
	pInst = Vma2Mem(pVmaHdl, *pFaultAddr, 4);
	if (!pInst) {
		PutVmaHdl(pVmaHdl);
		return EX_FAIL;
	}
	*pVal = A64CODE(0);
	PutVmaHdl(pVmaHdl);
	memset(&ArmExArg, 0, sizeof(ArmExArg));
	memcpy(ArmExArg.RegAry, pRegAry, REG_NUM * sizeof(pRegAry[0]));
	ArmExArg.Cpsr = Cpsr;
	ArmExArg.pFaultAddr = pFaultAddr;
	ArmExArg.pVal = pVal;
	ArmExArg.RegMask2 = ArmExArg.rRegMask = ArmExArg.RegMask = ((TSIZE)1 << ARY_CNT(ArmExArg.RegAry)) - 1;
#if !defined(SST64) && defined(SUPPORT_THUMB)
	if (THUMB_BIT(pRegAry[REG_PC])) {
		ArmExArg.RegAry[REG_PC] = *pFaultAddr + 4; /* PC + 4 */
		return ThumbInstDec(OFFVAL_U4(pVal, 0), &InstSz, &ExeInstF, &ArmExArg);
	}
#endif
#ifndef SST64
	ArmExArg.RegAry[REG_PC] += 8; /* PC + 8 */
#endif
	return ArmInstDec(OFFVAL_U4(pVal, 0), &InstSz, &ExeInstF, &ArmExArg);
}

/*************************************************************************
* DESCRIPTION
*	寄存器反向推导，用于推导函数参数
*
* PARAMETERS
*	FAddr: 函数地址
*	pRegAry: 寄存器信息，会被更新
*	RegMask: 寄存器有效位图
*	TarMask: 预期达到的有效位图
*
* RETURNS
*	更新后的有效位图
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE RegDerivation(TADDR FAddr, TSIZE *pRegAry, TSIZE RegMask, TSIZE TarMask)
{
	static DC_INSTFUN ExeInstF = {DeriArmUndef, DeriSetReg, DeriLoadStore, DeriMisc};
	TINT Cnt = (TINT)(pRegAry[REG_PC] - FAddr);
	ARMEX_ARG ArmExArg;
	const void *pInst;
	VMA_HDL *pVmaHdl;
	int InstSz;

	if (Cnt <= 0 || (pVmaHdl = GetVmaHdl(FAddr)) == NULL)
		return RegMask;
	memset(&ArmExArg, 0, sizeof(ArmExArg));
	memcpy(ArmExArg.RegAry, pRegAry, REG_NUM * sizeof(pRegAry[0]));
	ArmExArg.RegMask2 = ~TarMask;
	ArmExArg.rRegMask = ArmExArg.RegMask = RegMask|((TSIZE)1 << REG_ZR);
	ArmExArg.pRegAry = pRegAry;
	FAddr = FUNC_ADDR(FAddr);
	pInst = Vma2Mem(pVmaHdl, FAddr, Cnt);
	if (!pInst)
		goto _OUT;
#if !defined(SST64) && defined(SUPPORT_THUMB)
	if (THUMB_BIT(pRegAry[REG_PC])) {
		TUINT *pIMap;
		
		pIMap = GetT32InstMap(pInst, (size_t)Cnt + 4/* 当前异常指令 */);
		if (!pIMap)
			goto _OUT;
		FAddr += 4; /* PC + 4 */
		do {
			Cnt -= (2 << pIMap[(Cnt >> 1) - 1]);
			ArmExArg.RegAry[REG_PC] = FAddr + Cnt;
			SET_BIT(ArmExArg.RegMask, REG_PC);
			if (ThumbInstDec(T32CODE(Cnt), &InstSz, &ExeInstF, &ArmExArg) || ((ArmExArg.rRegMask|ArmExArg.RegMask2)&TarMask) == TarMask)
				break;
		} while (Cnt);
		Free(pIMap);
	} else
#endif
	{
#ifndef SST64
		FAddr += 8; /* PC + 8 */
#endif
		do {
			Cnt -= 4;
			ArmExArg.RegAry[REG_PC] = FAddr + Cnt;
			SET_BIT(ArmExArg.RegMask, REG_PC);
		} while (Cnt >= 0 && !ArmInstDec(A32CODE(Cnt), &InstSz, &ExeInstF, &ArmExArg)
		  && ((ArmExArg.rRegMask|ArmExArg.RegMask2)&TarMask) != TarMask);
	}
_OUT:
#ifdef _DEBUG
	if (!(ArmExArg.RegMask&((TSIZE)1 << REG_ZR)) || ArmExArg.RegAry[REG_ZR])
		DBGERR("REG_ZR位消失\n");
#endif
	PutVmaHdl(pVmaHdl);
	return ArmExArg.rRegMask;
}
