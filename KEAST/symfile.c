/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	symfile.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/5/22
* Description: 符号文件解析
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "string_res.h"
#include <module.h>
#include <armelf.h>
#include "os.h"
#include <memspace.h>
#include <dwarf/dwarf.h>
#include <symfile.h>

#if defined(__MINGW32__)
#include <stdint.h>   /*For define uintptr_t*/
#endif

/*======通用结构体======*/
#define SYM_MAX_SZ	0x3000 /* 符号最大长度 */
#define BANNER_SZ	512
#define SYMF_INT(Type, pSymf) ((SYMF_##Type *)((char *)(pSymf) - OFFSET_OF(SYMF_##Type, c)))

typedef struct _VM_REG
{
	TADDR VAddr, VREnd, VEnd; /* [VAddr, VEnd) */
	TSIZE FOff;
	const char *pBuf;
	TUINT Attr;
} VM_REG;

typedef struct _SYM_INT
{
/* Ex.Flag内部宏，不能超过SYM_RBITS!!! */
#define SYM_MEMSYM	(1 << 5) /* ELF_SYM来自内存, module专用 */
#define SYM_LOCKRW	(1 << 12) /* 锁住RW段 */
#define SYM_UMASK	(~((1 << SYM_RBITS) - 1))
	struct _SYM_HDL Ex; /* export info */
	/* === 加载信息 === */
	FP_HDL *pFpHdl;
	VM_REG Regs[10];
	size_t RCnt;
	/* 如果stripped则为NULL */
	DWF_HDL *pDwfHdl;
	DWI_HDL *pDwiHdl; /* 根据filename获取gdb句柄信息段？ */
	const char *pSymStr; /* 字符串表 */
	TSIZE SymStrSz;
	ELF_SYM *pSLSym, *pSGSym, *pEGSym; /* [pSLSym, pSGSym) 局部符号表, [pSGSym, pEGSym) 全局符号表 */
	/* 函数 */
	TADDR (*Name2Addr)(const struct _SYM_INT *pSymf, TSIZE *pSize, const char *pName, unsigned Op);
	const char *(*Addr2Name)(const struct _SYM_INT *pSymf, TADDR *pAddr, TADDR Addr, unsigned Op);
	int (*ForEachSym)(const struct _SYM_INT *pSymf
		, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag);
	void (*DeInit)(struct _SYM_INT *pSymf);
} SYM_INT;

typedef struct _BIND_ARG
{
	SYM_INT *pSymf;
	unsigned Attr;
} BIND_ARG;

/*======vmlinux相关结构体======*/
#define DEF_VMLINUX			 "vmlinux"
#define STT_IN_OPT(StInfo, Op)  ((1 << ELF_ST_TYPE(StInfo))&(Op))
#if (1 << STT_NOTYPE) != OPT_NOTYPE || (1 << STT_OBJECT) != OPT_OBJECT || ((1 << STT_FUNC)|(1 << STT_ARM_TFUNC)) != OPT_FUNC \
	|| (1UL << STB_LOCAL) != (OPB_LOCAL >> OPB_SHIFT) || (1UL << STB_GLOBAL) != (OPB_GLOBAL >> OPB_SHIFT)
#error OP* and ST* are not matched!!!
#endif

typedef struct _VM_DIFARG
{
	FILE *fp;
	SYM_INT *pSymf;
	unsigned Cnt, IgnoreCnt;
	int IsMatched;
	TADDR EAddr, LoadBias;
	TADDR IgnoreSAddrs[7], IgnoreEAddrs[7]; /* [) */
} VM_DIFARG;

/*======ksym相关结构体======*/
#define KSYM_NAME_LEN   128

typedef struct _SYMF_KS
{
	SYM_INT c;
	const TADDR *pKSymAddrs, *pKSymMarks;
	const unsigned char *pKSymStrs;
	const char *pKSymTTbl;
	const unsigned short *pKSymTIdx;
	unsigned KSymCnt, KSymNCnt/* 缓存名称个数 */;
	char **ppKSymN; /* 缓存名称 */
	VMA_HDL *pVmaHdl;
} SYMF_KS;

typedef struct _KSYM_ARG
{
	unsigned long long Off; /* sym句柄里的buffer与虚拟地址的偏移量 */
	TADDR SearchStart, SearchEnd;
	TADDR CodeEnd, RoDataEnd, BssEnd; /* 返回区域位置信息 */
	SYMF_KS *pSymfKs; /* sym句柄 */
	int rt;
} KSYM_ARG;

/*======symbol file相关结构体======*/
typedef struct _KALLSYM_UNIT
{
	TADDR Addr;
	TSIZE Size; /* 0: 未知大小, 其他: 大小 */
	unsigned NameIdx;
	unsigned Op; /* OP*类型 */
} KALLSYM_UNIT;

typedef struct _KALLSYM_SF
{
	struct _KALLSYM_SF *pNext;
	char ModName[MOD_NAME_LEN + 1];
	TADDR LoadBias; /* 0: 未定义, 其他: 模块加载地址 */
	KALLSYM_UNIT *pKSymAry;
	unsigned KSymCnt, KSymAllocCnt;
	char *pKSymStrs;
	unsigned KSymIdx, KSymAllocSz;
	int IsFKallSyms;
} KALLSYM_SF;

typedef struct _SYMF_SF
{
	SYM_INT c;
	KALLSYM_UNIT *pKSymAry;
	unsigned KSymCnt; /* 可能为0 */
	char *pKSymStrs;
	int IsFKallSyms;
} SYMF_SF;

static KALLSYM_SF *pKallSymSf; /* SYS_KALLSYMS或kelog符号信息缓存 */

/*======module相关结构体======*/
typedef enum _MOD_ID
{
#define MODOFF_MAP \
	_NL(MOD_START,	"module_core") \
	_NL(MOD_SIZE,	"core_size") \
	_NL(MOD_TEXTSZ,	"core_text_size") \
	_NL(MOD_ROSZ,	"core_ro_size") \
	/* 这里添加新的成员 */ \
	_NL(MOD_SYM,	"kallsyms")

#define _NL(Enum, str) Enum,
	MODOFF_MAP
#undef _NL
} MOD_ID;

typedef struct _SYMF_MD
{
	SYM_INT c;
	char Name[MOD_NAME_LEN + sizeof(".ko")];
} SYMF_MD;

static struct
{
	TUINT ModSz;
	int ModOffs[MOD_SYM + 1];
} ModInf;
static TADDR LastSymfAddr;
static char VmsBuf[sizeof(SYMF_SF) >= sizeof(SYMF_KS) ? sizeof(SYMF_SF) : sizeof(SYMF_KS)];
SYM_HDL *pVmSHdl;/* vmlinux符号文件句柄 */
unsigned MCntInstSize;
const char * const pUndFuncName = "??";


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	释放sym句柄
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FreeSymHdl(SYM_HDL *pSymHdl, void *pArg)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);
	size_t i;

	(void)pArg;
	if (pSymf->DeInit)
		pSymf->DeInit(pSymf);
	if (pSymf->pFpHdl) {
		for (i = 0; i < pSymf->RCnt; i++) {
			if (pSymf->Regs[i].pBuf)
				FpUnmap(pSymf->pFpHdl, pSymf->Regs[i].pBuf, pSymf->Regs[i].VEnd - pSymf->Regs[i].VAddr);
		}
		if (pSymf->pDwfHdl)
			PutDwfHdl(pSymf->pDwfHdl);
		if (pSymf->pDwiHdl)
			PutDwiHdl(pSymf->pDwiHdl);
		if (pSymf->pSLSym && !(pSymf->Ex.Flag&SYM_MEMSYM)) {
			FpUnmap(pSymf->pFpHdl, pSymf->pSLSym, (TSIZE)((char *)pSymf->pEGSym - (char *)pSymf->pSLSym));
			FpUnmap(pSymf->pFpHdl, pSymf->pSymStr, pSymf->SymStrSz);
		}
		PutFpHdl(pSymf->pFpHdl);
	}
	if (pSymHdl != pVmSHdl)
		Free(pSymf);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	设置sym句柄对应vma的属性
*
* PARAMETERS
*	pVmaHdl: vma句柄
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int BindSym2Vma(VMA_HDL *pVmaHdl, void *pArg)
{
	BIND_ARG * const pBindArg = pArg;
	SYM_INT * const pSymf = pBindArg->pSymf;
	VMA_HDL *ptVmaHdl;

	VmaSetAttr(pVmaHdl, pSymf->Ex.pName, VMA_ELF|(pVmaHdl->Flag&VMA_R ? pBindArg->Attr : VMA_ATTR(pVmaHdl))/* 只替换正常属性 */);
	VmaRegHdl(pVmaHdl, MOD_HDL(pSymf));
	if (pVmaHdl->Flag&VMA_X) {
		ptVmaHdl = GetVmaHdl(LastSymfAddr);
		if (!ptVmaHdl || ptVmaHdl->pHdl != MOD_HDL(pSymf)) {
			VmaLink(pVmaHdl, LastSymfAddr);
			LastSymfAddr = pVmaHdl->SAddr;
		}
		if (ptVmaHdl)
			PutVmaHdl(ptVmaHdl);
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	创建代码背景映射
*
* PARAMETERS
*	SAddr/EAddr: [起始地址, 结束地址)
*	Flag: vma标志
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 完成
*
* LOCAL AFFECTED
*
*************************************************************************/
static int BuildCodeBgMapEx(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg)
{
	TADDR * const pCodeAry = pArg;

	(void)Flag;
	if (SAddr > pCodeAry[1])
		SAddr = pCodeAry[1];
	if (pCodeAry[0] < SAddr && CreateMemRegion(pCodeAry[0], SAddr - pCodeAry[0], (unsigned)pCodeAry[2], 0, NULL, NULL, NULL))
		return 1;
	return (pCodeAry[0] < EAddr ? ((pCodeAry[0] = EAddr) >= pCodeAry[1]) : 0);
}

/*************************************************************************
* DESCRIPTION
*	创建symbol背景映射(为了stack unwind/符号获取)
*
* PARAMETERS
*	SAddr/EAddr: [起始地址, 结束地址)
*	Attr: VMA_MMASK
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int BuildSymBgMap(TADDR SAddr, TADDR EAddr, unsigned Attr)
{
	TADDR CodeAry[3];

	CodeAry[0] = SAddr&PAGE_MASK;
	CodeAry[1] = (EAddr + PAGE_SIZE - 1)&PAGE_MASK;
	CodeAry[2] = (TADDR)(Attr&~VMA_R); /* 缺少VMA_R使MMU不识别 */
	ForEachVma(CodeAry[0], CodeAry[1], BuildCodeBgMapEx, CodeAry);
	if (CodeAry[0] >= CodeAry[1])
		return 0;
	return CreateMemRegion(CodeAry[0], CodeAry[1] - CodeAry[0], (unsigned)CodeAry[2], 0, NULL, NULL, NULL);
}

/*************************************************************************
* DESCRIPTION
*	转换ksym类型为通用OP*类型
*
* PARAMETERS
*	Type: ksym类型
*
* RETURNS
*   OP*类型
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static unsigned KSymType2Op(char Type)
{
	static const unsigned OpTAry[] =
	{
		OPT_OBJECT, OPT_OBJECT, OPT_NOTYPE, OPT_OBJECT, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, /*a~j*/
		OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE, OPT_FUNC, /* k~t */
		OPT_NOTYPE, OPT_NOTYPE, OPT_FUNC, OPT_NOTYPE, OPT_NOTYPE, OPT_NOTYPE /* u~z */
	};

	return (Type < 'a' ? OPB_GLOBAL|OpTAry[Type - 'A'] : OPB_LOCAL|OpTAry[Type - 'a']);
}

/*************************************************************************
* DESCRIPTION
*	检查是否是ARM的mapping符号
*
* PARAMETERS
*	pStr: 符号名称
*
* RETURNS
*	0: 不是
*	1: 是
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static int IsArmMappingSym(const char *pStr)
{
	return pStr[0] == '$' && strchr("atd", pStr[1]) && (pStr[2] == '\0' || pStr[2] == '.');
}

/*************************************************************************
* DESCRIPTION
*	查找并返回指定模块名称的kall symbol file结构体
*
* PARAMETERS
*	pModName: 模块名称
*	DoAdd:
*		0: 查收成功则从链表摘除并返回该结构体(不用时需要释放!)，否则返回NULL
*		1: 查找成功则返回该结构体，否则添加该模块并返回
*
* RETURNS
*	NULL: 失败
*	其他: kall symbol file结构体
*
* LOCAL AFFECTED
*
*************************************************************************/
static KALLSYM_SF *GetKaSymfs(const char *pModName, int DoAdd)
{
	KALLSYM_SF *pKaSymSf, *pPre = NULL;

	for (pKaSymSf = pKallSymSf; pKaSymSf; pPre = pKaSymSf, pKaSymSf = pKaSymSf->pNext) {
		if (!strcmp(pKaSymSf->ModName, pModName)) {
			if (!DoAdd) {
				if (pPre)
					pPre->pNext = pKaSymSf->pNext;
				else
					pKallSymSf = pKaSymSf->pNext;
			}
			return pKaSymSf;
		}
	}
	if (DoAdd) {
		pKaSymSf = Calloc(sizeof(*pKaSymSf));
		if (pKaSymSf) {
			strncpy(pKaSymSf->ModName, pModName, sizeof(pKaSymSf->ModName) - 1);
			pKaSymSf->pNext = pKallSymSf;
			pKallSymSf = pKaSymSf;
		}
	}
	return pKaSymSf;
}

/*************************************************************************
* DESCRIPTION
*	释放kall symbol file结构体
*
* PARAMETERS
*	pKaSymSf: kall symbol file结构体
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FreeKaSymfs(KALLSYM_SF *pKaSymSf)
{
	Free(pKaSymSf->pKSymAry);
	Free(pKaSymSf->pKSymStrs);
	Free(pKaSymSf);
}

/*************************************************************************
* DESCRIPTION
*	添加符号到kall symbol file结构体，保证排序!!!
*
* PARAMETERS
*	pKaSymSf: kall symbol file结构体
*	pName: 符号名称
*	Addr/Size: 符号地址和大小(字节)
*	Op: 符号类型
*
* RETURNS
*	0: 成功
*	1: 失败
*	2: 重复记录
*
* LOCAL AFFECTED
*
*************************************************************************/
static int KaSfAddSym(KALLSYM_SF *pKaSymSf, const char *pName, TADDR Addr, TSIZE Size, unsigned Op)
{
	KALLSYM_UNIT *pAry = pKaSymSf->pKSymAry;
	char *pStrs = pKaSymSf->pKSymStrs;
	unsigned NewSz, i, low, high;
	TADDR tAddr;
	int Len;

	/* 查找是否重复 */
	i = pKaSymSf->KSymCnt;
	if (i) {
		if (pAry[i - 1].Addr > Addr) {
			high = i;
			low  = 0;
			while (high - low > 1) { /* 二分查找法 */
				i = low + ((high - low) >> 1);
				if (pAry[i].Addr <= Addr)
					low = i;
				else
					high = i;
			}
			tAddr = pAry[i = low].Addr;
			if (tAddr <= Addr) {
				while (pAry[++i].Addr == tAddr); /* 查找第1个> */
				for (low = i; pAry[low - 1].Addr == Addr; ) { /* 检查是否重复 */
					if (!strcmp(pStrs + pAry[low - 1].NameIdx, pName))
						return 2;
					if (!--low)
						break;
				}
			}
		} else if (pAry[i - 1].Addr == Addr && !strcmp(pStrs + pAry[i - 1].NameIdx, pName)) {
			return 2;
		}
	}
	Len = strlen(pName) + 1; /* \0 */
	/* 分配空间 */
	if (pKaSymSf->KSymIdx + Len > pKaSymSf->KSymAllocSz) {
		NewSz = (pKaSymSf->KSymIdx ? pKaSymSf->KSymIdx + Len + 24 * 1024 : 1536 * 1024);
		pStrs = Realloc(pKaSymSf->pKSymStrs, NewSz);
		if (!pStrs)
			return 1;
		pKaSymSf->pKSymStrs = pStrs;
		pKaSymSf->KSymAllocSz = NewSz;
	}
	memcpy(&pStrs[pKaSymSf->KSymIdx], pName, Len);
	if (pKaSymSf->KSymCnt + 1 > pKaSymSf->KSymAllocCnt) {
		NewSz = (pKaSymSf->KSymAllocCnt ? pKaSymSf->KSymAllocCnt + 400 : 80000);
		pAry  = Realloc(pKaSymSf->pKSymAry, sizeof(*pAry) * NewSz);
		if (!pAry)
			return 1;
		pKaSymSf->pKSymAry = pAry;
		pKaSymSf->KSymAllocCnt = NewSz;
	}
	/* 记录 */
	if (i < pKaSymSf->KSymCnt)
		memmove(&pAry[i + 1], &pAry[i], sizeof(*pAry) * (pKaSymSf->KSymCnt - i));
	pAry[i].NameIdx = pKaSymSf->KSymIdx;
	pAry[i].Addr	= Addr;
	pAry[i].Size	= Size;
	pAry[i].Op	  = Op;
	pKaSymSf->KSymCnt++;
	pKaSymSf->KSymIdx += Len;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	加载SYS_KALLSYMS符号信息
*
* PARAMETERS
*	无
*
* RETURNS
*	0: 成功
*	1: 失败
*	2: 不存在SYS_KALLSYMS
*
* LOCAL AFFECTED
*
*************************************************************************/
static int LoadKallSymsFile(void)
{
	char data[1024], Name[KSYM_NAME_LEN + 1], ModName[MOD_NAME_LEN + 1];
	KALLSYM_SF *pKaSymSf = NULL, *pVmKaSymSf = NULL;
	int ret = 0;
	TADDR Addr;
	char Type;
	FILE *fp;
	int rt;

	fp = fopen("SYS_KALLSYMS", "r");
	if (!fp)
		return 2;
	SSTINFO(RS_READ_SYMF, "SYS_KALLSYMS");
	while (fgets(data, sizeof(data), fp)) {
		rt = sscanf(data, "%"TLONGFMT"x %c %"MACRO2STR(KSYM_NAME_LEN)"s\t[%"MACRO2STR(MOD_NAME_LEN)"[^]]", &Addr, &Type, Name, ModName);
		if (rt < 3)
			continue;
		if (rt == 3) {
			if ((pKaSymSf = pVmKaSymSf) == NULL)
				goto _GETSTU;
		} else {
			if (IsArmMappingSym(Name)) /* 过滤 */
				continue;
			if (!pKaSymSf || strcmp(pKaSymSf->ModName, ModName)) {
_GETSTU:		if (!Addr) { /* 忽略地址为0的符号! */
					ret = 2;
					break;
				}
				pKaSymSf = (rt == 3 ? (pVmKaSymSf = GetKaSymfs(DEF_VMLINUX, 1)) : GetKaSymfs(ModName, 1));
				if (!pKaSymSf) {
					ret = 1;
					break;
				}
				pKaSymSf->IsFKallSyms = 1;
			}
		}
		if (KaSfAddSym(pKaSymSf, Name, Addr, 0, KSymType2Op(Type)) == 1)
			break;
	}
	fclose(fp);
	return ret;
}

/*************************************************************************
* DESCRIPTION
*	加载log里的符号信息
*
* PARAMETERS
*	pFileName: log文件名
*
* RETURNS
*	0: 成功
*	1: 失败
*	2: 不存在该文件
*
* LOCAL AFFECTED
*
*************************************************************************/
static int LoadKelogSym(const char *pFileName)
{
#ifdef SST64
#define BT_PATTERN  "<%"TLONGFMT"x>] %"MACRO2STR(KSYM_NAME_LEN)"[^+]+%x/%x%n [%"MACRO2STR(MOD_NAME_LEN)"[^]]%n"
#else
#define BT_PATTERN  "<%"TLONGFMT"x>] (%"MACRO2STR(KSYM_NAME_LEN)"[^+)]+%x/%x%n [%"MACRO2STR(MOD_NAME_LEN)"[^]]%n"
#endif
	char data[1024], Name[KSYM_NAME_LEN + 1], ModName[MOD_NAME_LEN + 1];
	KALLSYM_SF *pKaSymSf = NULL, *pVmKaSymSf = NULL;
	unsigned Off, Size, Pos, Pos2;
	int ret = 0;
	TADDR Addr;
	FILE *fp;
	char *p;
	int rt;

	fp = fopen(pFileName, "r");
	if (!fp)
		return 2;
	SSTINFO(RS_READ_SYMF, pFileName);
	while (fgets(data, sizeof(data), fp)) {
		p = data;
		while ((p = strstr(p, "[<")) != NULL) {
			rt = sscanf(++p, BT_PATTERN, &Addr, Name, &Off, &Size, &Pos, ModName, &Pos2);
			if (rt < 4 || Addr - Off < START_KERNEL)
				continue;
			if (rt == 4) {
				pKaSymSf = pVmKaSymSf;
				if (!pKaSymSf) {
					pKaSymSf = pVmKaSymSf = GetKaSymfs(DEF_VMLINUX, 1);
					if (!pKaSymSf) {
						ret = 1;
						goto _OUT;
					}
				}
				p += Pos;
			} else {
				if (!pKaSymSf || strcmp(pKaSymSf->ModName, ModName)) {
					pKaSymSf = GetKaSymfs(ModName, 1);
					if (!pKaSymSf) {
						ret = 1;
						goto _OUT;
					}
				}
				p += Pos2;
			}
			if (KaSfAddSym(pKaSymSf, Name, Addr - Off, Size, TGLBFUN) == 1)
				goto _OUT;
		}
	}
_OUT:
	fclose(fp);
	return ret;
}

/*************************************************************************
* DESCRIPTION
*	紧缩及计算每个符号大小
*
* PARAMETERS
*	DoCalcSz: 需要计算符号大小
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ProcessKelogSyms(int DoCalcSz)
{
	KALLSYM_UNIT *pAry, *pEKaSym;
	KALLSYM_SF *pKaSymSf;
	TADDR Addr;
	TSIZE Size;
	char *pStrs;

	for (pKaSymSf = pKallSymSf; pKaSymSf; pKaSymSf = pKaSymSf->pNext) {
		if (!pKaSymSf->KSymCnt)
			continue;
		pStrs = Realloc(pKaSymSf->pKSymStrs, pKaSymSf->KSymIdx);
		if (pStrs) {
			pKaSymSf->pKSymStrs = pStrs;
			pKaSymSf->KSymAllocSz = pKaSymSf->KSymIdx;
		}
		pAry  = Realloc(pKaSymSf->pKSymAry, sizeof(*pAry) * pKaSymSf->KSymCnt);
		if (pAry) {
			pKaSymSf->pKSymAry = pAry;
			pKaSymSf->KSymAllocCnt = pKaSymSf->KSymCnt;
		}
		if (DoCalcSz) {
			pAry = pKaSymSf->pKSymAry;
			pKaSymSf->LoadBias = pAry[0].Addr;
			pEKaSym = &pAry[pKaSymSf->KSymCnt - 1];
			pEKaSym->Size = Size = 4; /* 末尾固定为4?? */
			for (Addr = pEKaSym->Addr; --pEKaSym >= pAry; ) {
				if (pEKaSym->Addr != Addr) {
					Size = Addr - pEKaSym->Addr;
					if (Size > SYM_MAX_SZ)
						Size = SYM_MAX_SZ;
					Addr = pEKaSym->Addr;
				}
				pEKaSym->Size = Size;
			}
		}
#if defined(_DEBUG) && 0
		{
			FILE *fp;
			char FileName[50];

			FileName[sizeof(FileName) - 1] = '\0';
			snprintf(FileName, sizeof(FileName) - 1, "%s_sym.txt", pKaSymSf->ModName);
			fp = fopen(FileName, "w");
			if (fp) {
				pAry = pKaSymSf->pKSymAry;
				for (pEKaSym = &pAry[pKaSymSf->KSymCnt]; pAry < pEKaSym; pAry++) {
					LOGC(fp, TADDRFMT" "TADDRFMT" %04d %s\n", pAry->Addr, pAry->Op, pAry->Size, pAry->NameIdx + pKaSymSf->pKSymStrs);
				}
				fclose(fp);
			}
		}
#endif
#ifdef _DEBUG
		pAry = &pKaSymSf->pKSymAry[1];
		for (pEKaSym = &pKaSymSf->pKSymAry[pKaSymSf->KSymCnt]; pAry < pEKaSym; pAry++) {
			if (pAry->Addr < (pAry - 1)->Addr) {
				__SSTERR(RS_ERROR);
				LOGC(stderr, "%s: "TADDRFMT", "TADDRFMT"不排序\n", pKaSymSf->ModName, pAry->Addr, (pAry - 1)->Addr);
			}
		}
#endif
	}
}

/*************************************************************************
* DESCRIPTION
*	加载kelog符号信息
*
* PARAMETERS
*	无
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void LoadKelogSyms(void)
{
	static int Flag;

	if (Flag) /* 只需执行一遍 */
		return;
	if (LoadKallSymsFile() == 2) {
		Flag = 1;
		if (LoadKelogSym("SYS_KERNEL_LOG") == 2)
			LoadKelogSym("SYS_LAST_KMSG");
		LoadKelogSym("__exp_main.txt");
	} else {
		Flag = 2;
	}
	ProcessKelogSyms(Flag == 2);
}

/*************************************************************************
* DESCRIPTION
*	查找指定名称的符号信息
*
* PARAMETERS
*	s: 符号信息
*	pName: 符号名
*	pArg: 名称
*
* RETURNS
*	0: 继续
*	1: 结束
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindSymByName(ELF_SYM *s, const char *pName, void *pArg)
{
	(void)s;
	return !strcmp(pName, pArg);
}

/*************************************************************************
* DESCRIPTION
*	查找指定地址的符号信息
*
* PARAMETERS
*	s: 符号信息
*	pName: 符号名
*	pArg: 地址
*
* RETURNS
*	0: 继续
*	1: 结束
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindSymByAddr(ELF_SYM *s, const char *pName, void *pArg)
{
	(void)pName;
#ifdef SST64
	return (s->st_value <= *(TADDR *)pArg && s->st_value + s->st_size > *(TADDR *)pArg);
#else
	return (s->st_value <= (uintptr_t)pArg && s->st_value + s->st_size > (uintptr_t)pArg);
#endif
}

/*************************************************************************
* DESCRIPTION
*	遍历调试符号表
*
* PARAMETERS
*	pSymf: sym句柄
*	pFunc: 回调函数
*		s: 符号信息
*		pName: 符号名
*		pArg: 参数
*	pArg: 参数
*	Op: 查询属性: OP*
*
* RETURNS
*	NULL: 无符号
*	其他: 符合的符号信息
*
* LOCAL AFFECTED
*
*************************************************************************/
static ELF_SYM *ForEachDbgSym(const SYM_INT *pSymf, int (*pFunc)(ELF_SYM *s, const char *pName, void *pArg), void *pArg, unsigned Op)
{
	const char * const pStrs = pSymf->pSymStr;
	ELF_SYM *pSym, *pESym;

	if (Op&FLAG_TRANS(pSymf->Ex.Flag, SYM_LCLSYM, OPB_GLOBAL)) { /* 全局表 */
		for (pSym = pSymf->pSGSym, pESym = pSymf->pEGSym; pSym < pESym; pSym++) {
			if (STT_IN_OPT(pSym->st_info, Op) && pSym->st_shndx != SHN_UNDEF && pFunc(pSym, pSym->st_name + pStrs, pArg))
				return pSym;
		}
	}
	if (Op&FLAG_TRANS(pSymf->Ex.Flag, SYM_LCLSYM, OPB_LOCAL)) { /* 局部表 */
		for (pSym = pSymf->pSLSym, pESym = pSymf->pSGSym; ++pSym < pESym; ) {
			if (STT_IN_OPT(pSym->st_info, Op) && pSym->st_shndx != SHN_UNDEF && pFunc(pSym, pSym->st_name + pStrs, pArg))
				return pSym;
		}
	}
	return NULL;
}

/*============================== vmlinux ==============================*/
/*************************************************************************
* DESCRIPTION
*	根据符号名称获取地址
*
* PARAMETERS
*	pSymf: sym句柄
*	pSize: 不为NULL时返回符号大小(字节), 0表示不确定
*	pName: 符号名
*	Op: 查询属性: OP*
*
* RETURNS
*	0: 失败/无该符号
 *	其他: 地址, 如果是函数则包含THUMB bit (返回根据LoadBias修正后的虚拟地址？)
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR VmName2Addr(const SYM_INT *pSymf, TSIZE *pSize, const char *pName, unsigned Op)
{
	ELF_SYM *s;

	s = ForEachDbgSym(pSymf, FindSymByName, (void *)pName, Op);
	if (s) {
		if (pSize)
			*pSize = s->st_size;
		return s->st_value + pSymf->Ex.LoadBias;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取所在符号名
*
* PARAMETERS
*	pSymf: sym句柄
*	pAddr: 不为NULL时返回符号地址, 如果是函数则包含THUMB bit
*	Addr: 地址
*	Op: 查询属性: OP*
*
* RETURNS
*	NULL: 失败/无该地址对应的符号
*	其他: 符号名
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *VmAddr2Name(const SYM_INT *pSymf, TADDR *pAddr, TADDR Addr, unsigned Op)
{
#ifdef SST64
#define SYM_A2NARG(x)  (&(x))
#else
#define SYM_A2NARG(x)  ((void *)(uintptr_t)(x))
#endif
	ELF_SYM *s;

	Addr -= pSymf->Ex.LoadBias;
	s = ForEachDbgSym(pSymf, FindSymByAddr, SYM_A2NARG(Addr), Op);
	if (s) {
		if (pAddr)
			*pAddr = s->st_value + pSymf->Ex.LoadBias;
		return s->st_name + pSymf->pSymStr;
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	用回调函数处理扫描一遍vmlinux符号表
*
* PARAMETERS
*	pSymf: sym句柄
*	pFunc: 回调函数
*		pName: 符号名
*		Val: 值
*		Size: 大小
*		Info: 其他信息(st_info/st_other/st_shndx)
*		pArg: 参数
*	pArg: 参数
*	Flag: 要扫描的符号表
*		bit0: 动态符号表
*		bit1: plt符号表
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int VmForEachSym(const SYM_INT *pSymf
	, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag)
{
	ELF_SYM *pSym = pSymf->pSLSym, * const pESym = pSymf->pEGSym;
	const char * const pSymStr = pSymf->pSymStr;
	const TADDR LoadBias = pSymf->Ex.LoadBias;
	int rt = 0;

	(void)Flag;
	for (; pSym < pESym; pSym++) {
		if (pSym->st_shndx != SHN_UNDEF) {
			rt = pFunc(pSym->st_name + pSymStr, pSym->st_value + LoadBias, pSym->st_size, *(TUINT *)&pSym->st_info, pArg);
			if (rt)
				break;
		}
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	检查vmlinux和ksym符号是否一一匹配
*
* PARAMETERS
*	pSymf: sym句柄
*
* RETURNS
*	0: 匹配
*	1: 不匹配
*	2: 无法确认是否匹配
*
* LOCAL AFFECTED
*
*************************************************************************/
static int VmCmpKSym(const SYM_INT *pSymf)
{
	const TADDR LoadBias = pSymf->Ex.LoadBias;
	TADDR VmAddr, LowAddr, HighAddr, tAddr;
	size_t i, low, high, LeftCnt;
	KALLSYM_UNIT *pKSymAry;
	KALLSYM_SF *pKaSymSf;
	const char *pFName;
	ELF_SYM *s, *t;

	LoadKelogSyms();
	pKaSymSf = GetKaSymfs(pSymf->Ex.pName, 1/* SymFileInit()还会用到!!! */);
	if (!pKaSymSf || !pKaSymSf->KSymCnt)
		return 2;
	pKSymAry = pKaSymSf->pKSymAry;
	LowAddr  = pKSymAry[0].Addr;
	HighAddr = pKSymAry[pKaSymSf->KSymCnt - 1].Addr;
	LeftCnt  = pKaSymSf->KSymCnt;
	for (s = pSymf->pSLSym, t = pSymf->pEGSym; s < t; s++) {
		VmAddr = s->st_value + LoadBias;
		if (VmAddr < LowAddr || VmAddr > HighAddr || pSymf->pSymStr[s->st_name] == '$'/* 剔除mapping symbol */)
			continue;
		high = pKaSymSf->KSymCnt;
		low  = 0;
		while (high - low > 1) { /* 二分查找法 */
			i = low + ((high - low) >> 1);
			if (pKSymAry[i].Addr <= VmAddr)
				low = i;
			else
				high = i;
		}
		tAddr = pKSymAry[low].Addr;
		if (tAddr == VmAddr) {
			pFName = &pSymf->pSymStr[s->st_name];
			i = low;
			do {
				if (!strcmp(pFName, &pKaSymSf->pKSymStrs[pKSymAry[i].NameIdx])) {
					if (!--LeftCnt)
						return 0;
					goto _NEXT;
				}
			} while (i && pKSymAry[--i].Addr == tAddr);
			while (low < pKaSymSf->KSymCnt && pKSymAry[low + 1].Addr == tAddr) {
				low++;
				if (!strcmp(pFName, &pKaSymSf->pKSymStrs[pKSymAry[low].NameIdx])) {
					if (!--LeftCnt)
						return 0;
					goto _NEXT;
				}
			}
		}
_NEXT:;
	}
	SSTERR(RS_SYM_UNMATCH, pSymf->Ex.pName);
	SSTERR(RS_VM_MR_UNMATCHED1, LeftCnt);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	查找linux_banner所在的文件位置
*
* PARAMETERS
*	Flag: PF_*标志
*	FOff: 文件偏移量(字节)
*	Size: 占用文件大小(字节)
*	Va/Pa: 虚拟/物理地址
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
*	2: 成功
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindBannerVm(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg)
{
	TADDR * const pAry = pArg;

	(void)Flag, (void)Pa;
	if (pAry[0] >= Va && pAry[0] + pAry[1] <= Va + Size) {
		pAry[0] = FOff + (pAry[0] - Va);
		return 2;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	在内存中搜索字符串
*
* PARAMETERS
*	pMem: 内存起始地址
*	pStr: 字符串
*	Size: 查找范围大小
*
* RETURNS
*	NULL: 失败
*	其他: 找到的字符串起始地址
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *MemStr(const char *pMem, const char *pStr, int Size)
{
	const char *s1, *s2;
	unsigned Cnt;

	while (Size > 0) {
		s1  = pMem;
		s2  = pStr;
		Cnt = Size;
		do {
			if (!*s2)
				return pMem;
			if (!Cnt--)
				return NULL;
		} while (*s2++ == *s1++);
		pMem++;
		Size--;
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	搜索linux banner
*
* PARAMETERS
*	SAddr/EAddr: [起始地址, 结束地址)
*	Flag: vma标志
*	pArg: buffer, 存放找到的linux banner, 大小必须>=BANNER_SZ
*
* RETURNS
*	0: 继续
*	2: 完成
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindBannerMr(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg)
{
	int Size = (int)(EAddr - SAddr);
	const char *p, *q;
	VMA_HDL *pVmaHdl;
	int rt = 0;

	(void)Flag;
	if (Size <= 0) /* 溢出判断 */
		return 0;
	pVmaHdl = GetVmaHdl(SAddr);
	p = Vma2Mem(pVmaHdl, SAddr, 1);
	if (p && (q = MemStr(p, "Linux version ", Size)) != NULL) {
		Size -= (q - p);
		if (Size > BANNER_SZ)
			Size = BANNER_SZ;
		memcpy(pArg, q, Size);
		rt = 2;
	}
	PutVmaHdl(pVmaHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	检查vmlinux是否匹配
*
* PARAMETERS
*	pSymf: sym句柄
*	pElfHdl: elf句柄
*
* RETURNS
*	0: 匹配
*	1: 版本不对、文件格式错误或文件残缺
*	2: 无法确认是否匹配
*
* LOCAL AFFECTED
*
*************************************************************************/
static int VmlinuxCheck(const SYM_INT *pSymf, ELF_HDL *pElfHdl)
{
	TADDR Banner[2], BannerAddr;
	const char *pBanner = NULL;
	char Buf[BANNER_SZ];
	TSIZE FOff;
	int rt = 2;
	FILE *fp;

	if (!ElfGetSecInf(pElfHdl, &FOff, &FOff, SHT_PROGBITS, ".debug_line")) {
		SSTERR(RS_NO_DBGINF, pSymf->Ex.pName);
		return 1;
	}
	BannerAddr = VmName2Addr(pSymf, &Banner[1], "linux_banner", OPT_OBJECT|OPB_GLOBAL);
	if (!BannerAddr)
		goto _NO_BANNER;
	Banner[0] = BannerAddr - pSymf->Ex.LoadBias;
	if (ElfForEachLoad(pElfHdl, FindBannerVm, Banner) != 2)
		goto _NO_BANNER;
	if (sizeof(Buf) < Banner[1])
		goto _OUT;
	pBanner = FpMapRo(pSymf->pFpHdl, Banner[0], Banner[1]);
	if (!pBanner)
		goto _MAP_LEAK;
	fp = fopen("SYS_VERSION_INFO", "r");
	if (fp) {
		if (fgets(Buf, sizeof(Buf), fp))
			rt = !!memcmp(pBanner, Buf, (size_t)Banner[1]);
		fclose(fp);
		if (rt != 2)
			goto _OUT;
	}
	if (!DumpMem(BannerAddr, Buf, Banner[1]) && (rt = memcmp(pBanner, Buf, (size_t)Banner[1])) == 0)
		goto _OUT;
	if (ForEachVmaAll(FindBannerMr, Buf) != 2) {
		rt = VmCmpKSym(pSymf);
	} else {
		rt = !!memcmp(pBanner, Buf, (size_t)Banner[1]);
_OUT:   if (rt) {
			SSTERR(RS_SYM_UNMATCH, pSymf->Ex.pName);
			LOGC(stderr, "mrdump : %s%s: %s", Buf, pSymf->Ex.pName, pBanner);
		}
	}
	FpUnmap(pSymf->pFpHdl, pBanner, Banner[1]);
	return rt;
_NO_BANNER:
	SSTERR(RS_NO_VAR, "linux_banner");
	return 1;
_MAP_LEAK:
	SSTERR(RS_FILE_MAP, pSymf->Ex.pName, "linux_banner");
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	建立代码区域信息
*
* PARAMETERS
*	pSecName: 节名称
*	Flag: PF_*标志
*	FOff: 文件偏移量(字节)
*	Size: 占用文件大小(字节)
*	Addr: 虚拟地址
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int BuildVmRegion(const char *pSecName, TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Addr, void *pArg)
{
	SYM_INT * const pSymf = pArg;
	VM_REG * const pPReg = &pSymf->Regs[pSymf->RCnt - 1];

	if (!strncmp(pSecName, ".init", sizeof(".init") - 1) /* 去除.init.text/.init.data */
	  || !strncmp(pSecName, ".exit", sizeof(".exit") - 1) /* 去除.exit.text */
#ifdef SST64
	  || !strncmp(pSecName, ".altinstr", sizeof(".altinstr") - 1) /* 去除.altinstructions/.altinstr_replacement */
#else
	  || !strcmp(pSecName, ".vectors") || !strcmp(pSecName, ".stubs") /* kernel会搬移到0xFFFF0000上, 并且做0xe7fddef1格式化 */
#endif
	  )
		return 0;
	Addr += pSymf->Ex.LoadBias;
	if (pSymf->RCnt && Addr <= pPReg->VEnd && Addr >= pPReg->VAddr
	  && pPReg->Attr == Flag && (!FOff || Addr - pPReg->VAddr ==  FOff - pPReg->FOff)) { /* 重叠合并 */
		pPReg->VREnd = Addr + Size;
		pPReg->VEnd = (Addr + Size + PAGE_SIZE - 1)&PAGE_MASK;
		return 0;
	}
	pSymf->Regs[pSymf->RCnt].FOff  = FOff - (Addr&~PAGE_MASK);
	pSymf->Regs[pSymf->RCnt].VAddr = Addr&PAGE_MASK;
	pSymf->Regs[pSymf->RCnt].VREnd = Addr + Size;
	pSymf->Regs[pSymf->RCnt].VEnd  = (Addr + Size + PAGE_SIZE - 1)&PAGE_MASK;
	pSymf->Regs[pSymf->RCnt].Attr  = Flag;
	if (++pSymf->RCnt >= ARY_CNT(pSymf->Regs)) {
		SSTWARN(RS_VM_OVERFLOW);
		return 1;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	输出不匹配区域信息到文件
*
* PARAMETERS
*	pDifArg: dif结构体
*	p1/p2: buffer
*	VAddr: 起始虚拟地址
*	Cnt: 单位: 字
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispDifRegion(VM_DIFARG *pDifArg, const unsigned *p1, const unsigned *p2, TADDR VAddr, size_t Cnt)
{
#define DISP_LEN	64 /* 单位: 字 */
	const char *pName;
	TADDR Addr;
	size_t i;

	if (pDifArg->Cnt < 2) {
		if (pDifArg->Cnt&1) {
			LOG(pDifArg->fp, RS_DEMAGED1);
			SSTERR(RS_DEMAGED1);
		} else {
			SSTERR(RS_SYM_UNMATCH, pDifArg->pSymf->Ex.pName);
		}
	}
	pDifArg->Cnt += 2;
	LOG(pDifArg->fp, RS_DEMAGED_REG, pDifArg->Cnt / 2, VAddr);
	if ((pName = pDifArg->pSymf->Addr2Name(pDifArg->pSymf, &Addr, VAddr, TALLFUN)) != NULL)
		LOGC(pDifArg->fp, "<%s() + %d>", pName, (size_t)(VAddr - Addr));
	else if ((pName = pDifArg->pSymf->Addr2Name(pDifArg->pSymf, &Addr, VAddr, TALLVAR)) != NULL)
		LOGC(pDifArg->fp, "<%s + %d>", pName, (size_t)(VAddr - Addr));
	if (Cnt > DISP_LEN)
		Cnt = DISP_LEN;
	LOGC(pDifArg->fp, " ++ %d:\n%s:", Cnt * sizeof(*p1), pDifArg->pSymf->Ex.pName);
	for (i = 0; i < Cnt; i++) {
		if (!(i&0x07))
			LOGC(pDifArg->fp, "\n   ");
		LOGC(pDifArg->fp, " %08X", p1[i]);
	}
	LOGC(pDifArg->fp, "\nmrdump:");
	for (i = 0; i < Cnt; i++) {
		if (!(i&0x07))
			LOGC(pDifArg->fp, "\n   ");
		LOGC(pDifArg->fp, " %08X", p2[i]);
	}
	LOGC(pDifArg->fp, "\n\n");
}

/*************************************************************************
* DESCRIPTION
*	对比重叠区域数据, 不一致则举报
*
* PARAMETERS
*	Addr/Size: 重叠地址和大小(字节)
*	pBuf1/pBuf2: 2个buffer
*	pArg: 参数
*
* RETURNS
*	0: 正常
*	1: 异常
*
* LOCAL AFFECTED
*
*************************************************************************/
static int VmRegionCmp(TADDR Addr, TSIZE Size, const void *pBuf1, const void *pBuf2, void *pArg)
{
#define CRANGE  16 /* 单位: 字 */
	const unsigned * const p = pBuf1, * const q = pBuf2;
	const size_t Cnt = (size_t)(Size / sizeof(*p));
	size_t i, DifStart = 0, DifEnd = 0;
	VM_DIFARG * const pDifArg = pArg;
	TADDR tAddr;
	int j;

	if (!(pBuf1 && pBuf2))
		return 0;
	for (i = 0; i < Cnt; i++) {
		if (p[i] == q[i])
			continue;
#ifdef SST64
		if (p[i] == 0xD503201F) /* nop, ARM64_LSE_ATOMIC_INSN */
			continue;
		if (!(i&1)/* 8字节对齐 */ && *(TSIZE *)(p + i) + pDifArg->LoadBias == *(TSIZE *)(q + i)) { /* 重定位: R_AARCH64_RELATIVE */
			i++;
			continue;
		}
#else
		if (MCntInstSize && (p[i] >> 24) == 0xEB /* BL指令 */ && i && (p[i - 1] == 0xE92D4000 || p[i - 1] == 0xE52DE004) /* PUSH {LR} */
			&& (q[i] == 0xE8BD4000 || (q[i] >> 24) == 0xEB)) /* 跳过mcount自修改指令 */
			continue;
		if ((p[i]&0xFFFF0FFF) == 0xE3A00081 || (p[i]&0xFFF00FFF) == 0xE2900481 || (p[i]&0xFFF00FFF) == 0xE2400481) /* __pv_stub_mov_hi()/__pv_add_carry_stub()/__pv_stub((unsigned long) x, t, "sub", __PV_BITS_31_24) */
			continue;
		if (q[i] == 0x00717269 && p[i] == 0x3A717269) /* "irq:" => "irq" event_trace_init() -> ftrace_set_clr_event() -> strsep() */
			continue;
#endif
		tAddr = Addr + i * sizeof(*p);
		if (tAddr >= pDifArg->EAddr)
			break;
		for (j = pDifArg->IgnoreCnt; --j >= 0; ) {
			if (tAddr >= pDifArg->IgnoreSAddrs[j] && tAddr < pDifArg->IgnoreEAddrs[j]) {
				i = (size_t)((pDifArg->IgnoreEAddrs[j] - Addr) / sizeof(*p)) - 1;
				break;
			}
		}
		if (j >= 0) /* 忽略 */
			continue;
		if (DifEnd <= i) {
			if (DifEnd)
				DispDifRegion(pDifArg, &p[DifStart], &q[DifStart], Addr + DifStart * sizeof(*p), DifEnd - (DifStart + CRANGE - 1));
			DifStart = i;
		}
		DifEnd = i + CRANGE;
	}
	if (DifEnd)
		DispDifRegion(pDifArg, &p[DifStart], &q[DifStart], Addr + DifStart * sizeof(*p), DifEnd - (DifStart + CRANGE - 1));
	pDifArg->IsMatched = 1;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	将vmlinux叠加到内存层
*
* PARAMETERS
*	pSymf: sym句柄
*	CoreCnt: CPU核数
*	UnChk: 无法确认vmlinux是否匹配
*	fp: 异常输出文件句柄
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MergeVmCode(SYM_INT *pSymf, TUINT CoreCnt, int UnChk, FILE *fp)
{
	VM_DIFARG DifArg;
	VM_REG *pReg;
	size_t i;
	TSIZE Size;
	int rt;

	(void)CoreCnt;
	if (VmName2Addr(pSymf, NULL, "__gnu_mcount_nc", TGLBFUN))
		MCntInstSize = 2 * 4;
	DifArg.fp = fp;
	DifArg.pSymf = pSymf;
	DifArg.Cnt = !UnChk;
	DifArg.IsMatched = 0;
	DifArg.IgnoreCnt = 0;
	DifArg.LoadBias = pSymf->Ex.LoadBias;
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "handle_arch_irq", OPT_NOTYPE|OPB_GLOBAL);
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt]) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + sizeof(TADDR);
		DifArg.IgnoreCnt++;
	}
#ifdef SST64
	/* altinstr */
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "flush_icache_range", TGLBFUN) + 0x18;
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] != 0x18) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 8; /* kernel4.9在0x1C */
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "__dma_clean_range", TLCLFUN) + 0x18;
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] != 0x18) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 4;
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "ret_to_user", TLCLFUN) + 0x30;
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] != 0x30) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 40;
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "ret_fast_syscall", OPT_NOTYPE|OPB_LOCAL) + 0x3C;
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] != 0x3C) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 32;
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "__clean_dcache_area_pou", TGLBFUN) + 0x1C;
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] != 0x1C) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 8; /* kernel4.9在0x20 */
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "__pi___clean_dcache_area_poc", TGLBFUN) + 0x20;
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] != 0x20) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 4;
		DifArg.IgnoreCnt++;
	}
#else
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "__v7_setup_stack", OPT_NOTYPE|OPB_LOCAL);
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt]) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 4 * 11;
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "new_stack_0", OPT_NOTYPE|OPB_LOCAL);
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt]) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + CoreCnt * 1028;
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "__aeabi_idiv", TGLBFUN); /* 可能被patch_aeabi_idiv()修改 */
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt]) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 8;
		DifArg.IgnoreCnt++;
	}
	DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] = VmName2Addr(pSymf, NULL, "__udivsi3", TGLBFUN); /* 可能被patch_aeabi_idiv()修改 */
	if (DifArg.IgnoreSAddrs[DifArg.IgnoreCnt]) {
		DifArg.IgnoreEAddrs[DifArg.IgnoreCnt] = DifArg.IgnoreSAddrs[DifArg.IgnoreCnt] + 8;
		DifArg.IgnoreCnt++;
	}
#endif
	if (DifArg.IgnoreCnt > ARY_CNT(DifArg.IgnoreSAddrs))
		DBGERR("IgnoreAddrs数组溢出\n");
	for (i = 0; i < pSymf->RCnt; i++) {
		pReg = &pSymf->Regs[i];
		if (pReg->Attr&PF_W) /* 跳过可写区域 */
			continue;
		Size = pReg->VEnd - pReg->VAddr;
		pReg->pBuf = FpMapRo(pSymf->pFpHdl, pReg->FOff, Size);
		if (!pReg->pBuf)
			continue;
		DifArg.EAddr = pReg->VREnd;
		rt = CreateMemRegion(pReg->VAddr, Size, VMA_R|FLAG_TRANS(pReg->Attr, PF_X, VMA_X), 0, pReg->pBuf, VmRegionCmp, &DifArg);
		if (rt == 2) {
			FpUnmap(pSymf->pFpHdl, pReg->pBuf, Size);
			pReg->pBuf = NULL;
		} else if (rt) {
			return 1;
		}
	}
	if (DifArg.Cnt > 1)
		return 0; /* 不再限制不一致的情况, 有可能故意这样，然后用MMU保护 */
	if (UnChk && !DifArg.IsMatched)
		SSTWARN(RS_MATCH_ORNOT, pSymf->Ex.pName);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	将sym句柄关联到vma句柄
*
* PARAMETERS
*	pSymf: sym句柄
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int VmBind2Vma(SYM_INT *pSymf)
{
	BIND_ARG BindArg;
	VM_REG *pReg;
	size_t i;

	BindArg.pSymf = pSymf;
	for (i = 0; i < pSymf->RCnt; i++) {
		pReg = &pSymf->Regs[i];
		BindArg.Attr = VMA_R|FLAG_TRANS(pReg->Attr, PF_W, VMA_W)|FLAG_TRANS(pReg->Attr, PF_X, VMA_X);
		if (BuildSymBgMap(pReg->VAddr, pReg->VEnd, BindArg.Attr))
			return 1;
		TryMergeVma(pReg->VAddr, pReg->VEnd, BindSym2Vma, &BindArg);
	}
	if (LastSymfAddr)
		return 0;
	SSTERR(RS_NO_LOAD, pSymf->Ex.pName, "RO");
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	加载sym文件的调试信息
*
* PARAMETERS
*	pSymf: sym句柄
*	pElfHdl: elf句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void LoadDebugInf(SYM_INT *pSymf, ELF_HDL *pElfHdl)
{
	TSIZE FOff, FOff2, StrsFOff, RgeFOff, LocFOff, LineFOff, ARgeFOff, AbbSz, StrsSz, RgeSz, LocSz, LineSz, ARgeSz, Size;
	TUINT GlbIdx;
	TADDR Addr;

	Size = ElfGetSecInf(pElfHdl, &Addr, &FOff, SHT_PROGBITS, ".debug_frame");
	if (Size)
		pSymf->pDwfHdl = GetDwfHdl(pSymf->pFpHdl, FOff, Size);
	Size = ElfGetSecInf(pElfHdl, &Addr, &FOff, SHT_PROGBITS, ".debug_info");
	AbbSz = ElfGetSecInf(pElfHdl, &Addr, &FOff2, SHT_PROGBITS, ".debug_abbrev");
	if (Size && AbbSz) {
		StrsSz = ElfGetSecInf(pElfHdl, &Addr, &StrsFOff, SHT_PROGBITS, ".debug_str");
		RgeSz = ElfGetSecInf(pElfHdl, &Addr, &RgeFOff, SHT_PROGBITS, ".debug_ranges");
		LocSz = ElfGetSecInf(pElfHdl, &Addr, &LocFOff, SHT_PROGBITS, ".debug_loc");
		LineSz = ElfGetSecInf(pElfHdl, &Addr, &LineFOff, SHT_PROGBITS, ".debug_line");
		ARgeSz = ElfGetSecInf(pElfHdl, &Addr, &ARgeFOff, SHT_PROGBITS, ".debug_aranges");
		pSymf->pDwiHdl = GetDwiHdl(pSymf->pFpHdl, FOff, Size, FOff2, AbbSz, StrsFOff, StrsSz, RgeFOff, RgeSz
		  , LocFOff, LocSz, LineFOff, LineSz, ARgeFOff, ARgeSz);
	}
	Size = ElfGetSymInf(pElfHdl, &FOff, &FOff2, &StrsSz, &GlbIdx);
	if (Size) {
		pSymf->pSLSym = FpMapRo(pSymf->pFpHdl, FOff, Size);
		if (pSymf->pSLSym) {
			pSymf->pSGSym   = &pSymf->pSLSym[GlbIdx];
			pSymf->pEGSym   = (ELF_SYM *)((char *)pSymf->pSLSym + Size);
			pSymf->SymStrSz = StrsSz;
			pSymf->pSymStr  = FpMapRo(pSymf->pFpHdl, FOff2, pSymf->SymStrSz);
			if (pSymf->pSymStr) {
				pSymf->Ex.Flag |= SYM_LCLSYM;
			} else {
				FpUnmap(pSymf->pFpHdl, pSymf->pSLSym, Size);
				pSymf->pSLSym = pSymf->pSGSym = pSymf->pEGSym = NULL;
				SSTWARN(RS_FILE_MAP, pSymf->Ex.pName, ".strtab");
			}
		} else {
			SSTWARN(RS_FILE_MAP, pSymf->Ex.pName, ".symtab");
		}
	}
}

/*************************************************************************
* DESCRIPTION
 *	初始化vmlinux文件模块, 和地址信息, 以及fpHdl
*
* PARAMETERS
*	CoreCnt: CPU核数
*	CodeStart: kernel代码起始地址, 0表示未知
*	fp: 异常输出文件句柄
*
* RETURNS
*	0: 成功
*	1: 失败
*	2: 无该文件
*
* LOCAL AFFECTED
*
*************************************************************************/
static int VmlinuxInit(TUINT CoreCnt, TADDR CodeStart, FILE *fp)
{
	SYM_INT * const pSymf = (SYM_INT *)VmsBuf;
	ELF_HDL *pElfHdl;
	FP_HDL *pFpHdl;
	TADDR Addr;
	TSIZE FOff;
	int rt = 1;

	pFpHdl = GetFpHdl(DEF_VMLINUX, 1);
	if (!pFpHdl)
		return 2;
	memset(pSymf, 0, sizeof(*pSymf));
	pSymf->Ex.pName		= DEF_VMLINUX;
	pSymf->pFpHdl		= pFpHdl;
	pSymf->Ex.Flag		= SYM_LCLSYM|SYM_NOTICE|SYM_LOAD|SYM_MATCH;
	pSymf->Name2Addr	= VmName2Addr;
	pSymf->Addr2Name	= VmAddr2Name;
	pSymf->ForEachSym	= VmForEachSym;
	pVmSHdl = MOD_HDL(pSymf);
	pElfHdl = GetElfHdl(pFpHdl, ET_EXEC | ET_DYN/* 开KASLR似乎变成了DYN?? */);/*vmlinux elf Header*/
	if (!pElfHdl)
		goto _OUT;
	if (CodeStart) {
		if (ElfGetSecInf(pElfHdl, &Addr, &FOff, SHT_PROGBITS, ".head.text"))
			pSymf->Ex.LoadBias = CodeStart - Addr; //CodeStart 对齐.head.text段，LoadBias偏移量 : mrdump - Addr:vmlinux   load偏移地址
		else
			SSTWARN(RS_NO_SEC, pSymf->Ex.pName, ".head.text");
	}
	LoadDebugInf(pSymf, pElfHdl); /* VmlinuxCheck()需要symbol, 因此要优先执行,load .debug_info .debug_frame .debug_*** */
	rt = VmlinuxCheck(pSymf, pElfHdl); /*check vmlinux 是否匹配 ignore*/
	if (rt == 1)
		goto _OUT;
	ElfForEachSec(pElfHdl, BuildVmRegion, pSymf);
	rt = MergeVmCode(pSymf, CoreCnt, rt == 2, fp);
	if (!rt)
		rt = VmBind2Vma(pSymf);
_OUT:
	if (rt)
		FreeSymHdl(pVmSHdl, NULL);
	if (pElfHdl)
		PutElfHdl(pElfHdl);
	return rt;
}

/*============================== ksym ==============================*/
/*************************************************************************
* DESCRIPTION
*	根据符号名称位置解码符号名称
*
* PARAMETERS
*	pSymfKs: ksym结构体
*	Off: 符号名称位置
*	pName: 返回解码的符号名称, 第1个字节包含类型
*	NameSz: pName长度
*
* RETURNS
*	下一个符号名称位置
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static unsigned ExpandKSym(const SYMF_KS *pSymfKs, unsigned Off, char *pName, unsigned NameSz)
{
	const unsigned char *p = &pSymfKs->pKSymStrs[Off];
	const char *pStr;
	unsigned i;

	for (i = 0, Off += ((*p++) + 1); p < &pSymfKs->pKSymStrs[Off]; p++) {
		pStr = &pSymfKs->pKSymTTbl[pSymfKs->pKSymTIdx[*p]];
		do {
			pName[i++] = *pStr++;
			if (i == NameSz) {
				pName[NameSz - 1] = '\0';
				return Off;
			}
		} while (*pStr);
	}
	pName[i] = '\0';
	return Off;
}

/*************************************************************************
* DESCRIPTION
*	通过符号名称获取ksym符号地址和大小
*
* PARAMETERS
*	pSymf: sym句柄
*	pSize: 不为NULL时返回符号大小(字节), 0表示不确定
*	pName: 符号名
*	Op: 查询属性: OP*
*
* RETURNS
*	0: 失败/无该符号
*	其他: 地址, 如果是函数则包含THUMB bit
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR KsName2Addr(const SYM_INT *pSymf, TSIZE *pSize, const char *pName, unsigned Op)
{
	const SYMF_KS * const pSymfKs = SYMF_INT(KS, pSymf);
	const unsigned char *p, *pKSymStrs;
	unsigned tOp, i, j;
	const char *pStr;
	TADDR Addr;

	for (pKSymStrs = pSymfKs->pKSymStrs, i = 0; i < pSymfKs->KSymCnt; i++) {
		p = pKSymStrs + 1;
		pKSymStrs += (*pKSymStrs + 1);
		pStr = &pSymfKs->pKSymTTbl[pSymfKs->pKSymTIdx[*p]];
		tOp = KSymType2Op(*pStr++)&Op; /* 获取符号类型 */
		if (!OPB_MASK(tOp) || !OPT_MASK(tOp)) /* 不满足Op属性 */
			continue;
		for (j = 0; p < pKSymStrs; pStr = &pSymfKs->pKSymTTbl[pSymfKs->pKSymTIdx[*++p]]) {
			while (*pStr) {
				if (pName[j++] != *pStr++)
					goto _NEXT;
			}
		}
		if (pName[j] == '\0')
			goto _FOUND;
_NEXT:;
	}
	return 0;
_FOUND:
	Addr = pSymfKs->pKSymAddrs[i];
	if (!pSize)
		return Addr;
	*pSize = 0;
	if (!(tOp&OPT_FUNC)) /* 变量 */
		return Addr;
	while (++i < pSymfKs->KSymCnt) { /* 查找下一个不同的地址 */
		if (pSymfKs->pKSymAddrs[i] > Addr) {
			*pSize = pSymfKs->pKSymAddrs[i] - Addr;
			break;
		}
	}
	return Addr;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取所在符号名
*
* PARAMETERS
*	pSymf: sym句柄
*	pAddr: 不为NULL时返回符号地址, 如果是函数则包含THUMB bit
*	Addr: 地址
*	Op: 查询属性: OP*
*
* RETURNS
*	NULL: 失败/无该地址对应的符号
*	其他: 符号名
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *KsAddr2Name(const SYM_INT *pSymf, TADDR *pAddr, TADDR Addr, unsigned Op)
{
	SYMF_KS * const pSymfKs = SYMF_INT(KS, pSymf);
	const unsigned char *pKSymStrs;
	unsigned i, low, high, tOp;
	char Name[KSYM_NAME_LEN];
	TADDR tAddr;

	high = pSymfKs->KSymCnt;
	low  = 0;
	while (high - low > 1) { /* 二分查找法 */
		i = low + ((high - low) >> 1);
		if (pSymfKs->pKSymAddrs[i] <= Addr)
			low = i;
		else
			high = i;
	}
	for (tAddr = pSymfKs->pKSymAddrs[low]; low && pSymfKs->pKSymAddrs[low - 1] == tAddr; low--); /* 查找第一个同地址的符号 */
	for (; low < pSymfKs->KSymCnt && pSymfKs->pKSymAddrs[low] == tAddr; low++) {
		if (!pSymfKs->ppKSymN[low]) { /* 缓存名称 */
			pKSymStrs = &pSymfKs->pKSymStrs[pSymfKs->pKSymMarks[low >> 8]];
			for (i = 0; i < (low&0xFF); i++) {
				pKSymStrs = pKSymStrs + *pKSymStrs + 1;
			}
			ExpandKSym(pSymfKs, pKSymStrs - pSymfKs->pKSymStrs, Name, sizeof(Name)); /* Grab name */
			pSymfKs->ppKSymN[low] = strdup(Name);
			if (!pSymfKs->ppKSymN[low])
				continue;
			pSymfKs->KSymNCnt++;
		}
		tOp = KSymType2Op(*pSymfKs->ppKSymN[low])&Op; /* 获取符号类型 */
		if (OPT_MASK(tOp&Op) && OPB_MASK(tOp&Op)) {
			if (pAddr)
				*pAddr = tAddr;
			return pSymfKs->ppKSymN[low] + 1;
		}
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	用回调函数处理扫描一遍ksym符号表
*
* PARAMETERS
*	pSymf: sym句柄
*	pFunc: 回调函数
*		pName: 符号名
*		Val: 值
*		Size: 大小
*		Info: 其他信息(st_info/st_other/st_shndx)
*		pArg: 参数
*	pArg: 参数
*	Flag: 要扫描的符号表
*		bit0: 动态符号表
*		bit1: plt符号表
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int KsForEachSym(const SYM_INT *pSymf, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag)
{
	static const unsigned char StTAry[] =
	{
		STT_OBJECT, STT_OBJECT, STT_NOTYPE, STT_OBJECT, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, /*a~j*/
		STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_FUNC, /* k~t */
		STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE, STT_NOTYPE /* u~z */
	};
	const SYMF_KS *pSymfKs = SYMF_INT(KS, pSymf);
	char Name[KSYM_NAME_LEN];
	struct _ELF_SYM Sym;
	unsigned i, Off;
	int rt = 0;

	(void)Flag;
	Sym.st_other = 0;//STV_DEFAULT;
	Sym.st_shndx = 1;
	for (i = Off = 0; i < pSymfKs->KSymCnt && !rt; i++) {
		Off = ExpandKSym(pSymfKs, Off, Name, sizeof(Name));
		Sym.st_info = (Name[0] < 'a' ? ELF_ST_INFO(STB_GLOBAL, StTAry[Name[0] - 'A']) : ELF_ST_INFO(STB_LOCAL, StTAry[Name[0] - 'a']));
		rt = pFunc(Name + 1, pSymfKs->pKSymAddrs[i], 0, *(TUINT *)&Sym.st_info, pArg);
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	注销ksym模块
*
* PARAMETERS
*	pSymf: sym句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void KsDeInit(SYM_INT *pSymf)
{
	SYMF_KS * const pSymfKs = SYMF_INT(KS, pSymf);
	unsigned i, KSymNCnt = pSymfKs->KSymNCnt;

	if (KSymNCnt) {
		for (i = 0; ; i++) {
			if (pSymfKs->ppKSymN[i]) {
				Free(pSymfKs->ppKSymN[i]);
				if (!--KSymNCnt)
					break;
			}
		}
	}
	Free(pSymfKs->ppKSymN);
	PutVmaHdl(pSymfKs->pVmaHdl);
}

/*************************************************************************
* DESCRIPTION
*	根据内存块填充页表
*
* PARAMETERS
*	SAddr/EAddr: [起始地址, 结束地址)
*	Flag: vma标志
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 完成
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SearchKallSym(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg)
{
#ifdef SST64
#define ALIGN_SZ	 256 /* .align 8 */
#else
#define ALIGN_SZ	 16 /* .align 4 */
#endif
#define ALIGN_PTR(p) ((void *)(((uintptr_t)(p) + ALIGN_SZ - 1)&~(uintptr_t)(ALIGN_SZ - 1)))
#define KSYM_MIN_CNT 40000 /* 最小ksym个数 */
#define KSYM_MAX_CNT 0x50000 /* 最大ksym个数 */
	KSYM_ARG * const pKSymArg = pArg;
	SYMF_KS * const pSymfKs = pKSymArg->pSymfKs;
	TADDR Addr = pKSymArg->SearchStart;
	const unsigned char *pSymName;
	const TADDR *r, *s, *p, *q;
	VMA_HDL *pVmaHdl;
	size_t i;

	(void)Flag;
	if (EAddr - SAddr < KSYM_MIN_CNT * sizeof(*p))
		return 0;
	if (SAddr < pKSymArg->SearchStart)
		SAddr = pKSymArg->SearchStart;
	pVmaHdl = GetVmaHdl(SAddr);
	if (!pVmaHdl) /* pKSymArg->SearchStart可能超过EAddr */
		return 0;
	p = Vma2Mem(pVmaHdl, SAddr, 1);
	pKSymArg->Off = SAddr - (uintptr_t)p;
	q = (void *)((char *)p + (pVmaHdl->EAddr < pKSymArg->SearchEnd ? pVmaHdl->EAddr : pKSymArg->SearchEnd) - SAddr);
	do {
		if (*p >= Addr) {
			Addr = *p++;
			continue;
		}
		if (Addr > SAddr) {
			for (s = p; p < q && !*p; p++); /* 去除pad 0 */
			if (p == ALIGN_PTR(s) && *p < KSYM_MAX_CNT) {
				for (r = p; ++p < q && !*p; ); /* 去除pad 0 */
				if (p == ALIGN_PTR(r + 1))
					goto _FOUND;
			}
		}
		p += KSYM_MIN_CNT;
		Addr = pKSymArg->SearchStart;
	} while (p < q);
	PutVmaHdl(pVmaHdl);
	return 0;
_FOUND:
	pKSymArg->rt = 0;
	pSymfKs->KSymCnt = *(unsigned *)r; /* 记录KSym个数 */
	pSymfKs->pKSymAddrs = s - pSymfKs->KSymCnt;
	pSymfKs->pKSymStrs = pSymName = (void *)p;
	for (i = 0; i < pSymfKs->KSymCnt; i++) {
		pSymName = pSymName + *pSymName + 1;
	}
	pSymfKs->pKSymMarks = p = ALIGN_PTR(pSymName);
	p += ((pSymfKs->KSymCnt + 0xFF) >> 8);
	p = ALIGN_PTR(p);
	pSymfKs->pKSymTTbl = (void *)p;
	while (*(short *)p) {
		p = (void *)((char *)p + ALIGN_SZ);
	}
	pSymfKs->pKSymTIdx = (void *)(*p ? p : ALIGN_PTR(p + 1));
	pSymfKs->c.Ex.LoadBias = pKSymArg->SearchStart - pSymfKs->pKSymAddrs[0];
	pKSymArg->BssEnd = pSymfKs->pKSymAddrs[pSymfKs->KSymCnt - 1];
#ifndef SST64
	for (i = pSymfKs->KSymCnt; i-- > 0; ) {
		if (pSymfKs->pKSymAddrs[i] < 0xF0000000) { /* 去除sram区域 */
			pKSymArg->BssEnd = pSymfKs->pKSymAddrs[i];
			break;
		}
	}
#endif
#if 0
{
	char Name[KSYM_NAME_LEN];
	unsigned i, Off;
	FILE *fp;

	fp = fopen("KALLSYM", "w");
	for (i = Off = 0; i < pSymfKs->KSymCnt; i++) {
		Off = ExpandKSym(pSymfKs, Off, Name, sizeof(Name));
		fprintf(fp, TADDRFMT" %s\n", pSymfKs->pKSymAddrs[i], Name);
	}
	fclose(fp);
}
#endif
	pKSymArg->CodeEnd = KsName2Addr(&pSymfKs->c, NULL, "_etext", OPT_NOTYPE|OPT_FUNC|TGLBVAR);
	if (pKSymArg->CodeEnd) {
		Addr = KsName2Addr(&pSymfKs->c, NULL, "__start_rodata", OPT_NOTYPE|OPB_GLOBAL);
		if (Addr && Addr < pKSymArg->CodeEnd) { /* 有rodata */
			pKSymArg->RoDataEnd = pKSymArg->CodeEnd;
			pKSymArg->CodeEnd = Addr;
		} else { /* 只有函数 */
			Addr = KsName2Addr(&pSymfKs->c, NULL, "_sinittext", OPT_FUNC|OPB_GLOBAL);
			pKSymArg->RoDataEnd = (Addr > pKSymArg->CodeEnd ? Addr : pKSymArg->CodeEnd);
		}
	} else {
		pKSymArg->rt = 1;
		SSTERR(RS_NO_KSYM, "_etext");
	}
	PutVmaHdl(pVmaHdl);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	将sym句柄关联到vma句柄
*
* PARAMETERS
*	pSymf: sym句柄
*	CodeStart/CodeEnd: [CodeStart, CodeEnd)代码段
*	RoDataEnd: [CodeEnd, RoDataEnd)只读数据段
*	BssEnd: [RoDataEnd, BssEnd)读写数据段
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SfKsBind2Vma(SYM_INT *pSymf, TADDR CodeStart, TADDR CodeEnd, TADDR RoDataEnd, TADDR BssEnd)
{
	BIND_ARG BindArg;

	BindArg.pSymf = pSymf;
	BindArg.Attr = VMA_R|VMA_X;
	if (BuildSymBgMap(CodeStart, CodeEnd, BindArg.Attr))
		return 1;
	TryMergeVma(CodeStart, CodeEnd, BindSym2Vma, &BindArg); /* 代码段 */
	if (CodeEnd < RoDataEnd) {
		BindArg.Attr = VMA_R;
		if (BuildSymBgMap(CodeEnd, RoDataEnd, BindArg.Attr))
			return 1;
		TryMergeVma(CodeEnd, RoDataEnd, BindSym2Vma, &BindArg); /* 只读数据段 */
	}
	if (RoDataEnd < BssEnd) {
		BindArg.Attr = VMA_RW;
		if (BuildSymBgMap(RoDataEnd, BssEnd, BindArg.Attr))
			return 1;
		TryMergeVma(RoDataEnd, BssEnd, BindSym2Vma, &BindArg); /* 读写数据段 */
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	初始化ksym模块
*
* PARAMETERS
*	CodeStart: kernel代码起始地址
*	HasKSym: 有ksym
*
* RETURNS
*	0: 成功
*	1: 失败
*	2: 找不到ksym信息
*
* LOCAL AFFECTED
*
*************************************************************************/
static int KSymInit(TADDR CodeStart, int HasKSym)
{
	SYMF_KS * const pSymfKs = (SYMF_KS *)VmsBuf;
	KSYM_ARG KSymArg;

	memset(pSymfKs, 0, sizeof(*pSymfKs));
	KSymArg.pSymfKs		= pSymfKs;
	KSymArg.SearchStart	= CodeStart; /* 不要改动, 也用于代码起始地址 */
	KSymArg.SearchEnd	= CodeStart + 128 * 1024 * 1024/* 最多搜索128M */;
	KSymArg.rt = 1 + !HasKSym;
	ForEachVma(KSymArg.SearchStart, KSymArg.SearchEnd, SearchKallSym, &KSymArg);
	if (KSymArg.rt) {
		if (HasKSym)
			SSTERR(RS_NO_KSYM, "kallsyms");
		return KSymArg.rt;
	}
	pSymfKs->c.Ex.pName = DEF_VMLINUX;
	SfKsBind2Vma(&pSymfKs->c, CodeStart, KSymArg.CodeEnd, KSymArg.RoDataEnd, KSymArg.BssEnd);
	pSymfKs->pVmaHdl	= GetVmaHdl( (TADDR)((uintptr_t)pSymfKs->pKSymAddrs + KSymArg. Off) );
	pSymfKs->ppKSymN	= Calloc(sizeof(pSymfKs->ppKSymN[0]) * pSymfKs->KSymCnt);
	if (!pSymfKs->ppKSymN) {
		SSTWARN(RS_OOM);
		goto _FAIL;
	}
	pSymfKs->pKSymAddrs	= Vma2Mem(pSymfKs->pVmaHdl, (TADDR)((uintptr_t)pSymfKs->pKSymAddrs + KSymArg.Off), 4);
	pSymfKs->pKSymStrs	= Vma2Mem(pSymfKs->pVmaHdl, (TADDR)((uintptr_t)pSymfKs->pKSymStrs + KSymArg.Off), 4);
	pSymfKs->pKSymMarks	= Vma2Mem(pSymfKs->pVmaHdl, (TADDR)((uintptr_t)pSymfKs->pKSymMarks + KSymArg.Off), 4);
	pSymfKs->pKSymTTbl	= Vma2Mem(pSymfKs->pVmaHdl, (TADDR)((uintptr_t)pSymfKs->pKSymTTbl + KSymArg.Off), 4);
	pSymfKs->pKSymTIdx	= Vma2Mem(pSymfKs->pVmaHdl, (TADDR)((uintptr_t)pSymfKs->pKSymTIdx + KSymArg.Off), 4);
	if (KsName2Addr(&pSymfKs->c, NULL, "__gnu_mcount_nc", TGLBFUN))
		MCntInstSize = 4 * 2;
	pSymfKs->c.Ex.Flag	= SYM_NOTICE|SYM_LOAD;
	pSymfKs->c.Name2Addr	= KsName2Addr;
	pSymfKs->c.Addr2Name	= KsAddr2Name;
	pSymfKs->c.ForEachSym	= KsForEachSym;
	pSymfKs->c.DeInit		= KsDeInit;
	pVmSHdl = MOD_HDL(&pSymfKs->c);
	return 0;
_FAIL:
	KsDeInit(&pSymfKs->c);
	return 1;
}

/*============================== kallsyms ==============================*/
/*************************************************************************
* DESCRIPTION
*	通过符号名称获取kallsyms符号地址和大小
*
* PARAMETERS
*	pSymf: sym句柄
*	pSize: 不为NULL时返回符号大小(字节), 0表示不确定
*	pName: 符号名
*	Op: 查询属性: OP*
*
* RETURNS
*	0: 失败/无该符号
*	其他: 地址, 如果是函数则包含THUMB bit
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR SfName2Addr(const SYM_INT *pSymf, TSIZE *pSize, const char *pName, unsigned Op)
{
	const SYMF_SF * const pSymfSf = SYMF_INT(SF, pSymf);
	KALLSYM_UNIT *pKaSym = pSymfSf->pKSymAry, *pEKaSym = &pSymfSf->pKSymAry[pSymfSf->KSymCnt];
	const char *pKSymStrs = pSymfSf->pKSymStrs;

	for (; pKaSym < pEKaSym; pKaSym++) {
		if (OPT_MASK(Op&pKaSym->Op) && OPB_MASK(Op&pKaSym->Op) && !strcmp(pName, pKaSym->NameIdx + pKSymStrs)) {
			if (pSize)
				*pSize = pKaSym->Size;
			return pKaSym->Addr;
		}
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取所在符号名
*
* PARAMETERS
*	pSymf: sym句柄
*	pAddr: 不为NULL时返回符号地址, 如果是函数则包含THUMB bit
*	Addr: 地址
*	Op: 查询属性: OP*
*
* RETURNS
*	NULL: 失败/无该地址对应的符号
*	其他: 符号名
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *SfAddr2Name(const SYM_INT *pSymf, TADDR *pAddr, TADDR Addr, unsigned Op)
{
	const SYMF_SF * const pSymfSf = SYMF_INT(SF, pSymf);
	KALLSYM_UNIT * const pKSymAry = pSymfSf->pKSymAry;
	unsigned i, low, high;
	TADDR tAddr;

	if (!pKSymAry)
		return NULL;
	/* kallsym符号查找 */
	high = pSymfSf->KSymCnt;
	low  = 0;
	while (high - low > 1) { /* 二分查找法 */
		i = low + ((high - low) >> 1);
		if (pKSymAry[i].Addr <= Addr)
			low = i;
		else
			high = i;
	}
	tAddr = pKSymAry[low].Addr;
	if (tAddr > Addr)
		return NULL;
	for (; low && pKSymAry[low - 1].Addr == tAddr; low--); /* 搜索第1个同地址的符号 */
	for (; low < pSymfSf->KSymCnt && pKSymAry[low].Addr == tAddr; low++) {
		if (tAddr + pKSymAry[low].Size > Addr && OPT_MASK(pKSymAry[low].Op&Op) && OPB_MASK(pKSymAry[low].Op&Op)) {
			if (pAddr)
				*pAddr = tAddr;
			return pKSymAry[low].NameIdx + pSymfSf->pKSymStrs;
		}
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	用回调函数处理扫描一遍kallsyms符号表
*
* PARAMETERS
*	pSymf: sym句柄
*	pFunc: 回调函数
*		pName: 符号名
*		Val: 值
*		Size: 大小
*		Info: 其他信息(st_info/st_other/st_shndx)
*		pArg: 参数
*	pArg: 参数
*	Flag: 要扫描的符号表
*		bit0: 动态符号表
*		bit1: plt符号表
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SfForEachSym(const SYM_INT *pSymf, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag)
{
	const SYMF_SF * const pSymfSf = SYMF_INT(SF, pSymf);
	const KALLSYM_UNIT *pKaSym = pSymfSf->pKSymAry, * const pEKaSym = &pSymfSf->pKSymAry[pSymfSf->KSymCnt];
	const char * const pKSymStrs = pSymfSf->pKSymStrs;
	struct _ELF_SYM Sym;
	int rt = 0;

	(void)Flag;
	Sym.st_other = 0;//STV_DEFAULT;
	Sym.st_shndx = 1;
	for (; pKaSym < pEKaSym && !rt; pKaSym++) {
		Sym.st_info = ELF_ST_INFO(STB_GLOBAL, pKaSym->Op&OPT_FUNC ? STT_FUNC : STT_OBJECT);
		rt = pFunc(pKaSym->NameIdx + pKSymStrs, pKaSym->Addr, pKaSym->Size, *(TUINT *)&Sym.st_info, pArg);
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	注销kallsyms模块
*
* PARAMETERS
*	pSymf: sym句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void SfDeInit(SYM_INT *pSymf)
{
	SYMF_SF * const pSymfSf = SYMF_INT(SF, pSymf);

	Free(pSymfSf->pKSymStrs);
	Free(pSymfSf->pKSymAry);
}

/*************************************************************************
* DESCRIPTION
*	将sym句柄关联到vma句柄
*
* PARAMETERS
*	pSymfSf: sym句柄
*	CodeStart: vmlinux代码起始地址
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SfBind2Vma(SYMF_SF *pSymfSf, TADDR CodeStart)
{
	KALLSYM_UNIT * const pKSymAry = pSymfSf->pKSymAry;
	TADDR tAddr, CodeEnd = 0, RoDataEnd = 0, BssEnd = 0;
	size_t i;

	/* 获取范围 */
	if (pSymfSf->IsFKallSyms) {
		CodeEnd = SfName2Addr(&pSymfSf->c, NULL, "__start_rodata", OPT_NOTYPE|OPB_GLOBAL);
		RoDataEnd = SfName2Addr(&pSymfSf->c, NULL, "_etext", OPT_FUNC|TGLBVAR);
	}
	if (!(CodeEnd && RoDataEnd) && pKSymAry) {
		for (i = 0; i < pSymfSf->KSymCnt; i++) {
			if (!(pKSymAry[i].Op&OPT_FUNC))
				break;
		}
		tAddr = (i ? pKSymAry[i - 1].Addr + pSymfSf->pKSymAry[i - 1].Size : pKSymAry[0].Addr - 1);
		if (CodeEnd < tAddr)
			CodeEnd = tAddr;
		for (; i < pSymfSf->KSymCnt; i++) {
			if (pKSymAry[i].Op&OPT_FUNC)
				break;
		}
		tAddr = (i ? pKSymAry[i - 1].Addr + pSymfSf->pKSymAry[i - 1].Size : pKSymAry[0].Addr - 1);
		if (RoDataEnd < tAddr)
			RoDataEnd = tAddr;
	}
	/* 代码段 */
	CodeEnd = (CodeEnd > CodeStart ? (CodeEnd + PAGE_SIZE - 1)&PAGE_MASK/* 对齐 */ : CodeStart + 0x1400000/* 保守估计20M */);
	if (pSymfSf->KSymCnt) {
		BssEnd = pKSymAry[pSymfSf->KSymCnt - 1].Addr;
#ifndef SST64
		for (i = pSymfSf->KSymCnt; i-- > 0; ) {
			if (pKSymAry[i].Addr < 0xF0000000) { /* 去除sram区域 */
				BssEnd = pKSymAry[i].Addr;
				break;
			}
		}
#endif
	}
	return SfKsBind2Vma(&pSymfSf->c, CodeStart, CodeEnd, RoDataEnd, BssEnd);
}

/*************************************************************************
* DESCRIPTION
*	初始化symbol file模块
*
* PARAMETERS
*	CodeStart: kernel代码起始地址
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SymFileInit(TADDR CodeStart)
{
	SYMF_SF * const pSymfSf = (SYMF_SF *)VmsBuf;
	KALLSYM_SF *pKaSymSf;

	memset(pSymfSf, 0, sizeof(*pSymfSf));
	pSymfSf->c.Ex.pName = DEF_VMLINUX;
	LoadKelogSyms();
	pKaSymSf = GetKaSymfs(DEF_VMLINUX, 0);
	if (pKaSymSf) {
		pSymfSf->KSymCnt	= pKaSymSf->KSymCnt;
		pSymfSf->pKSymAry	= pKaSymSf->pKSymAry;
		pSymfSf->pKSymStrs	= pKaSymSf->pKSymStrs;
		pSymfSf->IsFKallSyms = pKaSymSf->IsFKallSyms;
		/* 释放 */
		pKaSymSf->pKSymAry	= NULL;
		pKaSymSf->pKSymStrs	= NULL;
		FreeKaSymfs(pKaSymSf);
	}
	/* 初始化所需符号 */
	if (SfName2Addr(&pSymfSf->c, NULL, "__gnu_mcount_nc", TGLBFUN))
		MCntInstSize = 4 * 2;
	if (SfBind2Vma(pSymfSf, CodeStart)) {
		SfDeInit(&pSymfSf->c);
		return 1;
	}
	pSymfSf->c.Ex.Flag		= SYM_NOTICE|SYM_LOAD;
	pSymfSf->c.Name2Addr	= SfName2Addr;
	pSymfSf->c.Addr2Name	= SfAddr2Name;
	pSymfSf->c.ForEachSym	= SfForEachSym;
	pSymfSf->c.DeInit		= SfDeInit;
	pVmSHdl = MOD_HDL(&pSymfSf->c);
	return 0;
}

/*============================== module ==============================*/
/*************************************************************************
* DESCRIPTION
*	通过符号名称获取module符号地址和大小
*
* PARAMETERS
*	pSymf: sym句柄
*	pSize: 不为NULL时返回符号大小(字节), 0表示不确定
*	pName: 符号名
*	Op: 查询属性: OP*
*
* RETURNS
*	0: 失败/无该符号
*	其他: 地址, 如果是函数则包含THUMB bit
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR MdName2Addr(const SYM_INT *pSymf, TSIZE *pSize, const char *pName, unsigned Op)
{
	ELF_SYM *s = pSymf->pSGSym, *t = pSymf->pEGSym;
	const char *pSymStr = pSymf->pSymStr, *q;

	q = Op&OPT_FUNC ? "t" : "drb";
	for (; s < t; s++) { /* 符号表搜索 */
		if (!strcmp(s->st_name + pSymStr, pName) && strchr(q, s->st_info)) {
			if (pSize)
				*pSize = s->st_size;
			return s->st_value;
		}
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取所在符号名
*
* PARAMETERS
*	pSymf: sym句柄
*	pAddr: 不为NULL时返回符号地址, 如果是函数则包含THUMB bit
*	Addr: 地址
*	Op: 查询属性: OP*
*
* RETURNS
*	NULL: 失败/无该地址对应的符号
*	其他: 符号名
*
* LOCAL AFFECTED
*
*************************************************************************/
static const char *MdAddr2Name(const SYM_INT *pSymf, TADDR *pAddr, TADDR Addr, unsigned Op)
{
	const char * const pSymStr = pSymf->pSymStr;
	ELF_SYM *s = pSymf->pSGSym, *t = pSymf->pEGSym;
	const char *pTypeStr;

	pTypeStr = (Op&OPT_FUNC ? "t" : "drb");
	for (; s < t; s++) { /* 符号表搜索 */
		if (s->st_value <= Addr && s->st_value + s->st_size > Addr && strchr(pTypeStr, s->st_info)) {
			if (pAddr)
				*pAddr = s->st_value;
			return s->st_name + pSymStr;
		}
	}
	/* module exported符号查找 */
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	用回调函数处理扫描一遍module符号表
*
* PARAMETERS
*	pSymf: sym句柄
*	pFunc: 回调函数
*		pName: 符号名
*		Val: 值
*		Size: 大小
*		Info: 其他信息(st_info/st_other/st_shndx)
*		pArg: 参数
*	pArg: 参数
*	Flag: 要扫描的符号表
*		bit0: 动态符号表
*		bit1: plt符号表
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MdForEachSym(const SYM_INT *pSymf, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag)
{
	ELF_SYM *s = pSymf->pSGSym, *t = pSymf->pEGSym;
	const char * const pSymStr = pSymf->pSymStr;
	struct _ELF_SYM Sym;
	int rt = 0;

	(void)Flag;
	Sym.st_other = 0;//STV_DEFAULT;
	Sym.st_shndx = 1;
	for (; s < t; s++) { /* 符号表搜索 */
		if (s->st_info != 'U' && s->st_info != '?') {
			Sym.st_info = ELF_ST_INFO(STB_GLOBAL, s->st_info == 't' ? STT_FUNC : STT_OBJECT);
			rt = pFunc(s->st_name + pSymStr, s->st_value, s->st_size, *(TUINT *)&Sym.st_info, pArg);
			if (rt)
				break;
		}
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	注销module模块
*
* PARAMETERS
*	pSymf: sym句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void MdDeInit(SYM_INT *pSymf)
{
	SYMF_MD * const pSymfMd = SYMF_INT(MD, pSymf);

	if (pSymfMd->c.Ex.Flag&SYM_MEMSYM) {
		if ((void *)pSymfMd->c.pEGSym != pSymfMd->c.pSymStr)
			Free((void *)pSymfMd->c.pSymStr);
		Free((void *)pSymfMd->c.pSGSym);
	}
}

/*************************************************************************
* DESCRIPTION
*	检查ko module是否匹配
*
* PARAMETERS
*	SAddr/EAddr: [起始地址, 结束地址)
*	Flag: vma标志
*	pArg: 参数
*
* RETURNS
*	0: 匹配
*	1: 版本不对、文件格式错误或文件残缺
*	2: 可能被踩坏或bit反转
*
* LOCAL AFFECTED
*
*************************************************************************/
int MdRegionCmp(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg)
{
	size_t Cnt, i, DifCnt, DifStart = 0, DifEnd = 0;
	VM_DIFARG * const pDifArg = pArg;
	const unsigned *p, *q;
	VMA_HDL *pVmaHdl;
	TSIZE Size;
	int rt = 0;
#ifndef SST64
	ELF_REL * const pRel = (void *)pDifArg->pSymf->Regs[1].pBuf;
	const TADDR Base = SAddr - pDifArg->LoadBias;
	unsigned j = 0;
	TADDR Addr;
#endif

	(void)Flag;
	if (EAddr > pDifArg->pSymf->Regs[0].VREnd)
		EAddr = pDifArg->pSymf->Regs[0].VREnd;
	Size = EAddr - SAddr;
	Cnt = (size_t)(Size / sizeof(*p));
	p = (void *)(pDifArg->pSymf->Regs[0].pBuf + (SAddr - pDifArg->pSymf->Regs[0].VAddr));
	pVmaHdl = GetVmaHdl(SAddr);
	q = Vma2Mem(pVmaHdl, SAddr, Size);
	if (!q)
		goto _OUT;
	for (DifCnt = i = 0; i < Cnt; i++) {
		if (p[i] == q[i])
			continue;
#ifdef SST64
		if (p[i] == 0x94000000/* BL #immed26, R_AARCH64_CALL26 */ || !p[i]/* R_AARCH64_ABS64 */ || p[i] == 0xD503201F/* nop, ARM64_LSE_ATOMIC_INSN */)
			continue;
#else
		Addr = Base + i * sizeof(*p);
		for (; j < pDifArg->IgnoreCnt && Addr > pRel[j].r_offset; j++) {}
		if (j < pDifArg->IgnoreCnt && Addr == pRel[j].r_offset) /* 忽略重定位点 */
			continue;
#endif
		if (DifEnd <= i) {
			if (DifEnd)
				DifCnt++;
			DifStart = i;
		}
		DifEnd = i + CRANGE;
	}
	pDifArg->IsMatched = 1; /* 表示有比较过 */
	if (!DifEnd) /* 完全匹配 */
		goto _OUT;
	rt = 1;
	if (!DifCnt && DifEnd - (DifStart + CRANGE - 1) < 4) { /* 只有1块且小于4个字的内容不对，怀疑是bitflip或踩坏 */
		DispDifRegion(pDifArg, &p[DifStart], &q[DifStart], SAddr + DifStart * sizeof(*p), DifEnd - (DifStart + CRANGE - 1));
		rt = 2;
	}
_OUT:
	PutVmaHdl(pVmaHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	检查ko module是否匹配
*
* PARAMETERS
*	pSymf: sym句柄
*	pElfHdl: elf句柄
*
* RETURNS
*	0: 匹配
*	1: 版本不对、文件格式错误或文件残缺
*	2: 无法确认是否匹配
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MdFileCheck(SYM_INT *pSymf, ELF_HDL *pElfHdl)
{
	TSIZE FOff, Size, Size2;
	VM_DIFARG DifArg;
	TADDR Addr;
	int rt;

	(void)Size2;
	Size = ElfGetSecInf(pElfHdl, &FOff, &FOff, SHT_PROGBITS, ".debug_line");
	if (!Size) {
		SSTERR(RS_NO_DBGINF, pSymf->Ex.pName);
		return 1;
	}
	Size = ElfGetSecInf(pElfHdl, &Addr, &FOff, SHT_PROGBITS, ".text");
	if (!Size) {
		SSTWARN(RS_NO_SEC, pSymf->Ex.pName, ".text");
		return 1;
	}
	pSymf->Regs[0].pBuf = FpMapRo(pSymf->pFpHdl, FOff, Size);
	if (!pSymf->Regs[0].pBuf)
		goto _OOM;
	pSymf->Regs[0].VAddr = pSymf->Ex.LoadBias;
	pSymf->Regs[0].VREnd = pSymf->Ex.LoadBias + Size;
#ifndef SST64
	Size2 = ElfGetSecInf(pElfHdl, &Addr, &FOff, SHT_REL, ".rel.text");
	if (!Size2) {
		FpUnmap(pSymf->pFpHdl, pSymf->Regs[0].pBuf, Size);
		SSTWARN(RS_NO_SEC, pSymf->Ex.pName, ".rel.text");
		return 1;
	}
	pSymf->Regs[1].pBuf = FpMapRo(pSymf->pFpHdl, FOff, Size2);
	if (!pSymf->Regs[1].pBuf) {
		FpUnmap(pSymf->pFpHdl, pSymf->Regs[0].pBuf, Size);
		goto _OOM;
	}
	DifArg.IgnoreCnt = (unsigned)(Size2 / sizeof(ELF_REL));
#endif
	DifArg.fp = stdout;
	DifArg.pSymf = pSymf;
	DifArg.Cnt = 1;
	DifArg.IsMatched = 0;
	DifArg.LoadBias = pSymf->Ex.LoadBias;
	rt = ForEachVma(pSymf->Regs[0].VAddr, pSymf->Regs[0].VREnd, MdRegionCmp, &DifArg);
	FpUnmap(pSymf->pFpHdl, pSymf->Regs[0].pBuf, Size);
#ifndef SST64
	FpUnmap(pSymf->pFpHdl, pSymf->Regs[1].pBuf, Size2);
#endif
	if (rt == 1)
		SSTERR(RS_SYM_UNMATCH, pSymf->Ex.pName);
	else if (!(rt || DifArg.IsMatched))
		SSTWARN(RS_MATCH_ORNOT, pSymf->Ex.pName);
	return rt;
_OOM:
	SSTWARN(RS_OOM);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	加载sym文件
*
* PARAMETERS
*	pSymHdl: sym句柄
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void LoadMdFile(SYM_HDL *pSymHdl)
{
	SYMF_MD *pSymfMd = SYMF_INT(MD, MOD_INT(SYM, pSymHdl));
	int DiffSymStr, rt;
	const char *pSymStr;
	ELF_HDL *pElfHdl;
	ELF_SYM *pSGSym;

	if (pSymfMd->c.Ex.Flag&SYM_LOAD)
		return;
	pSymfMd->c.Ex.Flag |= SYM_LOAD;
	pSymfMd->c.pFpHdl = GetFpHdl(pSymfMd->c.Ex.pName, 1);
	if (!pSymfMd->c.pFpHdl)
		return;
	pElfHdl = GetElfHdl(pSymfMd->c.pFpHdl, ET_REL);
	if (!pElfHdl)
		goto _OUT;
	rt = MdFileCheck(&pSymfMd->c, pElfHdl);
	if (!rt) { /* 已匹配, 加载sym信息 */
		pSymfMd->c.Ex.Flag |= SYM_MATCH;
		pSGSym = pSymfMd->c.pSGSym;
		pSymStr = pSymfMd->c.pSymStr;
		DiffSymStr = (pSymStr != (void *)pSymfMd->c.pEGSym);
		LoadDebugInf(MOD_INT(SYM, pSymHdl), pElfHdl);
		if (pSymfMd->c.pSGSym && pSymfMd->c.pSGSym != pSGSym) {
			if (DiffSymStr)
				Free((void *)pSymStr);
			Free((void *)pSGSym);
			pSymfMd->c.Ex.Flag &= ~SYM_MEMSYM;
			pSymfMd->c.Name2Addr	= VmName2Addr;
		    pSymfMd->c.Addr2Name	= VmAddr2Name;
		    pSymfMd->c.ForEachSym	= VmForEachSym;
		} else {
			pSymfMd->c.pSGSym = pSGSym;
			pSymfMd->c.pSymStr = pSymStr;
		}
	} else if (pSymfMd->c.pFpHdl) { /* 异常则关闭fp句柄 */
		PutFpHdl(pSymfMd->c.pFpHdl);
		pSymfMd->c.pFpHdl = NULL;
	}
_OUT:
	if (pElfHdl)
		PutElfHdl(pElfHdl);
}

/*************************************************************************
* DESCRIPTION
*	获取kallsyms里的module信息
*
* PARAMETERS
*	pSymfMd: module结构体
*	pModName: 模块名称
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MdKallSymsInit(SYMF_MD *pSymfMd, const char *pModName)
{
	KALLSYM_SF *pKaSymSf;
	struct _ELF_SYM *pSym, *pESym;
	KALLSYM_UNIT *pKaSym;

	LoadKelogSyms();
	pKaSymSf = GetKaSymfs(pModName, 0);
	if (!pKaSymSf || !pKaSymSf->KSymCnt) {
		if (pKaSymSf)
			FreeKaSymfs(pKaSymSf);
		return 1;
	}
	/* 转换格式 */
	pSymfMd->c.pSGSym = pSym = Malloc(sizeof(*pSym) * pKaSymSf->KSymCnt);
	if (!pSym) {
		FreeKaSymfs(pKaSymSf);
		return 1;
	}
	pSymfMd->c.pEGSym = pESym = pSym + pKaSymSf->KSymCnt;
	for (pKaSym = pKaSymSf->pKSymAry; pSym < pESym; pSym++, pKaSym++) {
		pSym->st_name  = pKaSym->NameIdx;
		pSym->st_value = pKaSym->Addr;
		pSym->st_size  = pKaSym->Size;
		pSym->st_info  = (pKaSym->Op&OPT_FUNC ? 't' : 'd');
		pSym->st_shndx = 1;
	}
	pSymfMd->c.pSymStr = pKaSymSf->pKSymStrs;
	/* 释放 */
	pKaSymSf->pKSymStrs = NULL;
	FreeKaSymfs(pKaSymSf);
	return 0;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据符号名称获取地址
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pSize: 不为NULL时返回符号大小(字节), 0表示不确定
*	pName: 符号名
*	Op: 查询属性: OP*
*
* RETURNS
*	0: 失败/无该符号
*	其他: 地址, 如果是函数则包含THUMB bit
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR SymName2Addr(SYM_HDL *pSymHdl, TSIZE *pSize, const char *pName, unsigned Op)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);

	return pSymf->Name2Addr(pSymf, pSize, pName, Op);
}

/*************************************************************************
* DESCRIPTION
*	根据文件名获取dbg句柄
*
* PARAMETERS
 *	pSymHdl: sym句柄 eg:vmlinux
 *	pDbgHdl: 返回dbg句柄
*	pFileName: 文件名
*
* RETURNS
*	0: 成功
*	1: 失败/无该dbg信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymFile2DbgHdl(SYM_HDL *pSymHdl, struct _DBG_HDL *pDbgHdl, const char *pFileName)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);

	return pSymf->pDwiHdl ? !Dwi2DieHdlByFile(pSymf->pDwiHdl, (void *)pDbgHdl, pFileName) : 1;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取所在符号名
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pAddr: 不为NULL时返回符号地址, 如果是函数则包含THUMB bit
*	Addr: 地址, 如是thumb函数首地址不包含THUMB bit找不到!!!
*	Op: 查询属性: OP*
*
* RETURNS
*	NULL: 失败/无该地址对应的符号
*	其他: 符号名
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *SymAddr2Name(SYM_HDL *pSymHdl, TADDR *pAddr, TADDR Addr, unsigned Op)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);

	return pSymf->Addr2Name(pSymf, pAddr, Addr, Op);
}

/*************************************************************************
* DESCRIPTION
*	通过地址获取sym句柄对应路径和行号
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pLineNo: 返回行号
*	Addr: 地址
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *SymAddr2Line(SYM_HDL *pSymHdl, TUINT *pLineNo, TADDR Addr)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);

	return (pSymf->pDwiHdl ? DwiAddr2Line(pSymf->pDwiHdl, pLineNo, Addr - pSymf->Ex.LoadBias) : NULL);
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取嵌套函数信息
*
* PARAMETERS
*	pSymHdl: sym句柄
*	Addr: 函数地址
*	pFunc: 回调函数, 按嵌套顺序回调
*		pSymHdl: sym句柄
*		FAddr: 函数首地址
*		pFName: 函数名
*		pDbgHdl: 函数dbg句柄
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
int SymAddr2Func(SYM_HDL *pSymHdl, TADDR Addr
  , int (*pFunc)(SYM_HDL *pSymHdl, TADDR FAddr, const char *pFName, DBG_HDL *pDbgHdl, void *pArg), void *pArg)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);
	DIE_HDL DieHdl;
	TADDR FAddr;
	int rt = 1;

	Addr -= pSymf->Ex.LoadBias;
	if (!pSymf->pDwiHdl || !Dwi2CuDieHdlByAddr(pSymf->pDwiHdl, &DieHdl, Addr))
		return rt;
_NEXT:
	if (!DieGetChildDieHdl(&DieHdl, &DieHdl))
		return rt;
	do {
		FAddr = DieCheckAddr(&DieHdl, Addr);
		if (FAddr) {
			if (DieHdl.Tag == DW_TAG_lexical_block)
				goto _NEXT;
			if (DieHdl.Tag != DW_TAG_subprogram && DieHdl.Tag != DW_TAG_inlined_subroutine) {
				DBGERR("%s: %X fail\n", pSymf->Ex.pName, DieHdl.Tag);
				return rt;
			}
			rt = pFunc(pSymHdl, FAddr + pSymf->Ex.LoadBias, DieGetLinkName(&DieHdl), (void *)&DieHdl, pArg);
			if (rt)
				return rt;
			goto _NEXT;
		}
	} while (DieGetSiblingDieHdl(&DieHdl, &DieHdl));
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	获取函数参数信息
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pDbgHdl: 函数dbg句柄
*	pFunc: 回调函数, 按参数顺序回调
*		pParaName: 函数参数名, 可能为NULL
*		Idx: 参数索引, 从0开始
*		Type: 函数参数类型
*		Val: 函数参数值
*		pArg: 参数
*	pRegAry/RegMask: 寄存器组/寄存器有效位图
*	pCallerRegAry/CParaMask/CParaMask2: 调用函数时寄存器组/参数有效位图(指令推导)/参数有效位图(栈推导), 为NULL则不使用ABI规则推导
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
int SymGetFuncPara(SYM_HDL *pSymHdl, DBG_HDL *pDbgHdl, int (*pFunc)(const char *pParaName, int Idx, BASE_TYPE Type, BASE_VAL Val, void *pArg)
  , const TSIZE *pRegAry, TSIZE RegMask, const TSIZE *pCallerRegAry, TSIZE CParaMask, TSIZE CParaMask2, void *pArg)
{
	const TADDR Addr = pRegAry[REG_PC] - pSymHdl->LoadBias;
	int rt = 0, i = 0, j = 0;
	BASE_VAL Val = {0};
	long long tVal = 0;
	DIE_HDL DieHdl;
	BASE_TYPE Type;

	if (!DieGetChildDieHdl((void *)pDbgHdl, &DieHdl))
		return 1;
	do {
		if (DieHdl.Tag != DW_TAG_formal_parameter) /* 忽略DW_TAG_unspecified_parameters */
			break;
		if (CParaMask&((TSIZE)1 << j)) {
			Type = DieFormatVal(&DieHdl, &Val, &pCallerRegAry[j]);
		} else if ((j >= REG_PNUM && pCallerRegAry && !DumpMem(pCallerRegAry[REG_SP] + (j - REG_PNUM) * sizeof(TSIZE), &tVal, sizeof(tVal)))
		  || !DieGetValByLoc(&DieHdl, &tVal, pRegAry, RegMask, Addr)) {
			Type = DieFormatVal(&DieHdl, &Val, &tVal);
		} else if (CParaMask2&((TSIZE)1 << j)) {
			Type = DieFormatVal(&DieHdl, &Val, &pCallerRegAry[j]);
		} else {
			continue;
		}
#ifndef SST64
		if (Type == CS_I64 || Type == CS_DOUBLE)
			j++;
#endif
		rt = pFunc(DieGetName(&DieHdl), i, Type, Val, pArg);
	} while (i++, j++, !rt && DieGetSiblingDieHdl(&DieHdl, &DieHdl));
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	设置sym句柄的标志
*
* PARAMETERS
*	pSymHdl: sym句柄
*	Flag: 标志
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SymSetFlag(SYM_HDL *pSymHdl, unsigned Flag)
{
	MOD_INT(SYM, pSymHdl)->Ex.Flag |= (Flag&SYM_UMASK);
}

/*************************************************************************
* DESCRIPTION
*	遍历符号表
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pFunc: 回调函数
*		pName: 符号名
*		Val: 值
*		Size: 大小
*		Info: 其他信息(st_info/st_other/st_shndx)
*		pArg: 参数
*	pArg: 参数
*	Flag: 要扫描的符号表
*		bit0: 动态符号表
*		bit1: plt符号表
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymForEachSym(SYM_HDL *pSymHdl, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);

	return pSymf->ForEachSym(pSymf, pFunc, pArg, Flag);
}

/*************************************************************************
* DESCRIPTION
*	强制触发sym加载
*
* PARAMETERS
*	pSymHdl: sym句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SymForceLoad(SYM_HDL *pSymHdl)
{
	if (pSymHdl != pVmSHdl)
		LoadMdFile(pSymHdl);
}

/*************************************************************************
* DESCRIPTION
*	还原1个栈帧
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pRegAry/pRegMask: 寄存器组/寄存器有效位图(REG_SP/REG_PC必须有效!!!), 会被更新!!!
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymUnwind(SYM_HDL *pSymHdl, TSIZE *pRegAry, TSIZE *pRegMask)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);

	return (!pSymf->pDwfHdl || DwfUnwind(pSymf->pDwfHdl, pRegAry, pRegMask, pRegAry[REG_PC] - pSymf->Ex.LoadBias));
}

/*************************************************************************
* DESCRIPTION
*	获取已加载文件的路径
*
* PARAMETERS
*	pSymHdl: sym句柄
*
* RETURNS
*	NULL: 失败
*	其他: 路径
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *SymGetFilePath(SYM_HDL *pSymHdl)
{
	SYM_INT * const pSymf = MOD_INT(SYM, pSymHdl);

	return (pSymf->pFpHdl ? pSymf->pFpHdl->pPath : NULL);
}


/*************************************************************************
*============================= debug section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dbg句柄名称
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*
* RETURNS
*	NULL: 失败/无该属性
*	其他: 名称
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *DbgGetName(DBG_HDL *pDbgHdl)
{
	return DieGetName((void *)pDbgHdl);
}

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄占用字节
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*
* RETURNS
*	~(TSIZE)0: 失败/无该属性
*	其他: 占用字节
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DbgGetByteSz(DBG_HDL *pDbgHdl)
{
	return DieGetByteSz((void *)pDbgHdl);
}

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄偏移量(比如结构体成员偏移量)
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*
* RETURNS
*	~(TSIZE)0: 失败/无该属性
*	其他: 偏移量
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DbgGetOffset(DBG_HDL *pDbgHdl)
{
	return DieGetOffset((void *)pDbgHdl);
}

/*************************************************************************
* DESCRIPTION
*	批量获取dbg句柄的子句柄的偏移量(比如结构体成员偏移量)
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	pOffAry: 返回偏移量数组, 长度为n, 没有获取的填充-1
*	pNames: 成员名称数组, 长度为n
*	n: 个数
*
* RETURNS
*	0: 成功
*	其他: 获取失败个数
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetChildOffs(DBG_HDL *pDbgHdl, int *pOffAry, const char *pNames[], size_t n)
{
	struct _DBG_HDL cDbgHdl;
	size_t Cnt = n, i;
	const char *pName;

	if (!DieGetChildDieHdl((void *)pDbgHdl, (void *)&cDbgHdl))
		return Cnt;
	do {
		for (i = 0; i < n; i++) {
			pName = DieGetName((void *)&cDbgHdl);
			if (!pName || strcmp(pName, pNames[i]))
				continue;
			pOffAry[i] = (int)DieGetOffset((void *)&cDbgHdl);
			if (pOffAry[i] >= 0)
				Cnt--;
			break;
		}
	} while (DieGetSiblingDieHdl((void *)&cDbgHdl, (void *)&cDbgHdl));
	return Cnt;
}

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄对应路径和行号
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	pLineNo: 返回行号
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DbgGetLineInf(DBG_HDL *pDbgHdl, TUINT *pLineNo)
{
	return DieGetLineInf((void *)pDbgHdl, pLineNo);
}

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄的type dbg句柄
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	ptDbgHdl: 返回type dbg句柄
*
* RETURNS
*	0: 成功
*	1: 失败/无该dbg信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetTDbgHdl(DBG_HDL *pDbgHdl, struct _DBG_HDL *ptDbgHdl)
{
	return !DieGetTypeDieHdl((void *)pDbgHdl, (void *)ptDbgHdl);
}

/*************************************************************************
* DESCRIPTION
*	根据名称获取dbg句柄的子dbg句柄
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	pcDbgHdl: 返回子dbg句柄
*	pName: 名称
*
* RETURNS
*	0: 成功
*	1: 失败/无该dbg信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetCDbgHdl(DBG_HDL *pDbgHdl, struct _DBG_HDL *pcDbgHdl, const char *pName)
{
	const char *ptName;

	if (!DieGetChildDieHdl((void *)pDbgHdl, (void *)pcDbgHdl))
		return 1;
	do {
		ptName = DieGetName((void *)pcDbgHdl);
		if (ptName && !strcmp(ptName, pName))
			return 0;
	} while (DieGetSiblingDieHdl((void *)pcDbgHdl, (void *)pcDbgHdl));
	return 1;
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据名称获取sym句柄, 会触发sym加载!!!
*
* PARAMETERS
*	pName: sym名称
*
* RETURNS
*	NULL: 没有
*	其他: sym句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
SYM_HDL *GetSymHdlByName(const char *pName)
{
	TADDR SymfAddr = LastSymfAddr;
	VMA_HDL *pVmaHdl;
	SYM_HDL *pSymHdl;

	while (SymfAddr) {
		pVmaHdl = GetVmaHdl(SymfAddr);
		SymfAddr = pVmaHdl->LinkAddr;
		pSymHdl = pVmaHdl->pHdl;
		PutVmaHdl(pVmaHdl);
		if (!strcmp(pSymHdl->pName, pName)) {
			if (pSymHdl != pVmSHdl)
				LoadMdFile(pSymHdl);
			return pSymHdl;
		}
	}
	return NULL;
}

/*************************************************************************
* DESCRIPTION
*	根据地址获取sym句柄, 会触发sym加载!!!
*
* PARAMETERS
*	Addr: 地址
*
* RETURNS
*	NULL: 地址不在任何sym句柄内
*	其他: sym句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
SYM_HDL *GetSymHdlByAddr(TADDR Addr)
{
	SYM_HDL *pSymHdl = NULL;
	VMA_HDL *pVmaHdl;

	pVmaHdl = GetVmaHdl(Addr);
	if (pVmaHdl) {
		if (VMA_TYPE(pVmaHdl) == VMA_ELF) {
			pSymHdl = pVmaHdl->pHdl;
			if (pSymHdl != pVmSHdl)
				LoadMdFile(pSymHdl);
		}
		PutVmaHdl(pVmaHdl);
	}
	return pSymHdl;
}

/*************************************************************************
* DESCRIPTION
 *	遍历sym句柄, 不会触发sym加载!!!  vmlinux
*
* PARAMETERS
*	pFunc: 回调函数
*		pSymHdl: sym句柄
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachSymHdl(int (*pFunc)(SYM_HDL *pSymHdl, void *pArg), void *pArg)
{
	TADDR SymfAddr = LastSymfAddr;
	VMA_HDL *pVmaHdl;
	SYM_HDL *pSymHdl;
	int rt;

	while (SymfAddr) {
		pVmaHdl = GetVmaHdl(SymfAddr);
		SymfAddr = pVmaHdl->LinkAddr;
		pSymHdl = pVmaHdl->pHdl;
		PutVmaHdl(pVmaHdl);
		rt = pFunc(pSymHdl, pArg);
		if (rt)
			return rt;
	}
	return 0;
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
 *	初始化symfiles模块， load vmlinux文件模块 地址信息 ，将vmlinux叠加到coredump生成的内存层
*	使用方法: PrepareSymFiles() -> ModuleSymInit() -> DestorySymFiles()
*
* PARAMETERS
*	CoreCnt: CPU核数
*	CodeStart: kernel代码起始地址, 0表示未知
*	fp: 异常输出句柄
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PrepareSymFiles(TUINT CoreCnt,
		TADDR CodeStart /* KASLR random后的startaddress */, FILE *fp)
{
	static const char *pModNames[] =
	{
#define _NL(Enum, str) str,
		MODOFF_MAP
#undef _NL
	};
	struct _DBG_HDL fDbgHdl, DbgHdl, tDbgHdl;
	int rt, HasKSym = 1;
	TSIZE Off;

	if (sizeof(DBG_HDL) < sizeof(DIE_HDL)) /* 空间必须>=sizeof(DIE_HDL) */
		return 1;
	rt = VmlinuxInit(CoreCnt, CodeStart, fp);/* 初始化vmlinux文件模块, 和地址信息, 以及fpHdl */
	if (rt != 2) {
		if (!rt && !SymFile2DbgHdl(pVmSHdl, &fDbgHdl, "kernel/module.c") && !DbgGetCDbgHdl(&fDbgHdl, &DbgHdl, "module")) {
			ModInf.ModSz = (TUINT)DbgGetByteSz(&DbgHdl);
			if (!DbgGetCDbgHdl(&DbgHdl, &tDbgHdl, "core_layout")) { /* kernel4.9 */
				Off = DbgGetOffset(&tDbgHdl);
				ModInf.ModOffs[MOD_START]	= (int)Off;
				ModInf.ModOffs[MOD_SIZE]	= (int)Off + sizeof(TADDR);
				ModInf.ModOffs[MOD_TEXTSZ]	= (int)Off + sizeof(TADDR) + sizeof(TUINT) * 1;
				ModInf.ModOffs[MOD_ROSZ]	= (int)Off + sizeof(TADDR) + sizeof(TUINT) * 2;
				if (!DbgGetCDbgHdl(&DbgHdl, &tDbgHdl, "kallsyms"))
					ModInf.ModOffs[MOD_SYM] = (int)DbgGetOffset(&tDbgHdl);
				else
					SSTWARN(RS_VAR_CHANGE, "module");
			} else if (DbgGetChildOffs(&DbgHdl, ModInf.ModOffs, pModNames, ARY_CNT(pModNames))) {
				SSTWARN(RS_VAR_CHANGE, "module");
			}
		}
		return rt;
	}
	if (!CodeStart) { /* 老版本kernel起始地址 */
#ifdef SST64
		CodeStart = 0xFFFFFFC000000000 + KVM_OFF;  //stext
#else
		CodeStart = 0xC0000000 + KVM_OFF;
#endif
		HasKSym = 0;
	}
	rt = KSymInit(CodeStart, HasKSym);/**/
	if (!rt)
		return rt;
	return SymFileInit(CodeStart);/**/
}

/*************************************************************************
* DESCRIPTION
*    初始化module模块
*
* PARAMETERS
*    ModAddr: 模块结构体地址，如果不为0，则LoadBias和pName可以为空
*    LoadBias/Size: 加载偏移量和模块大小
*    pName: 模块名称
*
* RETURNS
*    0: 成功
*    1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ModuleSymInit(TADDR ModAddr, TADDR LoadBias, TSIZE Size, const char *pName)
{
#define MODULE_STATE_LIVE	0
#define MODULE_STATE_COMING	1
#define MODULE_STATE_GOING	2
	struct /* struct mod_kallsyms */
	{
		TADDR TblAddr;
		TUINT Cnt;
	} ModKaSyms;
    TADDR CodeEnd, RoDataEnd;
	VMA_HDL *pVmaHdl = NULL;
    SYMF_MD *pSymfMd;
	const char *pBuf;
	TSIZE SymSz;

    pSymfMd = Calloc(sizeof(*pSymfMd));
    if (!pSymfMd) {
        SSTWARN(RS_OOM);
        return 1;
    }
	pVmaHdl = GetVmaHdl(ModAddr);
    if (pVmaHdl) {
		pBuf = Vma2Mem(pVmaHdl, ModAddr, ModInf.ModSz);
		if (!pBuf || OFFVAL(TUINT, pBuf, 0) > MODULE_STATE_GOING) /* 安全检查 */
			goto _FAIL;
		snprintf(pSymfMd->Name, sizeof(pSymfMd->Name) - 4, "%s", pBuf + sizeof(TSIZE) * 3);
        LoadBias	= OFFVAL_ADDR(pBuf, ModInf.ModOffs[MOD_START]);
		CodeEnd		= OFFVAL(TUINT, pBuf, ModInf.ModOffs[MOD_TEXTSZ]) + LoadBias;
		RoDataEnd	= OFFVAL(TUINT, pBuf, ModInf.ModOffs[MOD_ROSZ]) + LoadBias;
		Size		= OFFVAL(TUINT, pBuf, ModInf.ModOffs[MOD_SIZE]);
		if (!DumpMem(OFFVAL_ADDR(pBuf, ModInf.ModOffs[MOD_SYM]), &ModKaSyms, sizeof(ModKaSyms))) {
			ModKaSyms.TblAddr += sizeof(*pSymfMd->c.pSGSym); /* 从第1个起始 */
			SymSz = Size - (ModKaSyms.TblAddr - LoadBias); /* symtab/strtab放在最后 */
			pSymfMd->c.pSGSym = Malloc((size_t)SymSz);
			if (pSymfMd->c.pSGSym && !DumpMem(ModKaSyms.TblAddr, (void *)pSymfMd->c.pSGSym, SymSz)) {
				pSymfMd->c.pEGSym = pSymfMd->c.pSGSym + ModKaSyms.Cnt - 1;
				pSymfMd->c.pSymStr = (char *)pSymfMd->c.pEGSym;
			} else {
				Free((void *)pSymfMd->c.pSGSym);
				pSymfMd->c.pSGSym = NULL;
			}
		}
		PutVmaHdl(pVmaHdl);
    } else {
		if (!LoadBias)
			goto _FAIL;
		snprintf(pSymfMd->Name, sizeof(pSymfMd->Name) - 4, "%s", pName);
		CodeEnd = RoDataEnd = LoadBias + Size;
    }
	pSymfMd->c.Ex.LoadBias = LoadBias;
    if (!pSymfMd->c.pSGSym)
        MdKallSymsInit(pSymfMd, pSymfMd->Name);
	strcat(pSymfMd->Name, ".ko");
    pSymfMd->c.Ex.pName		= pSymfMd->Name;
	pSymfMd->c.Ex.Flag		= SYM_MEMSYM;
    pSymfMd->c.Name2Addr	= MdName2Addr;
    pSymfMd->c.Addr2Name	= MdAddr2Name;
    pSymfMd->c.ForEachSym	= MdForEachSym;
    pSymfMd->c.DeInit		= MdDeInit;
	SfKsBind2Vma(&pSymfMd->c, LoadBias, CodeEnd, RoDataEnd, LoadBias + Size);
    return 0;
_FAIL:
	if (pVmaHdl)
		PutVmaHdl(pVmaHdl);
	Free(pSymfMd);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	注销symfiles模块
*
* PARAMETERS
*	无
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DestorySymFiles(void)
{
	KALLSYM_SF *pKaSymSf;

	ForEachSymHdl(FreeSymHdl, NULL);
	while ((pKaSymSf = pKallSymSf) != NULL) {
		pKallSymSf = pKallSymSf->pNext;
		FreeKaSymfs(pKaSymSf);
	}
	LastSymfAddr = 0;
}
