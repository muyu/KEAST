/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	ttypes.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/9/17
* Description: 目标相关类型信息
**********************************************************************************/
#ifndef _TTYPES_H
#define _TTYPES_H

#define MACRO2STR1(Str) #Str
#define MACRO2STR(Str)  MACRO2STR1(Str)

#ifdef SST64
#define SST_VERSION		1
#define SST_VERSION_SUB	6
#define SST_NAME		"KSST64"

#define ADDR_BITS		39

#define TLONGFMT		"ll"
#define LCNTP4BITS		16 /* * 4 = 64 */
typedef unsigned long long TADDR;
typedef unsigned long long TSIZE;
typedef long long		TSSIZE;

#define REG_FP			29
#define REG_SP			31 /* 对应dwarf, 禁止改动!!! */
#define REG_LR			30
#define REG_PC			32 /* 对应dwarf, 禁止改动!!! */
#define REG_NUM			33
#define REG_PNUM		8 /* ABI寄存器传递参数个数 */
#define TMPREG_MASK		(((TSIZE)1 << REG_LR)|(((TSIZE)1 << 18) - 1))
#else
#define SST_VERSION		1
#define SST_VERSION_SUB	9
#define SST_NAME		"KSST"

#define ADDR_BITS		32

#define TLONGFMT		""
#define LCNTP4BITS		8 /* * 4 = 32 */
typedef unsigned		TADDR;
typedef unsigned		TSIZE;
typedef int				TSSIZE;

#define REG_FP			11
#define REG_SP			13
#define REG_LR			14
#define REG_PC			15
#define REG_NUM			16
#define REG_PNUM		4
#define TMPREG_MASK		(((TSIZE)1 << REG_LR)|(((TSIZE)1 << 4) - 1)|((TSIZE)1 << 12))
#endif

#define REG_BITS		(LCNTP4BITS * 4)
typedef int				TINT;
typedef unsigned		TUINT;

#define TADDRFMT		"0x%0"MACRO2STR(LCNTP4BITS) TLONGFMT"X"
#define TADDRFMT2		"%0"MACRO2STR(LCNTP4BITS) TLONGFMT"X"
#define TSIZEFMT		"%"TLONGFMT"d"
#define TSIZEFMT2		"0x%"TLONGFMT"X"

#define PAGE_BITS		12
#define PAGE_SIZE		((TSIZE)1 << PAGE_BITS)
#define PAGE_MASK		(~(PAGE_SIZE - 1))

#define MAX_CORE		16 /* 最多支持32 */

#endif
