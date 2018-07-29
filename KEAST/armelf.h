/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	armelf.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: arm elf相关定义
**********************************************************************************/
#ifndef _ARMELF_H
#define _ARMELF_H

#include <ttypes.h>

typedef void * ELF_HDL;

typedef const struct _ELF_SYM
{
	TUINT st_name;
#ifndef SST64
	TADDR st_value;
	TSIZE st_size;
#endif

#define STB_LOCAL	0
#define STB_GLOBAL	1
#define STB_WEAK	2
#define ELF_ST_BIND(x) ((x) >> 4) /* symbol binding */
#define STT_NOTYPE	0
#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_ARM_TFUNC	13 /* A Thumb function */
#define STT_ARM_16BIT	15 /* A Thumb label */
#define ELF_ST_TYPE(x)	((x)&0xF)  /* symbol type */
#define ELF_ST_INFO(b, t) (((b) << 4) + ((t)&0xF)) /* symbol info */
	unsigned char st_info;
	unsigned char st_other;

#define SHN_UNDEF	0
	unsigned short st_shndx;
#ifdef SST64
	TADDR st_value;
	TSIZE st_size;
#endif
} ELF_SYM;

typedef const struct _ELF_REL
{
	TADDR r_offset;

#ifdef SST64
#define ELF_R_TYPE(x)	((x)&0xFFFFFFFF)
#define ELF_R_SYM(x)	((x) >> 32)
#else
#define R_ARM_ABS32		2  /* Direct 32 bit  */
#define R_ARM_RELATIVE	23 /* Adjust by program base */
#define ELF_R_TYPE(x)	((x)&0xFF)
#define ELF_R_SYM(x)	((x) >> 8)
#endif
	TSIZE r_info;
} ELF_REL;

typedef const struct _ELF_RELA
{
	TADDR r_offset;
	TSIZE r_info;
	TSSIZE r_addend;
} ELF_RELA;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取elf句柄中指定的类型和名称的节信息
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pAddr: 返回虚拟地址
*	pFOff: 返回文件偏移量(字节)
*	Type: SHT_*标志
*	pName: 节名称, NULL表示不考虑匹配
*
* RETURNS
*	0: 失败
*	其他: 节大小(字节)
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define SHT_PROGBITS	1 /* Program specific (private) data */
#define SHT_SYMTAB		2 /* Link editing symbol table */
#define SHT_STRTAB		3 /* A string table */
#define SHT_RELA		4 /* Relocation entries with addends */
#define SHT_HASH		5 /* A symbol hash table */
#define SHT_DYNAMIC		6 /* Information for dynamic linking */
#define SHT_NOTE		7 /* Information that marks file */
#define SHT_NOBITS		8 /* Section occupies no space in file */
#define SHT_REL			9 /* Relocation entries, no addends */
#define SHT_SHLIB		10 /* Reserved, unspecified semantics */
#define SHT_DYNSYM		11 /* Dynamic linking symbol table */
#define SHT_INIT_ARRAY	14 /* Array of ptrs to init functions */
#define SHT_FINI_ARRAY	15 /* Array of ptrs to finish functions */
#define SHT_PREINIT_ARRAY 16 /* Array of ptrs to pre-init funcs */
#define SHT_GROUP		17 /* Section contains a section group */
#define SHT_SYMTAB_SHNDX 18 /* Indicies for SHN_XINDEX entries */
#define SHT_ARM_EXIDX	0x70000001 /* Section holds ARM unwind info */
TSIZE ElfGetSecInf(ELF_HDL *pElfHdl, TADDR *pAddr, TSIZE *pFOff, TUINT Type, const char *pName);

/*************************************************************************
* DESCRIPTION
*	获取elf句柄中符号表信息
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pSymFOff: 符号表文件偏移量(字节)
*	pStrsFOff: 字符串表文件偏移量(字节)
*	pStrsSz: 字符串表大小(字节)
*	pGlbIdx: 全局符号索引
*
* RETURNS
*	0: 失败
*	其他: 符号表大小(字节)
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE ElfGetSymInf(ELF_HDL *pElfHdl, TSIZE *pSymFOff, TSIZE *pStrsFOff, TSIZE *pStrsSz, TUINT *pGlbIdx);

/*************************************************************************
* DESCRIPTION
*	获取elf句柄中指定的类型的段信息
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pAddr: 返回虚拟地址
*	pFOff: 返回文件偏移量(字节)
*	Type: PT_*标志
*	Flag: PF_*标志
*
* RETURNS
*	0: 失败
*	其他: 段大小(字节)
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define PT_LOAD			1 /* Loadable program segment */
#define PT_DYNAMIC		2 /* Dynamic linking information */
#define PT_NOTE			4 /* Auxiliary information */
#define PT_PHDR			6 /* Entry for header table itself */
#define PT_ARM_EXIDX	0x70000001 /* Frame unwind information */
#define PT_GNU_EH_FRAME	0x6474E550 /* GCC .eh_frame_hdr segment */
#define PT_GNU_STACK	0x6474E551 /* Indicates stack executability */
#define PT_GNU_RELRO	0x6474E552 /* Read-only after relocation */
TSIZE ElfGetSegInf(ELF_HDL *pElfHdl, TADDR *pAddr, TSIZE *pFOff, TUINT Type, TUINT Flag);

/*************************************************************************
* DESCRIPTION
*	遍历elf句柄的可加载sec节
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pFunc: 回调函数
*		pSecName: 节名称
*		Flag: PF_*标志
*		FOff: 文件偏移量(字节)
*		Size: 占用文件大小(字节)
*		Addr: 虚拟地址
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define PF_X	(1 << 0) /* 段可执行 */
#define PF_W	(1 << 1) /* 段可写 */
#define PF_R	(1 << 2) /* 段可读 */
int ElfForEachSec(ELF_HDL *pElfHdl
	, int (*pFunc)(const char *pSecName, TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Addr, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	遍历elf句柄的load段
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pFunc: 回调函数
*		Flag: PF_*标志
*		FOff: 文件偏移量(字节)
*		Size: 占用文件大小(字节)
*		Va/Pa: 虚拟/物理地址
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ElfForEachLoad(ELF_HDL *pElfHdl
	, int (*pFunc)(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	遍历elf句柄的note段
*	注意: ELF文件类型为ET_CORE时才有note段!!!
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pFunc: 回调函数
*		Type: NT_*标志
*		pName: 名称, 离开该函数则地址无效!!!
*		pBuf: buffer, 离开该函数则地址无效!!!
*		Size: buffer大小(字节)
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define NT_PRSTATUS	1
#define NT_PRPSINFO	3
#define NT_MRINFO	0x0000AEE0		//MRDUMP01
#define NT_CTRLBLK	0x0000BEEF     //for android o load,MRDUMP08
#define NT_IPANIC_MISC 0x00000FFF
int ElfForEachNote(ELF_HDL *pElfHdl
	, int (*pFunc)(TUINT Type, const char *pName, const void *pBuf, TUINT Size, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	记录一个符号信息，只能用于CreateSymFile()的回调路径上!!!
*
* PARAMETERS
*	pName: 名称
*	Val: 值
*	Size: 大小
*	Info: 其他信息
*	pArg: 额外的参数
*
* RETURNS
*	0: 继续
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int BuildOneSym(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg);

/*************************************************************************
* DESCRIPTION
*	生成符号文件
*
* PARAMETERS
*	pFile: 符号文件路径
*	BuildSyms: 提供sym的回调函数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int CreateSymFile(const char *pFile, int (*BuildSyms)(void *pArg), void *pArg);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据fp句柄获取elf句柄
*
* PARAMETERS
*	pFpHdl: fp句柄
*	Type: 类型
*
* RETURNS
*	NULL: 失败
*	其他: elf句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define ET_REL	(1 << 1) /* 重定位ELF */
#define ET_EXEC	(1 << 2) /* 可执行ELF */
#define ET_DYN	(1 << 3) /* 共享ELF */
#define ET_CORE	(1 << 4) /* 内存ELF */
ELF_HDL *GetElfHdl(const void *pFpHdl, TUINT Type);

/*************************************************************************
* DESCRIPTION
*	释放elf句柄
*
* PARAMETERS
*	pElfHdl: elf句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutElfHdl(ELF_HDL *pElfHdl);

#endif
