/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	armelf.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/10/2
* Description: armelf解析
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <armelf.h>
#if defined(__MINGW32__)
#include <stdint.h>   /*For define uintptr_t*/
#endif

typedef const struct _ELF_HDR
{
#define EI_NIDENT	16
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_OSABI	7
#define ELFMAG		"\177ELF"
#define SELFMAG		4
#define ELFCLASS32	1 /* 32bit */
#define ELFCLASS64	2 /* 64bit */
#define ELFCLASSNUM	3
#ifdef SST64
#define ELF_CLASS	ELFCLASS64
#else
#define ELF_CLASS	ELFCLASS32
#endif
#define ELFDATA2LSB	1 /* little endian */
#define ELFDATA2MSB	2 /* big endian */
#define ELF_DATA	ELFDATA2LSB
#define ELFOSABI_NONE 0 /* UNIX System V ABI */
	unsigned char e_ident[EI_NIDENT];
	unsigned short e_type;

#define EM_ARM		40 /* ARM */
#define EM_AARCH64	183
#ifdef SST64
#define EM_MACH		EM_AARCH64
#else
#define EM_MACH		EM_ARM
#endif
	unsigned short e_machine;

#define EV_NONE		0 /* Invalid version */
#define EV_CURRENT	1 /* Current version */
#define EV_NUM		2
	TUINT e_version;
	TADDR e_entry;
	TSIZE e_phoff;
	TSIZE e_shoff;
	TUINT e_flags;
	unsigned short e_ehsize;
	unsigned short e_phentsize;

#define PN_XNUM 0xFFFF
	unsigned short e_phnum;
	unsigned short e_shentsize;
	unsigned short e_shnum;
	unsigned short e_shstrndx;
} ELF_HDR;

typedef const struct _ELF_SHDR
{
	TUINT sh_name;
	TUINT sh_type;

#define SHF_WRITE	0x1 /* Writable data during execution */
#define SHF_ALLOC	0x2 /* Occupies memory during execution */
#define SHF_EXECINSTR 0x4 /* Executable machine instructions */
	TSIZE sh_flags;
	TADDR sh_addr;   /* address at memory */
	TSIZE sh_offset; /* offset of section at ELF file */
	TSIZE sh_size;   /* size of section */

/* sh_type	   sh_link					  sh_info */
/* ======		=====						===== */
/* SHT_DYNAMIC   The section header index of  0 */
/*			   the string table used by */
/*			   entries in the section. */
/* SHT_HASH	  The section header index of  0 */
/*			   the symbol table to which the */
/*			   hash table applies. */
/* SHT_REL/	  The section header index of  The section header index of */
/* SHT_RELA	  the associated symbol table. the section to which the relocation applies. */
/* SHT_SYMTAB/   The section header index of  One greater than the symbol */
/* SHT_DYNSYM	the associated string table. table index of the last local symbol (binding STB_LOCAL). */
/* other		 SHN_UNDEF					0 */
	TUINT sh_link;
	TUINT sh_info; /* additional info, depend on section type */
	TSIZE sh_addralign;
	TSIZE sh_entsize; /* section entry size */
} ELF_SHDR;

typedef const struct _ELF_PHDR
{
	TUINT p_type; /* PT_*标志 */
#ifdef SST64
	TUINT p_flags;
#endif
	TSIZE p_offset;
	TADDR p_vaddr;
	TADDR p_paddr;
	TSIZE p_filesz;
	TSIZE p_memsz;
#ifndef SST64
	TUINT p_flags; /* PF_*标志 */
#endif
	TSIZE p_align;
} ELF_PHDR;

typedef struct _ELF_INT
{
#define ELF_INVALID_IDX (~(size_t)0)
	FP_HDL *pFpHdl;
	ELF_SHDR *pShdrs;
	ELF_PHDR *pPhdrs;
	size_t ShCnt;
	size_t PhCnt, OPhCnt/* 原始段数 */;
	size_t SymIdx; /* 节索引 */
	const char *pShStrs;
	TSIZE ShStrsSz;
} ELF_INT;


typedef enum _CMM_SHDR_TYPE
{
#define CMM_SHDR_NONE_N "\0"
	CMM_SHDR_NONE,

#define CMM_SHDR_SHSTR_N ".shstrtab\0"
	CMM_SHDR_SHSTR,

#define CMM_SHDR_SYMTAB_N ".symtab\0"
	CMM_SHDR_SYMTAB,

#define CMM_SHDR_SYMSTR_N ".strtab\0"
	CMM_SHDR_SYMSTR,

#define CMM_SHSTR_N CMM_SHDR_NONE_N CMM_SHDR_SHSTR_N CMM_SHDR_SYMTAB_N CMM_SHDR_SYMSTR_N "\0\0"/* 用于对齐 */
	CMM_SHDR_MAX,
} CMM_SHDR_TYPE;

static struct
{
	struct _ELF_HDR Hdr;
	struct _ELF_SHDR Shdrs[CMM_SHDR_MAX];
	unsigned ShdrsOff;

	unsigned SymsOff;
	struct _ELF_SYM *pSyms;
	unsigned SymCnt, SymMax;

	unsigned SymStrsOff;
	char *pSymStrs;
	unsigned SymStrsSz, SymStrsMax;

	unsigned ShStrsOff;
} *pElfSym;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取符号节索引
*
* PARAMETERS
*	pShdr: 节结构信息
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
*	2: 已找到
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindSymIdx(ELF_SHDR *pShdr, void *pArg)
{
	ELF_INT * const pElf = pArg;

	if (pShdr->sh_type == SHT_SYMTAB) {
		pElf->SymIdx = pShdr - pElf->pShdrs;
		return 2;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	获取指定类型节信息
*
* PARAMETERS
*	pShdr: 节结构信息
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
*	2: 已找到
*
* LOCAL AFFECTED
*
*************************************************************************/
typedef struct _ELF_SEC_ARG
{
	TUINT Type;
	const char *pName;
	const char *pShStrs;
	ELF_SHDR *pShdr;
} ELF_SEC_ARG;
static int ParseSec(ELF_SHDR *pShdr, void *pArg)
{
	ELF_SEC_ARG * const pSecArg = pArg;

	if (pShdr->sh_type == pSecArg->Type && (!pSecArg->pName || !strcmp(pShdr->sh_name + pSecArg->pShStrs, pSecArg->pName))) {
		pSecArg->pShdr = pShdr;
		return 2;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	获取指定类型段信息
*
* PARAMETERS
*	pPhdr: 段结构信息
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
*	2: 已找到
*
* LOCAL AFFECTED
*
*************************************************************************/
typedef struct _ELF_SEG_ARG
{
	TUINT Type;
	TUINT Flag;
	ELF_PHDR *pPhdr;
} ELF_SEG_ARG;
static int ParseSeg(ELF_PHDR *pPhdr, void *pArg)
{
	ELF_SEG_ARG * const pSegArg = pArg;

	if (pPhdr->p_type == pSegArg->Type && pPhdr->p_flags == pSegArg->Flag) {
		pSegArg->pPhdr = pPhdr;
		return 2;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	处理加载节
*
* PARAMETERS
*	pShdr: 节结构信息
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
typedef const struct _ELF_LOAD2_ARG
{
	int (*pFunc)(const char *pSecName, TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Addr, void *pArg);
	void *pArg;
	const char *pShStrs;
} ELF_LOAD2_ARG;
static int ParseLoad2(ELF_SHDR *pShdr, void *pArg)
{
	ELF_LOAD2_ARG * const pLoadArg = pArg;

	return ((pShdr->sh_flags&SHF_ALLOC) && pShdr->sh_size ? 
	  pLoadArg->pFunc(pLoadArg->pShStrs + pShdr->sh_name, PF_R|FLAG_TRANS(pShdr->sh_flags, SHF_WRITE, PF_W)|FLAG_TRANS(pShdr->sh_flags, SHF_EXECINSTR, PF_X)
		|(pShdr->sh_type == SHT_NOTE ? PF_W : 0) /* NOTE段包含BSS!!! */
		, pShdr->sh_type == SHT_NOBITS ? 0 : pShdr->sh_offset, pShdr->sh_size, pShdr->sh_addr, pLoadArg->pArg) : 0);
}

/*************************************************************************
* DESCRIPTION
*	处理PT_LOAD段
*
* PARAMETERS
*	pPhdr: 段结构信息
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
typedef const struct _ELF_LOAD_ARG
{
	int (*pFunc)(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Pa, TADDR Va, void *pArg);
	void *pArg;
	const char *pShStrs;
} ELF_LOAD_ARG;
static int ParseLoad(ELF_PHDR *pPhdr, void *pArg)
{
	ELF_LOAD_ARG * const pLoadArg = pArg;

	return (pPhdr->p_type == PT_LOAD ?
		pLoadArg->pFunc(pPhdr->p_flags, pPhdr->p_offset, pPhdr->p_filesz, pPhdr->p_vaddr, pPhdr->p_paddr, pLoadArg->pArg) : 0);
}

/*************************************************************************
* DESCRIPTION
*	遍历elf句柄的节
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pFunc: 回调函数
*		pShdr: 节结构信息
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ForEachShdr(const ELF_INT *pElf, int (*pFunc)(ELF_SHDR *pShdr, void *pArg), void *pArg)
{
	ELF_SHDR *pShdr = pElf->pShdrs, *pEShdr;
	int rt;

	if (!pShdr)
		return 1;
	for (pEShdr = pShdr + pElf->ShCnt; pShdr < pEShdr; pShdr++) {
		rt = pFunc(pShdr, pArg);
		if (rt)
			return rt;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	遍历elf句柄的段
*
* PARAMETERS
*	pElfHdl: elf句柄
*	pFunc: 回调函数
*		pPhdr: 段结构信息
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ForEachPhdr(const ELF_INT *pElf, int (*pFunc)(ELF_PHDR *pPhdr, void *pArg), void *pArg)
{
	ELF_PHDR *pPhdr = pElf->pPhdrs, *pEPhdr;
	int rt;

	if (!pPhdr)
		return 1;
	for (pEPhdr = pPhdr + pElf->PhCnt; pPhdr < pEPhdr; pPhdr++) {
		rt = pFunc(pPhdr, pArg);
		if (rt)
			return rt;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	分配sym内存
*
* PARAMETERS
*	无
*
* RETURNS
*	NULL: 失败
*	其他: sym
*
* LOCAL AFFECTED
*
*************************************************************************/
static struct _ELF_SYM *ElfSymAlloc(void)
{
	struct _ELF_SYM *pNew;
	unsigned SymMax;

	if (pElfSym->SymCnt + 1 > pElfSym->SymMax) {
		SymMax = pElfSym->SymMax * 2 + 1024 * 32;
		pNew = Realloc(pElfSym->pSyms, SymMax * sizeof(*pNew));
		if (!pNew)
			return NULL;
		pElfSym->pSyms  = pNew;
		pElfSym->SymMax = SymMax;
	}
	pNew = pElfSym->pSyms + pElfSym->SymCnt;
	pElfSym->SymCnt++;
	return pNew;
}

/*************************************************************************
* DESCRIPTION
*	分配sym string内存
*
* PARAMETERS
*	Size: 大小
*
* RETURNS
*	NULL: 失败
*	其他: sym string
*
* LOCAL AFFECTED
*
*************************************************************************/
static char *ElfSymStrAlloc(unsigned Size)
{
	unsigned NewStrsSz, SymStrsMax;
	char *pNew;

	NewStrsSz = pElfSym->SymStrsSz + Size;
	if (NewStrsSz > pElfSym->SymStrsMax) {
		SymStrsMax = (NewStrsSz * 2 + 1024 * 1024);
		pNew = Realloc(pElfSym->pSymStrs, SymStrsMax);
		if (!pNew)
			return NULL;
		pElfSym->pSymStrs   = pNew;
		pElfSym->SymStrsMax = SymStrsMax;
	}
	pNew = pElfSym->pSymStrs + pElfSym->SymStrsSz;
	pElfSym->SymStrsSz = NewStrsSz;
	return pNew;
}


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
TSIZE ElfGetSecInf(ELF_HDL *pElfHdl, TADDR *pAddr, TSIZE *pFOff, TUINT Type, const char *pName)
{
	ELF_INT * const pElf = (ELF_INT *)pElfHdl;
	ELF_SEC_ARG SecArg;
	int rt;

	SecArg.Type	= Type;
	SecArg.pName   = pName;
	SecArg.pShStrs = pElf->pShStrs;
	rt = ForEachShdr(pElf, ParseSec, &SecArg);
	if (rt == 2) {
		*pAddr = SecArg.pShdr->sh_addr;
		*pFOff = SecArg.pShdr->sh_offset;
		return SecArg.pShdr->sh_size;
	}
	return 0;
}

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
TSIZE ElfGetSymInf(ELF_HDL *pElfHdl, TSIZE *pSymFOff, TSIZE *pStrsFOff, TSIZE *pStrsSz, TUINT *pGlbIdx)
{
	const ELF_INT * const pElf = (ELF_INT *)pElfHdl;
	ELF_SHDR *pShdr, *pTmp;

	if (pElf->SymIdx == ELF_INVALID_IDX)
		return 0;
	pShdr = &pElf->pShdrs[pElf->SymIdx];
	*pSymFOff = pShdr->sh_offset;
	*pGlbIdx  = pShdr->sh_info;
	pTmp = &pElf->pShdrs[pShdr->sh_link];
	*pStrsFOff = pTmp->sh_offset;
	*pStrsSz   = pTmp->sh_size;
	return pShdr->sh_size;
}

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
TSIZE ElfGetSegInf(ELF_HDL *pElfHdl, TADDR *pAddr, TSIZE *pFOff, TUINT Type, TUINT Flag)
{
	ELF_SEG_ARG SegArg;
	int rt;

	SegArg.Type = Type;
	SegArg.Flag = Flag;
	rt = ForEachPhdr((ELF_INT *)pElfHdl, ParseSeg, &SegArg);
	if (rt == 2) {
		*pAddr = SegArg.pPhdr->p_vaddr;
		*pFOff = SegArg.pPhdr->p_offset;
		return SegArg.pPhdr->p_filesz;
	}
	return 0;
}

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
int ElfForEachSec(ELF_HDL *pElfHdl, int (*pFunc)(const char *pSecName, TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Addr, void *pArg), void *pArg)
{
	struct _ELF_LOAD2_ARG LoadArg;

	LoadArg.pFunc = pFunc;
	LoadArg.pArg  = pArg;
	LoadArg.pShStrs = ((ELF_INT *)pElfHdl)->pShStrs;
	return ForEachShdr((ELF_INT *)pElfHdl, ParseLoad2, &LoadArg);
}

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
	, int (*pFunc)(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg), void *pArg)
{
	struct _ELF_LOAD_ARG LoadArg;

	LoadArg.pFunc = pFunc;
	LoadArg.pArg  = pArg;
	return ForEachPhdr((ELF_INT *)pElfHdl, ParseLoad, &LoadArg);
}

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
int ElfForEachNote(ELF_HDL *pElfHdl, int (*pFunc)(TUINT Type, const char *pName, const void *pBuf, TUINT Size, void *pArg), void *pArg)
{
	typedef const struct _ELF_NOTE
	{
		TUINT Namesz;
		TUINT Descsz;
		TUINT Type;
	} ELF_NOTE;
	const ELF_INT * const pElf = (ELF_INT *)pElfHdl;
	const char *pBuf;
	ELF_NOTE *pNote;
	TSIZE i, Size;
	TADDR Addr;
	TSIZE FOff;
	int rt;

	if (!pElf->pFpHdl)
		return 1;
	Size = ElfGetSegInf(pElfHdl, &Addr, &FOff, PT_NOTE, 0); //elf PT_NOTE 段size
	if (!Size) {
		SSTERR(RS_NO_SEG, pElf->pFpHdl->pPath, ".note");
		return 1;
	}
	pBuf = FpMapRo(pElf->pFpHdl, FOff, Size);
	if (!pBuf) {
		SSTWARN(RS_FILE_MAP, pElf->pFpHdl->pPath, ".note");
		return 1;
	}
	for (rt = 0, i = 0; i < Size; i = (i + pNote->Descsz + 3)&~(TSIZE)3/* 4字节对齐 */) {
		pNote = (ELF_NOTE *)&pBuf[i];
		i = (i + sizeof(*pNote) + pNote->Namesz + 3)&~(TSIZE)3; /* 4字节对齐 */
		rt = pFunc(pNote->Type, (char *) (pNote + 1), &pBuf[i], pNote->Descsz,
				pArg);   //parseNote info
		if (rt)
			break;
	}
	FpUnmap(pElf->pFpHdl, pBuf, Size);
	return rt;
}

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
int BuildOneSym(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg)
{
#define CMM_SYMSTR2OFF(p)	   ((TUINT)((uintptr_t)(p) - (uintptr_t)pElfSym->pSymStrs))
	struct _ELF_SYM *pSym;
	unsigned NLen;
	char *pStr;

	(void)pArg;
	NLen = strlen(pName) + 1;
	pStr = ElfSymStrAlloc(NLen);
	pSym = ElfSymAlloc();
	if (pStr && pSym) {
		memcpy(pStr, pName, NLen);
		pSym->st_name  = CMM_SYMSTR2OFF(pStr);
		pSym->st_value = Val;
		pSym->st_size  = Size;
		*((TUINT *)&pSym->st_info) = Info;
		pSym->st_shndx = 1; /* 固定一个segment */
		return 0;
	}
	return 1;
}

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
int CreateSymFile(const char *pFile, int (*BuildSyms)(void *pArg), void *pArg)
{
	struct _ELF_SHDR *pShdr;
	struct _ELF_SYM *pSym;
	unsigned FileOff;
	int rt, Idx;
	FILE *fp;
	char *c;
	
	pElfSym = Calloc(sizeof(*pElfSym));
	if (!pElfSym) {
		SSTWARN(RS_OOM);
		return 1;
	}
	pSym = ElfSymAlloc();
	if (pSym)
		memset(pSym, 0, sizeof(*pSym));
	c = ElfSymStrAlloc(1);
	if (c)
		*c = '\0';
	rt = BuildSyms(pArg);
	if (rt) {
		SSTWARN(RS_OOM);
		goto _END;
	}
	if (pElfSym->SymCnt <= 1) { /* 没有symbol */
		rt = 1;
		goto _END;
	}
	if (pElfSym->SymStrsSz&0x03) /* 字对齐 */
		ElfSymStrAlloc(4 - (pElfSym->SymStrsSz&0x03));
	/* 创建文件 */
	fp = fopen(pFile, "wb");
	if (!fp) {
		SSTERR(RS_CREATE, pFile);
		rt = 1;
		goto _END;
	}
	/* 写入hdr */
	FileOff = fwrite(&pElfSym->Hdr, 1, sizeof(pElfSym->Hdr), fp);
	/* 写入sym */
	pElfSym->SymsOff = FileOff;
	FileOff += fwrite(pElfSym->pSyms, 1, sizeof(*pElfSym->pSyms) * pElfSym->SymCnt, fp);
	/* 写入sym str */
	pElfSym->SymStrsOff = FileOff;
	FileOff += fwrite(pElfSym->pSymStrs, 1, pElfSym->SymStrsSz, fp);
	/* 写入sh str */
	pElfSym->ShStrsOff = FileOff;
	FileOff += fwrite(CMM_SHSTR_N, 1, sizeof(CMM_SHSTR_N)&~3UL, fp);
	/* 写入shdrs */
	/* 构造.shstrtab */
	Idx = sizeof(CMM_SHDR_NONE_N) - 1;
	pShdr = &pElfSym->Shdrs[CMM_SHDR_SHSTR];
	pShdr->sh_name	  = Idx;
	pShdr->sh_type	  = SHT_STRTAB;
	pShdr->sh_size	  = sizeof(CMM_SHSTR_N)&~3UL;
	pShdr->sh_addralign = 1;
	pShdr->sh_offset	= pElfSym->ShStrsOff;
	Idx += sizeof(CMM_SHDR_SHSTR_N) - 1;
	/* 构造.symtab */
	pShdr = &pElfSym->Shdrs[CMM_SHDR_SYMTAB];
	pShdr->sh_name	  = Idx;
	pShdr->sh_type	  = SHT_SYMTAB;
	pShdr->sh_size	  = pElfSym->SymCnt * sizeof(*pElfSym->pSyms);
	pShdr->sh_addralign = 4;
	pShdr->sh_offset	= pElfSym->SymsOff;
	pShdr->sh_entsize   = sizeof(*pElfSym->pSyms);
	pShdr->sh_info	  = 1; /* global sym idx */
	pShdr->sh_link	  = CMM_SHDR_SYMSTR; /* .strtab index */
	Idx += sizeof(CMM_SHDR_SYMTAB_N) - 1;
	/* 构造.strtab */
	pShdr = &pElfSym->Shdrs[CMM_SHDR_SYMSTR];
	pShdr->sh_name	  = Idx;
	pShdr->sh_type	  = SHT_STRTAB;
	pShdr->sh_size	  = pElfSym->SymStrsSz;
	pShdr->sh_addralign = 1;
	pShdr->sh_offset	= pElfSym->SymStrsOff;
	pElfSym->ShdrsOff = FileOff;
	FileOff += fwrite(pElfSym->Shdrs, 1, sizeof(pElfSym->Shdrs), fp);
	/* 写入头部 */
	memcpy(pElfSym->Hdr.e_ident, ELFMAG, SELFMAG);
	pElfSym->Hdr.e_ident[EI_CLASS]	= ELF_CLASS;
	pElfSym->Hdr.e_ident[EI_DATA]	= ELF_DATA;
	pElfSym->Hdr.e_ident[EI_VERSION] = EV_CURRENT;
	pElfSym->Hdr.e_ident[EI_OSABI]	= ELFOSABI_NONE;
	pElfSym->Hdr.e_type		= 3; /* ET_DYN */
	pElfSym->Hdr.e_machine	= EM_MACH;
	pElfSym->Hdr.e_version	= EV_CURRENT;
	pElfSym->Hdr.e_ehsize	= sizeof(pElfSym->Hdr);
	pElfSym->Hdr.e_shoff	= pElfSym->ShdrsOff;
	pElfSym->Hdr.e_shentsize = sizeof(pElfSym->Shdrs[0]);
	pElfSym->Hdr.e_shnum	= (unsigned short)ARY_CNT(pElfSym->Shdrs);
	pElfSym->Hdr.e_shstrndx	= CMM_SHDR_SHSTR;
	fseek(fp, 0, SEEK_SET);
	fwrite(&pElfSym->Hdr, 1, sizeof(pElfSym->Hdr), fp);
	fclose(fp);
	Idx = sizeof(pElfSym->Hdr) + sizeof(*pElfSym->pSyms) * pElfSym->SymCnt + pElfSym->SymStrsSz + (sizeof(CMM_SHSTR_N)&~3UL) + sizeof(pElfSym->Shdrs);
	if ((unsigned)Idx != FileOff) {
		SSTERR(RS_FILE_WRITE, pFile);
		remove(pFile);
		rt = 1;
	}
_END:
	Free(pElfSym->pSyms);
	Free(pElfSym->pSymStrs);
	Free(pElfSym);
	return rt;
}


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
ELF_HDL *GetElfHdl(const void *pFpHdl, TUINT Format)
{
	static const char * const pFormats[] = {"NONE", "REL", "EXEC", "DYN", "CORE"};
	const char * const pPath = ((FP_HDL *)pFpHdl)->pPath;
	ELF_SHDR *pShdr;
	ELF_PHDR *pPhdr;
	ELF_INT *pElf;
	ELF_HDR *pHdr;

	/* 读取elf头 */
	pHdr = FpMapRo(pFpHdl, 0, sizeof(*pHdr));
	if (!pHdr) {
		SSTWARN(RS_FILE_MAP, pPath, "elf header");
		return NULL;
	}
	/* 检查头部 */
	if (memcmp(pHdr->e_ident, ELFMAG, SELFMAG)) {
		SSTERR(RS_ELF_HEADER, pPath, pHdr->e_ident[EI_MAG0], pHdr->e_ident[EI_MAG1], pHdr->e_ident[EI_MAG2], pHdr->e_ident[EI_MAG3]);
		goto _ERR1;
	}
	if (pHdr->e_ident[EI_CLASS] != ELF_CLASS) {
		SSTERR(RS_ELF_TYPE, pPath, ELF_CLASS == ELFCLASS64 ? "64bit" : "32bit");
		goto _ERR1;
	}
	if (pHdr->e_ident[EI_DATA] != ELF_DATA) {
		SSTERR(RS_ELF_TYPE, pPath, ELF_DATA == ELFDATA2LSB ? "little endian" : "big endian");
		goto _ERR1;
	}
	if (pHdr->e_machine != EM_MACH) {
		SSTERR(RS_ELF_TYPE, pPath, EM_MACH == EM_ARM ? "ARM" : "ARM64");
		goto _ERR1;
	}
	if (!(Format&(1 << pHdr->e_type))) {
		SSTERR(RS_ELF_FORMAT, pHdr->e_type < ARY_CNT(pFormats) ? pFormats[pHdr->e_type] : "?", pPath);
		goto _ERR1;
	}
	/* 创建句柄 */
	pElf = Calloc(sizeof(*pElf));
	if (!pElf) {
		SSTWARN(RS_OOM);
		goto _ERR1;
	}
	if (pHdr->e_shstrndx >= pHdr->e_shnum || ((FP_HDL *)pFpHdl)->FileSz < pHdr->e_shoff + pHdr->e_shnum * sizeof(*pElf->pShdrs)
	  || ((FP_HDL *)pFpHdl)->FileSz < pHdr->e_phoff) /* 检查文件完整性 */
		goto _DAMAGED;
	pElf->pFpHdl = DupFpHdl(pFpHdl);
	pElf->SymIdx = ELF_INVALID_IDX;
	/* 读取节表 */
	pElf->ShCnt = pHdr->e_shnum;
	if (pElf->ShCnt) {
		pElf->pShdrs = FpMapRo(pFpHdl, pHdr->e_shoff, pElf->ShCnt * sizeof(*pElf->pShdrs));
		if (!pElf->pShdrs) {
			SSTWARN(RS_FILE_MAP, pPath, "shdr table");
			goto _ERR2;
		}
		pShdr = &pElf->pShdrs[pElf->ShCnt - 1];
		if (pElf->pFpHdl->FileSz < pShdr->sh_offset + pShdr->sh_size) /* 检查文件完整性 */
			goto _DAMAGED;
		pShdr = &pElf->pShdrs[pHdr->e_shstrndx];
		pElf->ShStrsSz = pShdr->sh_size;
		pElf->pShStrs  = FpMapRo(pFpHdl, pShdr->sh_offset, pElf->ShStrsSz);
		if (!pElf->pShStrs) {
			SSTWARN(RS_FILE_MAP, pPath, ".shstrtab");
			goto _ERR2;
		}
		ForEachShdr(pElf, FindSymIdx, pElf);
	}
	/* 读取段表 */
	pElf->OPhCnt = pHdr->e_phnum;
	if (pElf->OPhCnt) {
		if (pElf->OPhCnt == PN_XNUM && pElf->pShdrs && pElf->pShdrs[0].sh_size == 1) /* PhCnt超过了PN_XNUM */
			pElf->OPhCnt = (size_t)pElf->pShdrs[0].sh_info;
		pElf->pPhdrs = FpMapRo(pFpHdl, pHdr->e_phoff, pElf->OPhCnt * sizeof(*pElf->pPhdrs));
		if (!pElf->pPhdrs) {
			SSTWARN(RS_FILE_MAP, pPath, "phdr table");
			goto _ERR2;
		}
		for (; pElf->PhCnt < pElf->OPhCnt && (pPhdr = &pElf->pPhdrs[pElf->PhCnt])->p_type; pElf->PhCnt++) { /* 检查文件完整性 */
			if (pElf->pFpHdl->FileSz < pPhdr->p_offset + pPhdr->p_filesz) {
				if (pElf->PhCnt)
					pElf->PhCnt--;
				SSTWARN(RS_FILE_DAMAGED, pPath);
				break;
			}
		}
	}
	FpUnmap(pFpHdl, pHdr, sizeof(*pHdr));
	return (ELF_HDL *)pElf;
_DAMAGED:
	SSTERR(RS_FILE_DAMAGED, pPath);
_ERR2:
	PutElfHdl((ELF_HDL *)pElf);
_ERR1:
	FpUnmap(pFpHdl, pHdr, sizeof(*pHdr));
	return NULL;
}

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
void PutElfHdl(ELF_HDL *pElfHdl)
{
	ELF_INT * const pElf = (ELF_INT *)pElfHdl;

	if (pElf->pFpHdl) {
		if (pElf->pShdrs) {
			FpUnmap(pElf->pFpHdl, pElf->pShdrs, pElf->ShCnt * sizeof(*pElf->pShdrs));
			if (pElf->pShStrs)
				FpUnmap(pElf->pFpHdl, pElf->pShStrs, pElf->ShStrsSz);
		}
		if (pElf->pPhdrs)
			FpUnmap(pElf->pFpHdl, pElf->pPhdrs, pElf->OPhCnt * sizeof(*pElf->pPhdrs));
		PutFpHdl(pElf->pFpHdl);
	}
	Free(pElf);
}
