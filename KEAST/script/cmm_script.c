/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	cmm_script.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/11/15
* Description: cmm脚本定义
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <armelf.h>
#include <mrdump.h>
#include <kernel/task.h>
#include "script_int.h"
#include "cmm_script.h"

//#define MRDUMP_DISABLE_MMU
#ifdef SST64
#define REGN_PREFIX	"x"
#define SP_STR		"sp "
#else
#define REGN_PREFIX	"r"
#define SP_STR		"r13"
#endif

typedef struct _MMU_ARG
{
	FILE *fp;

#ifdef SST64
#define MMU_TBL_FLAG	0x03
#ifdef MRDUMP_DISABLE_MMU
#define MMU_PADDR(x)	((TSIZE)(x))
#else
#define MMU_PADDR(x)	((TSIZE)(x) - (~(((TSIZE)1 << ADDR_BITS) - 1))) /* PA只有48bit，不支持64bit因此高16位必须为0 */
#endif
#define PGD_BITS		9
#define PMD_BITS		9
	TADDR Pgd[1 << PGD_BITS], *pcPgd;
#else
#define MMU_TBL_FLAG	0x01
#define MMU_PADDR(x)	((TSIZE)(x))
#define PGD_BITS		0
#define PMD_BITS		12
#endif
#define PTE_BITS		(ADDR_BITS - PGD_BITS - PMD_BITS - PAGE_BITS)
#define PMD_SIZE		((TSIZE)1 << (PAGE_BITS + PTE_BITS))
	TADDR Pmd[1 << PMD_BITS], *pcPmd, Pte[1 << PTE_BITS];
	TADDR AllocAddr;
} MMU_ARG;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
 *	设置cmm所需的符号文件  KASLR:pSymHdl->LoadBias
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pArg: 参数
*
* RETURNS
*	0: 继续
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SetCmmNeededSyms(SYM_HDL *pSymHdl, void *pArg)
{
	const char *pStr, *pStr2;
	FILE * const fp = pArg;

	if (IS_SCRLOADALL())
		SymForceLoad(pSymHdl);
	if (pSymHdl->Flag&SYM_MATCH) {
#ifdef MRDUMP_DISABLE_MMU
		pStr = "";
#else
		pStr = " /ncode"; /* Suppress the code download. Only loads symbolic information. */
#endif
		pStr2 = (pSymHdl == pVmSHdl ? "" : " /codesec /reloc .text at");/* vmlinux不需要再reloc 直接使用LoadBias偏移 */
		if (GetKExpInf()->mType == MRD_T_MIDUMP/* minidump */) { /* 需要加载代码段 */
			if (MMU_PADDR(0))
				LOGC(fp, "d.load.e %s%s "TADDRFMT" /nc%s\n", SymGetFilePath(pSymHdl), pStr2, MMU_PADDR(pSymHdl->LoadBias), " /ny");
			else
				pStr = "";
		}
		LOGC(fp, "d.load.e %s%s "TADDRFMT" /nc%s\n", SymGetFilePath(pSymHdl), pStr2, pSymHdl->LoadBias, pStr);
	} else if (pSymHdl->Flag&SYM_LOAD) {
		LOGC(fp, "if os.file(\"%s/%s\")\n\tgosub newfile %s\n", SstOpt.pSymDir, pSymHdl->pName, pSymHdl->pName);
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	建立sym句柄的global sym tab
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pArg: arg
*
* RETURNS
*	0: 继续
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SymBuildSyms(SYM_HDL *pSymHdl, void *pArg)
{
	return (pSymHdl->Flag&SYM_MATCH ? 0 : SymForEachSym(pSymHdl, BuildOneSym, pArg, 0x03));
}

/*************************************************************************
* DESCRIPTION
*	coredump建立所有需要的sym
*
* PARAMETERS
*	pArg: 额外的参数
*
* RETURNS
*	0: 继续
*	1: 失败/OOM
*
* LOCAL AFFECTED
*
*************************************************************************/
static int BuildSyms(void *pArg)
{
	return ForEachSymHdl(SymBuildSyms, pArg);
}

/*************************************************************************
* DESCRIPTION
*	刷新pte表
*
* PARAMETERS
*	pMmuArg: mmu信息
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FlushPteTbl(MMU_ARG *pMmuArg)
{
	unsigned i = 0;
	TADDR Val;

	if (!pMmuArg->pcPmd)
		return;
	if (pMmuArg->Pte[0] && pMmuArg->Pte[(1 << PTE_BITS) - 1]) { /* 是否一致? */
		for (Val = pMmuArg->Pte[0]&(PAGE_SIZE - 1), i = 1; i < (1 << PTE_BITS); i++) {
			if ((pMmuArg->Pte[i]&(PAGE_SIZE - 1)) != Val)
				break;
		}
	}
	if (i != (1 << PTE_BITS)) { /* 不一致! */
		*pMmuArg->pcPmd = pMmuArg->AllocAddr|MMU_TBL_FLAG;
		pMmuArg->AllocAddr += sizeof(pMmuArg->Pte);
		fwrite(pMmuArg->Pte, 1, sizeof(pMmuArg->Pte), pMmuArg->fp);
	}
	memset(pMmuArg->Pte, 0, sizeof(pMmuArg->Pte));
	pMmuArg->pcPmd = NULL;
}

/*************************************************************************
* DESCRIPTION
*	刷新pmd表
*
* PARAMETERS
*	pMmuArg: mmu信息
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FlushPmdTbl(MMU_ARG *pMmuArg)
{
#ifdef SST64
	if (!pMmuArg->pcPgd)
		return;
	/* 基本不可能发生一致! */
	*pMmuArg->pcPgd = pMmuArg->AllocAddr|MMU_TBL_FLAG;
	pMmuArg->AllocAddr += sizeof(pMmuArg->Pmd);
	fwrite(pMmuArg->Pmd, 1, sizeof(pMmuArg->Pmd), pMmuArg->fp);
	memset(pMmuArg->Pmd, 0, sizeof(pMmuArg->Pmd));
	pMmuArg->pcPgd = NULL;
#else
	(void)pMmuArg;
#endif
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
static int GenMMU2ndTable(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg)
{
#define PGD_IDX(Addr)   (((Addr) >> (PAGE_BITS + PTE_BITS + PMD_BITS))&((1 << PGD_BITS) - 1))
#define PMD_IDX(Addr)   (((Addr) >> (PAGE_BITS + PTE_BITS))&((1 << PMD_BITS) - 1))
#define PTE_IDX(Addr)   (((Addr) >> PAGE_BITS)&((1 << PTE_BITS) - 1))
#define PGD_SIZE		((TSIZE)1 << (PAGE_BITS + PTE_BITS + PMD_BITS))
	MMU_ARG * const pMmuArg = pArg;
	unsigned PmdFlag, PteFlag;
	TADDR PmdNext, *pPmd, *pPte;
#ifdef SST64
	TADDR PgdNext, *pPgd;
	unsigned PgdFlag;
#else
#define PgdNext EAddr
#endif
	
	if (!(Flag&VMA_R)) /* 忽略无读属性 */
		return 0;
#ifdef SST64
	PgdFlag = PmdFlag = (Flag&VMA_W ? 0x461 : 0x4E1);
	PteFlag = PgdFlag|0x03;
	pPgd = &pMmuArg->Pgd[PGD_IDX(SAddr)];
	do {
		PgdNext = (SAddr + PGD_SIZE)&~(PGD_SIZE - 1);
		if (PgdNext - 1 > EAddr - 1) {
			PgdNext = EAddr;
		} else if (!(SAddr&(PGD_SIZE - 1))) {
			*pPgd = MMU_PADDR(SAddr)|PgdFlag;
			SAddr = PgdNext;
			continue;
		}
		if (!*pPgd) { /* 分配空间 */
			FlushPteTbl(pMmuArg);
			FlushPmdTbl(pMmuArg);
			*pPgd = (MMU_PADDR(SAddr)&~(PGD_SIZE - 1))|PgdFlag;
			pMmuArg->pcPgd = pPgd;
		}
#else
		PmdFlag = (Flag&VMA_W ? 0x0C02 : 0x8C02);
		PteFlag = (Flag&VMA_W ? 0x0032 : 0x0232);
#endif
		pPmd = &pMmuArg->Pmd[PMD_IDX(SAddr)];
		do {
			PmdNext = (SAddr + PMD_SIZE)&~(PMD_SIZE - 1);
			if (PmdNext - 1 > EAddr - 1) {
				PmdNext = EAddr;
			} else if (!(SAddr&(PMD_SIZE - 1))) {
				*pPmd = MMU_PADDR(SAddr)|PmdFlag;
				SAddr = PmdNext;
				continue;
			}
			if (!*pPmd) { /* 分配空间 */
				FlushPteTbl(pMmuArg);
				*pPmd = (MMU_PADDR(SAddr)&~(PMD_SIZE - 1))|PmdFlag;
				pMmuArg->pcPmd = pPmd;
			}
			pPte = &pMmuArg->Pte[PTE_IDX(SAddr)];
			do {
				*pPte++ = MMU_PADDR(SAddr)|PteFlag;
			} while (SAddr += PAGE_SIZE, SAddr != PmdNext);
		} while (pPmd++, SAddr != PgdNext);
#ifdef SST64
	} while (pPgd++, SAddr != EAddr);
#endif
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	根据coredump生成mmu页表文件
*
* PARAMETERS
*	pFile: 生成的MMU页表文件
*
* RETURNS
*	0: 失败
*	其他: 加载地址
*
* LOCAL AFFECTED
*
*************************************************************************/
static TADDR CreateMMUTable(const char *pFile)
{
	MMU_ARG *pArg;
	TADDR BaseAddr;
	int rt = 1;

	pArg = Calloc(sizeof(*pArg));
	if (!pArg)
		goto _OOM;
	pArg->fp = fopen(pFile, "wb");
	if (!pArg->fp)
		goto _ERR1;
#ifdef SST64
	BaseAddr = ((TADDR)1 << ADDR_BITS);
	pArg->AllocAddr = BaseAddr + sizeof(pArg->Pgd);
	fwrite(pArg->Pgd, 1, sizeof(pArg->Pgd), pArg->fp);
#else
	BaseAddr = 0x40000000;
	pArg->AllocAddr = BaseAddr + sizeof(pArg->Pmd);
	fwrite(pArg->Pmd, 1, sizeof(pArg->Pmd), pArg->fp);
#endif
	ForEachVmaAll(GenMMU2ndTable, pArg);
	FlushPteTbl(pArg);
	FlushPmdTbl(pArg);
	if (BaseAddr + ftell(pArg->fp) == pArg->AllocAddr) {
		fseek(pArg->fp, 0, SEEK_SET);
#ifdef SST64
		rt = (fwrite(pArg->Pgd, 1, sizeof(pArg->Pgd), pArg->fp) != sizeof(pArg->Pgd));
#else
		rt = (fwrite(pArg->Pmd, 1, sizeof(pArg->Pmd), pArg->fp) != sizeof(pArg->Pmd));
#endif
	}
	fclose(pArg->fp);
	Free(pArg);
	if (!rt)
		return BaseAddr;
	remove(pFile);
	SSTERR(RS_FILE_WRITE, pFile);
	return 0;
_ERR1:
	Free(pArg);
	SSTERR(RS_CREATE, pFile);
	return 0;
_OOM:
	SSTWARN(RS_OOM);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	添加cpu上下文信息
*
* PARAMETERS
*	fp: 文件句柄
*	pCpuCntx: 进程信息
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void AddCmmCntx(FILE *fp, CORE_CNTX *pCpuCntx)
{
	size_t i;

#ifdef SST64
	LOGC(fp, "r.s sp_el0\t"TADDRFMT"\n", pCpuCntx->TskAddr);
#endif
	LOGC(fp, "r.s cpsr\t"TADDRFMT"\nr.s pc\t"TADDRFMT"\nr.s "SP_STR"\t"TADDRFMT"\n"
		, pCpuCntx->Cpsr, pCpuCntx->RegAry[REG_PC], pCpuCntx->RegAry[REG_SP]);
	for (i = 0; i < REG_PC; i++) {
		if (i != REG_SP)
			LOGC(fp, "r.s "REGN_PREFIX"%d\t"TADDRFMT"\n", i, pCpuCntx->RegAry[i]);
	}
}

/*************************************************************************
* DESCRIPTION
*	设置linux aware
*
* PARAMETERS
*	fp: 文件句柄
*	IsEn: 是否打开linux aware
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void SetupLinuxAware(FILE *fp, int IsEn)
{
	static const char *pSstScriptFiles[] =
	{
#ifdef SST64
		"linux64.t32",
#else
		"linux.t32",
#endif
		"linux.men",
		"autoload.cmm",
		"task_ps_config.cmm",
	};
	char FilePath[MAX_PATH], DstPath[MAX_PATH];
	size_t i;

	FilePath[sizeof(FilePath) - 1] = DstPath[sizeof(DstPath) - 1] = '\0';
	for (i = 0; i < ARY_CNT(pSstScriptFiles); i++) { /* 检查文件是否存在 */
		snprintf(FilePath, sizeof(FilePath) - 1, "%s/%s", SstOpt.pInstallDir, pSstScriptFiles[i]);
		snprintf(DstPath, sizeof(DstPath) - 1, "%s/%s", UTILS_DIR, pSstScriptFiles[i]);
		if (CCpyFile(DstPath, FilePath))
			return;
	}
	LOGC(fp, "\nmenu.rp %s/%s\n", UTILS_DIR, pSstScriptFiles[1]);
	if (!IsEn)
		return;
	LOGC(fp, "\ntask.config %s/%s\nhelp.filter.add rtoslinux\ntask.setdir %s/~\ntask.y.o mmuscan off\ntask.y.o rootpath \"%s\"\n"
	  "task.y.o al m\ntask.y.o al cl\ntask.y.o al vm\ntask.o nm taskname\ny.aload.checklinux \"do %s/autoload \"\ny.aload.check\n"
	  "task.check\n", UTILS_DIR, pSstScriptFiles[0], UTILS_DIR, SstOpt.pSymDir, UTILS_DIR);
}

#ifdef MRDUMP_DISABLE_MMU
/*************************************************************************
* DESCRIPTION
*	直接从mrdump文件加载
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
static int GenPhyMapping(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg)
{
	struct
	{
		VMA_HDL Ex; /* export info */
		void *Pad[2];
		TSIZE FOff; /* 文件偏移量, 0表示0内存/外部映射/无映射 */
	} *pVmaHdl;

	if (!Flag)
		return 0;
	pVmaHdl = (void *)GetVmaHdl(SAddr);
	if (pVmaHdl->FOff)
		LOGC(pArg, "d.load.binary %s "TADDRFMT"--"TADDRFMT" /skip "TADDRFMT" /nc /ny\n"
			, "SYS_COREDUMP", SAddr, EAddr - 1, pVmaHdl->FOff);
	PutVmaHdl((void *)pVmaHdl);
	return 0;
}
#endif
#ifdef SST64
/*************************************************************************
* DESCRIPTION
*	用d.load.binary加载core file
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
*	1: 完成
*
* LOCAL AFFECTED
*
*************************************************************************/
static int LoadCoreFile(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg)
{
	FILE * const fp = pArg;

	(void)Flag, (void)Va;
	LOGC(fp, "d.load.binary %s "TADDRFMT"++"TADDRFMT" /nc /skip "TADDRFMT"\n", GetKExpInf()->pCoreFile, Pa, Size - 1, FOff);
	return 0;
}
#endif

/*************************************************************************
* DESCRIPTION
*	根据coredump生成cmm脚本
*
* PARAMETERS
*	pFileName: 脚本名
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void GenCmmScript(const char *pFileName)
{
#define CMM_SYMFILE UTILS_DIR"/cmmsym.elf"
#define CMM_MMUFILE UTILS_DIR"/cmmmmu.bin"
	KEXP_INF * const pKExpInf = GetKExpInf();  			//exp_info Form KDinfo
	CORE_CNTX *pCpuCntx;								//per cpu context
	TADDR MmuBase;
	FILE *fp;
	TUINT i;

	fp = fopen(pFileName, "w");
	if (!fp) {
		SSTERR(RS_CREATE, pFileName);
		return;
	}
	SSTINFO(RS_GENSCRIPT, "cmm");
	LOGC(fp, "; ");
	LOG(fp, RS_GEN_NOTE);
#ifdef SST64
	LOGC(fp, "\ndo ~~/t32.cmm\nsys.res\nsys.cpu CortexA53\nsys.config.cn %d\ncore.num %d\n", pKExpInf->CoreCnt, pKExpInf->CoreCnt);
#else
	LOGC(fp, "\ndo ~~/t32.cmm\nsys.res\nsys.cpu CortexA7MPCore\nsys.config.cn %d\ncore.num %d\n", pKExpInf->CoreCnt, pKExpInf->CoreCnt);
#endif
#ifdef MRDUMP_DISABLE_MMU /* MMU disable 直接loadbinary 虚拟地址 对齐到PA ， Signal 单步运行时候需要关闭MMU才行 */
	MmuBase = 0;
#else
	MmuBase = (
			pKExpInf->mType == MRD_T_MRDUMP ?
					pKExpInf->MmuBase : CreateMMUTable(CMM_MMUFILE)); /* minidump 如果不直接物理地址映射需要自建页表 */
#endif
	if (MmuBase)
		LOGC(fp, "sys.o mmuspaces on\n");			//启用逻辑地址空间ID space id
	LOGC(fp, "sys.u\ntitle os.pwd()\n\n");
	/* 写入符号文件 */
	if (!CreateSymFile(CMM_SYMFILE, BuildSyms, NULL))
		LOGC(fp, "d.load.e %s /nc /as\n", CMM_SYMFILE); /* cmmsym.elf */
	ForEachSymHdl(SetCmmNeededSyms, fp);
	if (pKExpInf->mType == MRD_T_MRDUMP) {
#ifdef MRDUMP_DISABLE_MMU
		ForEachVmaAll(GenPhyMapping, fp);
#else
#ifdef SST64
		int Workaround = 0;
		ELF_HDL *pElfHdl;
		FP_HDL *pFpHdl;

		pFpHdl = GetFpHdl(pKExpInf->pCoreFile, 0); /* workaround for T32 don't support loading elf file over 4G */
		if (pFpHdl) {
			if (pFpHdl->FileSz >= 0x100000000 && (pElfHdl = GetElfHdl(pFpHdl, ET_CORE)) != NULL) {
				ElfForEachLoad(pElfHdl, LoadCoreFile, fp);
				PutElfHdl(pElfHdl);
				Workaround = 1;
			}
			PutFpHdl(pFpHdl);
		}
		if (!Workaround)
#ifdef Wen_Feature
			{

			}
#endif
#endif
			LOGC(fp, "d.load.e %s /nc /ny /physload\n", pKExpInf->pCoreFile);//load mrdump  option: noclear, nosymbol phyload
#endif
	} else {
		LOGC(fp, "d.load.e %s "TADDRFMT" /nc /ny /logload\n",
				pKExpInf->pCoreFile, MMU_PADDR(0)); /* minidump 直接load 到制定Va地址*/
		if (MmuBase) /* 写入MMU页表文件 */
			LOGC(fp, "d.load.b %s "TADDRFMT" /b /nc\n", CMM_MMUFILE, MmuBase);
	}
	/* 写入全局变量 */
	LOGC(fp, "global &MemMapAddr &ModStart\n&MemMapAddr="TADDRFMT"\n&ModStart="TADDRFMT"\n", pKExpInf->MemMapAddr, pKExpInf->ModStart);
	/* 写入内核上下文 */
	for (i = pKExpInf->CoreCnt; i-- > 0; ) {
		pCpuCntx = GetCoreCntx(i);
		if (!pCpuCntx)
			continue;
		LOGC(fp, "\ncore.select %d\n", i);
		AddCmmCntx(fp, pCpuCntx);
		if (MmuBase)
#ifdef SST64
			LOGC(fp, "per.s spr:0x30201 %%q "TADDRFMT"\nper.s spr:0x30202 %%q "TADDRFMT"\nper.s spr:0x30100 %%l 0x%X\n"
				, MmuBase, pCpuCntx->Tcr, pCpuCntx->Sctlr);
#else
			LOGC(fp, "per.s c15:0x2 %%l "TADDRFMT"\nper.s c15:0x202 %%l "TADDRFMT"\nper.s c15:0x1 %%l 0x%X\n"
				, MmuBase, pCpuCntx->Tcr&(pKExpInf->mType == MRD_T_MIDUMP ? 0x7FF0FFF0/* minidump用常规MMU的 */ : 0xFFF0FFF0)/* 清除TxSZ, 始终使用TTBR0 */
				, pCpuCntx->Sctlr);
#endif
	}
	if (MmuBase)
		LOGC(fp, "mmu.format linuxswap3 A:"TADDRFMT"\ntrans.common "TADDRFMT"--"TADDRFMT"\ntrans.tw on\ntrans.on\n", MmuBase, START_KERNEL, ~(TADDR)0);
	/* linux aware */
	if ((pVmSHdl->Flag&(SYM_MATCH|SYM_LCLSYM)) == (SYM_MATCH|SYM_LCLSYM) && SstOpt.pInstallDir)
		SetupLinuxAware(fp, pKExpInf->mType == MRD_T_MRDUMP);
	/* misc segment */
	LOGC(fp, "\ncore.select %d\nmmu.scan\ny.spath src\nsetup.v %%h.on %%s.on %%y.on\nwclear\nwpos 0%% 0%% 50%% 100%%\nw.v.f /a /l\n"
	  "wpos 50%% 0%%\nw.r\nwpos 50%% 30%%\nd.l\nenddo\n"
	  "newfile:\n\tentry &file\n\tdialog\n\t(&\n\t\theader \"New file exist\"\n\t\tpos 0. 0. 28. 1.\n"
	  "\t\ttext \"[Warning]Detect &file exist.\"\n\t\ttext \"Please rerun 'NK/KE-Analyze' to check if matched.\"\n"
	  "\t\tpos 2. 3. 24. 1.\n\t\tdefbutton \"OK\" \"quit\"\n\t\tclose \"quit\"\n\t)\n\tenddo", pKExpInf->CpuId);
	fclose(fp);
}
