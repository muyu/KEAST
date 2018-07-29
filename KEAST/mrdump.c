/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	mrdump.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: mrdump�����������ַת��
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "os.h"
#include "string_res.h"
#include <armelf.h>
#include "os.h"
#include <module.h>
#include <mrdump.h>

#define DEF_MRFILE	"SYS_COREDUMP"
#define DEF_MIFILE	"SYS_MINI_RDUMP"

typedef struct _ELF_OFF
{
	TADDR Pa, EPa;
	TSIZE FOff;
} ELF_OFF;

typedef struct _MMU_MAP
{
	TADDR Pa, Va;
	TSIZE Sz;
	unsigned Attr;
} MMU_MAP;

typedef struct _CD_ARG
{
	FP_HDL *pFpHdl;
	ELF_HDL *pElfHdl;
} CD_ARG;

static struct
{
	struct _KEXP_INF KExpInf;
	/* CPU��Ϣ */
	TUINT CoreVMap/* ��Чλͼ */;
	struct _CORE_CNTX Cpu[MAX_CORE];
	TADDR PSAddr, CodeStart;
} KDInf;
static const TSIZE ZeroRegs[REG_NUM] = {0};


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡָ�������ַ���ļ�ƫ����
*
* PARAMETERS
*	Flag: PF_*��־
*	FOff: �ļ�ƫ����(�ֽ�)
*	Size: ռ���ļ���С(�ֽ�)
*	Va/Pa: ����/�����ַ
*	pArg: ����
*
* RETURNS
*	0: ����
*	1: ʧ��
*	2: �ɹ�
*
* LOCAL AFFECTED
*
*************************************************************************/
static int Phy2OffEx(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg)
{
	ELF_OFF * const pElfOff = pArg;

	(void)Flag, (void)Va;
	if (pElfOff->Pa >= Pa && pElfOff->EPa <= Pa + Size - 1) {
		pElfOff->FOff = pElfOff->Pa - Pa + FOff;
		return 2;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
 *	��ȡָ�������ַ���ļ�ƫ���������������ַת��ΪCoredump �ļ��е�ƫ�Ƶ�ַ
*
* PARAMETERS
*	pElfHdl: elf���
*	PAddr/Size: �����ַ�ʹ�С(�ֽ�)
*
* RETURNS
*	0: ʧ��
*	����: �ļ�ƫ����
*
* LOCAL AFFECTED
*
*************************************************************************/
static TSIZE Phy2Off(ELF_HDL *pElfHdl, TADDR PAddr, TSIZE Size)
{
	ELF_OFF ElfOff;

	ElfOff.Pa = PAddr;
	ElfOff.EPa = PAddr + Size - 1;
	return (ElfForEachLoad(pElfHdl, Phy2OffEx, &ElfOff) == 2 ? ElfOff.FOff : 0);
}

/*************************************************************************
* DESCRIPTION
*	��ȡCPU�Ĵ�����Ϣ
*
* PARAMETERS
*	pStr: ������
*	pBuf: buffer
*	Size: ��С(�ֽ�)
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ReadCoreCntxInf(const char *pStr, const void *pBuf, TUINT Size)
{
	typedef const struct _KTIMEVAL
	{
		TSSIZE tv_sec, tv_usec;
	} KTIMEVAL;
	const struct
	{
		TINT si_signo, si_code, si_errno;
		short cursig; /* Current signal == si_signo */
		TSIZE sigpend, sighold; /* Set of pending/held signals */
		TUINT pid, ppid, pgrp, sid;
		KTIMEVAL utime, stime, cutime, cstime; /* User/System time, Cumulative user/system time */
		TSIZE RegAry[REG_NUM], Cpsr;
#ifndef SST64
		TSIZE OrgR0;
#endif
		TINT fpvalid; /* True if math co-processor being used. */
	} * const pCntxInf = pBuf;
	TUINT CpuId = 0; 

	if (Size != sizeof(*pCntxInf)) {
		SSTERR(RS_SEG_SIZE, KDInf.KExpInf.pCoreFile, "NT_PRSTATUS", Size);
		return 1;
	}
	if (KDInf.KExpInf.CoreCnt < pCntxInf->ppid) /* �°�SYS_MINI_RDUMP���NR_CPUS */
		KDInf.KExpInf.CoreCnt = pCntxInf->ppid;
	if (!strcmp(pStr, "NA")) /* ������ */
		return 0;
	if ((sscanf(pStr, "%*[^0-9]%d", &CpuId) != 1 || CpuId >= MAX_CORE) && strcmp(pStr, "CORE")/* < KK1.MP11�汾 */) {
		SSTWARN(RS_CORE_INF);
		return 1;
	}
	if (!memcmp(&pCntxInf->RegAry, &ZeroRegs, sizeof(ZeroRegs))) /* ������ */
		return 0;
	if (pCntxInf->RegAry[REG_SP] && pCntxInf->RegAry[REG_SP] <= KDInf.Cpu[CpuId].RegAry[REG_SP]) /* ѡ��ջ���, Ϊ�����ֳ� */
		return 0;
	if (!KDInf.CoreVMap)
		KDInf.KExpInf.CpuId = CpuId;
	UpdateCoreCntx(CpuId, pCntxInf->RegAry, pCntxInf->Cpsr, pCntxInf->sigpend,
			0, NULL); //UpdateCoreCntx ,�����CoreCnt
	return 0;
}

/*************************************************************************
* DESCRIPTION
 *	��ȡcontrol block��Ϣ,
*
* PARAMETERS
*	pBuf: buffer
*	Size: ��С(�ֽ�)
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ExtractCtrlBlock(const void *pBuf, TUINT Size)
{
#define CTRLBLK_V8_SZ	4880 /* sizeof(CTRLBLK_V8) */
#define CTRLBLK_V7_SZ	5232 /* sizeof(CTRLBLK_V7) */
	typedef const struct _CTRLBLK_V8
	{
		char Sig[8];
		unsigned CpuCnt, Pad;
		unsigned long long PageOff, HighMem;
		unsigned long long VmStart, Pad2[2], SText, Pad3[2];
		unsigned long long KimgOffset;
		unsigned long long Pad4[2];
		unsigned long long VMlcStart, VMlcEnd;
		unsigned long long ModStart, ModEnd;
		unsigned long long PhysOff, MasterPgTbl, MemMapAddr, DfdMemPa;
		unsigned KAllSymsInf[12], machdesc_crc, enabled, output_fs_lbaooo, Pad5;
		/* crash info */
		unsigned reboot_mode;
		char msg[128];
		unsigned fault_cpu;
		unsigned long long regs[12][34];
		union
		{
			struct
			{
				unsigned sctlr, pad;
				unsigned long long ttbcr, ttbr0, ttbr1;
			} aarch32;
			struct
			{
				unsigned sctlr_el[3]/* ֻ��el1~3 */, pad;
				unsigned long long tcr_el[3]/* ֻ��el1~3 */, ttbr0_el[3]/* ֻ��el1~3 */, ttbr1_el1, sp_el[4];
			} aarch64;
		} exts[12];
	} CTRLBLK_V8;
	typedef const struct _CTRLBLK_V7
	{
		char Sig[8];
		TUINT CpuCnt, Pad;
		unsigned long long PageOff, HighMem;
		unsigned long long VmStart, Pad2[2], SText, Pad3[5];
		unsigned long long VMlcStart, VMlcEnd;
		unsigned long long ModStart, ModEnd;
		unsigned long long PhysOff, MasterPgTbl, MemMapAddr, DfdMemPa;
		unsigned KAllSymsInf[12], machdesc_crc, enabled, output_fs_lbaooo, Pad4, reboot_mode;
		char msg[128], bt[512];
		unsigned fault_cpu;
		unsigned long long regs[16][34];
	} CTRLBLK_V7;
	const TSIZE *pRegAry;
	size_t i;

#ifdef _DEBUG
	if (CTRLBLK_V8_SZ != sizeof(CTRLBLK_V8) || CTRLBLK_V7_SZ != sizeof(CTRLBLK_V7))
		DBGERR("mrdump_cblock size unmatched: (%d,%d),(%d,%d)\n", CTRLBLK_V8_SZ, sizeof(CTRLBLK_V8), CTRLBLK_V7_SZ, sizeof(CTRLBLK_V7));
#endif
	switch (Size) {
	case sizeof(CTRLBLK_V8):
		KDInf.PSAddr			= (TADDR)((CTRLBLK_V8 *)pBuf)->PhysOff;
		KDInf.CodeStart			= (TADDR)((CTRLBLK_V8 *)pBuf)->SText;
		KDInf.KExpInf.MmuBase	= (TADDR)((CTRLBLK_V8 *)pBuf)->MasterPgTbl;
		KDInf.KExpInf.MemMapAddr = (TADDR)((CTRLBLK_V8 *)pBuf)->MemMapAddr;
		KDInf.KExpInf.ModStart	= (TADDR)((CTRLBLK_V8 *)pBuf)->ModStart;
		for (i = 0; i < ARY_CNT(((CTRLBLK_V8 *)pBuf)->exts) && i < ARY_CNT(KDInf.Cpu); i++) {
			pRegAry = (void *)((CTRLBLK_V8 *)pBuf)->regs[i];
			if (!memcmp(pRegAry, &ZeroRegs, sizeof(ZeroRegs))) /* ������ */
				continue;
#ifdef SST64
			KDInf.Cpu[i].Sctlr	= (TUINT)((CTRLBLK_V8 *)pBuf)->exts[i].aarch64.sctlr_el[0];
			KDInf.Cpu[i].Tcr	= (TSIZE)((CTRLBLK_V8 *)pBuf)->exts[i].aarch64.tcr_el[0];
			//KDInf.Cpu[i].TskAddr = ((CTRLBLK_V8 *)pBuf)->exts[i].aarch64.sp_el[0]; /* kernel4.4��֮��������� */
#else
			KDInf.Cpu[i].Sctlr	= (TUINT)((CTRLBLK_V8 *)pBuf)->exts[i].aarch32.sctlr;
			KDInf.Cpu[i].Tcr	= (TSIZE)((CTRLBLK_V8 *)pBuf)->exts[i].aarch32.ttbcr;
#endif
			if (pRegAry[REG_SP] && pRegAry[REG_SP] <= KDInf.Cpu[i].RegAry[REG_SP]) /* ѡ��ջ���, Ϊ�����ֳ� */
				continue;
			UpdateCoreCntx(i, pRegAry, pRegAry[REG_NUM], 0, 0, NULL);
		}
		break;
	case sizeof(CTRLBLK_V7):
		KDInf.PSAddr			= (TADDR)((CTRLBLK_V7 *)pBuf)->PhysOff;
		KDInf.CodeStart			= (TADDR)((CTRLBLK_V7 *)pBuf)->SText;
		KDInf.KExpInf.MmuBase	= (TADDR)((CTRLBLK_V7 *)pBuf)->MasterPgTbl;
		KDInf.KExpInf.MemMapAddr = (TADDR)((CTRLBLK_V7 *)pBuf)->MemMapAddr;
		KDInf.KExpInf.ModStart	= (TADDR)((CTRLBLK_V7 *)pBuf)->ModStart;
		for (i = 0; i < ARY_CNT(((CTRLBLK_V7 *)pBuf)->regs) && i < ARY_CNT(KDInf.Cpu); i++) {
			pRegAry = (void *)((CTRLBLK_V7 *)pBuf)->regs[i];
			if (!memcmp(pRegAry, &ZeroRegs, sizeof(ZeroRegs))) /* ������ */
				continue;
			if (pRegAry[REG_SP] && pRegAry[REG_SP] <= KDInf.Cpu[i].RegAry[REG_SP]) /* ѡ��ջ���, Ϊ�����ֳ� */
				continue;
			UpdateCoreCntx(i, pRegAry, pRegAry[REG_NUM], 0, 0, NULL);
		}
		break;
	default:;
	}
}

/*************************************************************************
* DESCRIPTION
 *	��ȡmrdump��Ϣ; ��ȡPT_NOTE���� Name:MRDUMP01  TYPE:0xaee0
 *
*
* PARAMETERS
 *	pStr: ������ MRDUMP01 offset + sizeof struct{namesize, descriptsz, type} :pnameAddr
 *	pBuf: buffer	,Descrip addr
 *	Size: ��С(�ֽ�)	,description size
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ReadMrInf(const char *pStr, const void *pBuf, TUINT Size, void *pArg)
{
#ifdef SST64
#define HIGH_BASE 0xFFFFFFC000000000
#else
#define HIGH_BASE 0
#endif
	typedef const struct _MACHDESC_V3
	{
		TUINT Flags, CpuCnt;							//0x00000000, 0x00000008
		unsigned long long PhysOff, TotalMem;				//0x0000000040000000
		unsigned long long PageOff, HighMem, VmStart; //0xffffffc000000000  0xffffffc100000000  0xffffff8008000000
		unsigned long long ModStart, ModEnd; //module start0xffffff8000000000 , module end 0xffffff8008000000
		unsigned long long VMlcStart, VMlcEnd; /* vmalloc:0xffffff8008000000, 0xffffffbdbfff0000*/
		unsigned long long MasterPgTbl, DfdMemPa;	  //0x0000000041639000,	0
	} MACHDESC_V3; /* Description size 0x60,64bit,for Kernel-4.4 */
	typedef const struct _MACHDESC_V2
	{
		TUINT Flags, CpuCnt;
		unsigned long long PhysOff, TotalMem;
		unsigned long long PageOff, HighMem;
		unsigned long long ModStart, ModEnd;
		unsigned long long VMlcStart, VMlcEnd;
		unsigned long long MasterPgTbl;
	} MACHDESC_V2;/* Description size 0x50 */
	typedef const struct _MACHDESC_V1
	{
		TUINT Flags;
		TUINT PhysOff, TotalMem;
		TUINT PageOff, HighMem;
		TUINT ModStart, ModEnd;
		TUINT VMlcStart, VMlcEnd;
	} MACHDESC_V1; /* Description size 0x28 */
	const CD_ARG * const pCdArg = pArg;
	TSIZE Off;

	switch (Size) {
	case sizeof(MACHDESC_V3): /* N�汾 */
		KDInf.PSAddr = (TADDR) ((MACHDESC_V3 *) pBuf)->PhysOff; /* pthysical addr 0x40000000 */
		if ((TADDR)((MACHDESC_V3 *)pBuf)->VmStart) /* 4.4��ǰ��KIMAGE_ADDR */
			KDInf.CodeStart = (TADDR) ((MACHDESC_V3 *) pBuf)->VmStart + KVM_OFF; /* module start + KVMoff */
		KDInf.KExpInf.MmuBase	= (TADDR)((MACHDESC_V3 *)pBuf)->MasterPgTbl;
		KDInf.KExpInf.ModStart	= (TADDR)((MACHDESC_V3 *)pBuf)->ModStart;
		return 0;
	case sizeof(MACHDESC_V2):
		KDInf.PSAddr = (TADDR)((MACHDESC_V2 *)pBuf)->PhysOff;
		KDInf.KExpInf.MmuBase	= (TADDR)(((MACHDESC_V2 *)pBuf)->MasterPgTbl - ((MACHDESC_V2 *)pBuf)->PageOff + KDInf.PSAddr);
		KDInf.KExpInf.ModStart	= (TADDR)((MACHDESC_V2 *)pBuf)->ModStart;
		return 0;
	case sizeof(MACHDESC_V1):
		KDInf.PSAddr = ((MACHDESC_V1 *)pBuf)->PhysOff;
		KDInf.KExpInf.ModStart = (TADDR)((MACHDESC_V1 *)pBuf)->ModStart;
		return 0;
	case 8: /* O�汾 Description size �̶�8�� */
		if (!strcmp(pStr, "MRDUMP08"))
			Size = CTRLBLK_V8_SZ;
		else
			goto _FAIL;
		Off = Phy2Off(pCdArg->pElfHdl, OFFVAL_ADDR(pBuf, 0), Size);
		if (!Off || (pBuf = FpMapRo(pCdArg->pFpHdl, Off, Size)) == NULL)
			goto _FAIL;
		ExtractCtrlBlock(pBuf, Size);
		FpUnmap(pCdArg->pFpHdl, pBuf, Size);
		return 0;
	default:;
	}
	SSTERR(RS_SEG_SIZE, KDInf.KExpInf.pCoreFile, "NT_MRINFO", Size);
	return 1;
_FAIL:
	SSTWARN(RS_CTRLTBL, pStr);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	��ȡnote����Ϣ
*
* PARAMETERS
*	Type: NT_*��־
*	pStr: ����, �뿪�ú������ַ��Ч!!!
*	pBuf: buffer, �뿪�ú������ַ��Ч!!!
*	Size: buffer��С(�ֽ�)
*	pArg: ����
*
* RETURNS
*	0: ����
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ParseNote(TUINT Type, const char *pStr, const void *pBuf, TUINT Size, void *pArg)
{
	int rt = 0;

	switch (Type) {
	case NT_PRSTATUS:
		rt = ReadCoreCntxInf(pStr, pBuf, Size);
		break;
	case NT_PRPSINFO:
		break; //���Ե�һ��
	case NT_MRINFO: case NT_CTRLBLK: rt = ReadMrInf(pStr, pBuf, Size, pArg); break;
	case NT_IPANIC_MISC:
	default:
		SSTERR(RS_UNKNOWN_TYPE, "NOTE", Type);
		rt = 1;
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	��ʾ��ƥ��������Ϣ
*
* PARAMETERS
*	fp: �쳣��Ϣ������
*	p1/p2: buffer
*	VAddr: ��ʼ�����ַ
*	Cnt: ��λ: ��
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void DispDifMrRegion(FILE *fp, const unsigned *p1, const unsigned *p2, TADDR VAddr, size_t Cnt)
{
#define DISP_LEN	16 /* ��λ: �� */
	size_t i;

	SSTERR(RS_UNMATCH_DATA, VAddr, VAddr + Cnt * sizeof(*p1));
	LOG(fp, RS_CMP_REGION, VAddr, Cnt * sizeof(*p1));
	if (Cnt > DISP_LEN)
		Cnt = DISP_LEN;
	for (i = 0; i < Cnt; i++) {
		if (!(i&0x07))
			LOGC(fp, "\n");
		LOGC(fp, " %08X", p1[i]);
	}
	LOGC(fp, "\n");
	for (i = 0; i < Cnt; i++) {
		if (!(i&0x07))
			LOGC(fp, "\n");
		LOGC(fp, " %08X", p2[i]);
	}
	LOGC(fp, "\n\n");
}

/*************************************************************************
* DESCRIPTION
*	�Ա��ص���������, ��һ����ٱ�
*
* PARAMETERS
*	Addr/Size: �ص���ַ�ʹ�С(�ֽ�)
*	pBuf1/pBuf2: 2��buffer
*	pArg: ����
*
* RETURNS
*	0: ����
*	1: �쳣
*
* LOCAL AFFECTED
*
*************************************************************************/
static int RegionCmp(TADDR Addr, TSIZE Size, const void *pBuf1, const void *pBuf2, void *pArg)
{
#define CRANGE  4 /* ��λ: �� */
	const unsigned * const p = pBuf1, * const q = pBuf2;
	const size_t Cnt = (size_t)(Size / sizeof(*p));
	size_t i, DifStart = 0, DifEnd = 0;
	int HasDif = 0;

	if (!p || !q || !memcmp(p, q, (size_t)Size))
		return 0;
	for (i = 0; i < Cnt; i++) {
		if (p[i] == q[i])
			continue;
		if (DifEnd < i || !DifEnd) {
			if (HasDif)
				DispDifMrRegion(pArg, &p[DifStart], &q[DifStart], Addr + DifStart * sizeof(*p), DifEnd - (DifStart + CRANGE - 1));
			HasDif = 1;
			DifStart = i;
		}
		DifEnd = i + CRANGE;
	}
	if (HasDif)
		DispDifMrRegion(pArg, &p[DifStart], &q[DifStart], Addr + DifStart * sizeof(*p), DifEnd - (DifStart + CRANGE - 1));
	return 0; /* ��ʱ���Բ�һ������ */
}

/*************************************************************************
* DESCRIPTION
*	�����ڴ�������Ϣ
*
* PARAMETERS
*	pMap: mmuӳ����Ϣ
*	pElfHdl: elf���
 *	pArg: ���� (eg:fp)
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MrSetMemRegion(const MMU_MAP *pMap, ELF_HDL *pElfHdl, void *pArg)
{
#define SPLIT_SZ ((TSIZE)1 << 27) /* 128M Ϊ������pc������dump�е�ַ����ֱ��ӳ����������memory���µ��쳣������������mem �и� */
	TADDR Addr = pMap->Va, EAddr, tAddr;
	TSIZE Size = pMap->Sz, FOff;

	FOff = Phy2Off(pElfHdl, pMap->Pa, Size);
	if (!FOff && (FOff = Phy2Off(pElfHdl, pMap->Pa, PAGE_SIZE)) != 0)
		Size = (FOff + Size - 1 >= 0xFFF03FFF ? 0xFFF04000 - FOff : PAGE_SIZE); /* 32bit aee����4G-1M������ */
	if (!FOff || Size <= SPLIT_SZ)
		return (CreateMemRegion(Addr, Size, pMap->Attr, FOff, NULL, RegionCmp, pArg) == 1);
	for (tAddr = (Addr + SPLIT_SZ)&~(SPLIT_SZ - 1), EAddr = pMap->Va + Size;
	  tAddr - 1 <= EAddr - 1; tAddr += SPLIT_SZ, Addr += Size, FOff += Size) {
		Size = tAddr - Addr;
		if (CreateMemRegion(Addr, Size, pMap->Attr, FOff, NULL, RegionCmp, pArg) == 1)
			return 1;
	}
	return (EAddr != Addr ? CreateMemRegion(Addr, EAddr - Addr, pMap->Attr, FOff, NULL, RegionCmp, pArg) == 1 : 0);
}

/*************************************************************************
* DESCRIPTION
*	�����ڴ�������Ϣ
*
* PARAMETERS
*	Flag: PF_*��־
*	FOff: �ļ�ƫ����(�ֽ�)
*	Size: ռ���ļ���С(�ֽ�)
*	Va/Pa: ����/�����ַ
*	pArg: ����
*
* RETURNS
*	0: ����
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int MiSetMemRegion(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg)
{
	(void)Flag, (void)Pa;
	if (Va&~PAGE_MASK) /* �����ز������region, ����control block */
		return 0;
	return CreateMemRegion(Va, Size, VMA_R|VMA_W|VMA_X, FOff, NULL, RegionCmp, pArg) == 1;
}

/*************************************************************************
* DESCRIPTION
*	Ѱ��control block
*
* PARAMETERS
*	Flag: PF_*��־
*	FOff: �ļ�ƫ����(�ֽ�)
*	Size: ռ���ļ���С(�ֽ�)
*	Va/Pa: ����/�����ַ
*	pArg: ����
*
* RETURNS
*	0: ����
*	1: ���
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindCtrlBlock(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg)
{
	CD_ARG * const pCdArg = pArg;
	const char *pBuf;
	TUINT Idx, Sz;
	char Str[9];

	(void)Flag, (void)Va, (void)Pa, (void)Pa;
	pBuf = FpMapRo(pCdArg->pFpHdl, FOff, Size);
	if (!pBuf)
		return 0;
	for (Sz = Idx = 0; Idx < Size; Idx += 1024) {
		if ((pBuf[Idx] != 'M' && pBuf[Idx] != 'X') || strncmp(&pBuf[Idx + 1], "RDUMP", sizeof("RDUMP") - 1))
			continue;
		switch (OFFVAL_U2(pBuf, Idx + 6)) {
		case 0x3830: Sz = CTRLBLK_V8_SZ; break;
		case 0x3730: Sz = CTRLBLK_V7_SZ; break;
		default: memcpy(Str, &pBuf[Idx], 8); Str[8] = '\0'; SSTWARN(RS_CTRLTBL, Str);
		}
		if (Sz) {
			ExtractCtrlBlock(&pBuf[Idx], Sz);
			FpUnmap(pCdArg->pFpHdl, pBuf, Size);
			return 1;
		}
	}
	FpUnmap(pCdArg->pFpHdl, pBuf, Size);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	ѭ��mmuҳ��λ��
*
* PARAMETERS
*	pFpHdl: fp���
*	pElfHdl: elf���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindMmuBase(FP_HDL *pFpHdl, ELF_HDL *pElfHdl)
{
#ifdef SST64
#define FIND_MMU_SZ	 0x2000
	const void *pInst;
	int Off, i, rt = 1;
	TSIZE FOff;

	if (KDInf.KExpInf.MmuBase)
		return 0;
	FOff = Phy2Off(pElfHdl, KDInf.PSAddr + KVM_OFF, FIND_MMU_SZ);
	if (!FOff || (pInst = FpMapRo(pFpHdl, FOff, FIND_MMU_SZ)) == NULL)
		goto _FAIL;
	for (i = 8; i < FIND_MMU_SZ - 8; i += 4) {
		/* LDR X26, [PC, #immed], LDR X25, [PC, #immed], ADD X26, X26, X28, ADD X25, X25, X28. >=3.18 < 4.4 */
		if (OFFVAL_U8(pInst, i) == 0x8B1C03398B1C035A && (OFFVAL_U8(pInst, i - 8)&0xFF00001FFF00001F) == 0x580000195800001A) {
			Off = i - 8 + ((OFFVAL_U4(pInst, i - 8)&0xFFFFE0) >> 3);
			if (Off > FIND_MMU_SZ - 4)
				continue;
			KDInf.KExpInf.MmuBase = KDInf.PSAddr + OFFVAL_U4(pInst, Off);
			rt = 0;
			break;
		}
		if (OFFVAL_U8(pInst, i) == 0xD1400B599141F71A) { /* ADD X26, X24, #0x7D000, SUB X25, X26, #0x2000. <3.18 */
			KDInf.KExpInf.MmuBase = KDInf.PSAddr + 0x7D000;
			rt = 0;
			break;
		}
	}
	FpUnmap(pFpHdl, pInst, FIND_MMU_SZ);
_FAIL:
	if (rt)
		SSTERR(RS_MMU_BASE);
	return rt;
#else
	(void)pFpHdl, (void)pElfHdl;
	if (!KDInf.KExpInf.MmuBase)
		KDInf.KExpInf.MmuBase = KDInf.PSAddr + 0x4000;
	return 0;
#endif
}

#ifndef SST64
/*************************************************************************
* DESCRIPTION
*	����ҳ��(LPAE�汾)
*
* PARAMETERS
*	MmuBase: �ں�ҳ�������ַ
*	pFpHdl: fp���
*	pElfHdl: elf���
*	fp: �쳣������
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ForEachMmuPageLpae(TADDR MmuBase, FP_HDL *pFpHdl, ELF_HDL *pElfHdl, FILE *fp)
{
#define VADDR_BASE_L 0
#define PADDR_MASK_L 0xFFFFFFFFFF /* 40bit PA */
#define ISFAULT_L(x) (!((x)&1))
#define ISTABLE_L(x) ((x)&2)
	const unsigned long long *pTbl[] = {NULL, NULL, NULL};
	TUINT Idx[] = {0, 0, (START_KERNEL >> 30) - 1};
	int i = ARY_CNT(pTbl) - 1, j, rt = 1;
	unsigned long long Entry = MmuBase;
	MMU_MAP tMap, Map = {1, 0, 0, 0};
	const TUINT Bits[] = {9, 9, 2};
	TSIZE FOff;
	TUINT Sz;

	goto _START;
	do {
		while (!(++Idx[i] >> Bits[i])) {
			Entry = pTbl[i][Idx[i]];
			if (!ISFAULT_L(Entry)) {
				if (ISTABLE_L(Entry) && i > 0) { /* table */
					Idx[--i] = ~(TUINT)0; /* reset */
_START:				Sz = (1 << Bits[i]) * sizeof(**pTbl);
					if (pTbl[i])
						FpUnmap(pFpHdl, pTbl[i], Sz);
					FOff = Phy2Off(pElfHdl, Entry&PADDR_MASK_L&~(unsigned long long)(Sz - 1), Sz);
					if (FOff && (pTbl[i] = FpMapRo(pFpHdl, FOff, Sz)) != NULL) {
						i--; /* ����i++ */
						break;
					}
					SSTERR(RS_EADDR, KDInf.KExpInf.pCoreFile, "MMU table", Entry&PADDR_MASK_L&~(unsigned long long)(Sz - 1));
					pTbl[i] = NULL;
					tMap.Pa = 1;
					if (/* �������table */++i >= (int)ARY_CNT(pTbl))
						goto _ERR;
					goto _BYPASS_TBL;
				}
				for (tMap.Va = VADDR_BASE_L, Sz = ADDR_BITS, j = ARY_CNT(pTbl) - 1; j >= i; j--) {
					Sz -= Bits[j];
					tMap.Va += ((TADDR)Idx[j] << Sz);
				}
				tMap.Sz = (TSIZE)1 << Sz;
				tMap.Pa = Entry&PADDR_MASK_L&~(tMap.Sz - 1);
				if ((Entry >> 40)&0x7FF) { /* RES0 */
					SSTERR(RS_MMU_ENTRY, Entry, tMap.Va);
					tMap.Pa = 1;
					goto _BYPASS_TBL;
				}
				tMap.Attr = (VMA_R|(unsigned)FLAG_TRANS(Entry, 1 << 7, VMA_W)|(unsigned)FLAG_TRANS(Entry, 1ULL << 53, VMA_X))^(VMA_W|VMA_X);
				if (Map.Pa + Map.Sz == tMap.Pa && Map.Attr == tMap.Attr) { /* �ϲ� */
					Map.Sz += tMap.Sz;
				} else {
_BYPASS_TBL:		if (Map.Pa != 1 && MrSetMemRegion(&Map, pElfHdl, fp))
						goto _ERR;
					Map = tMap;
				}
			} else if (Map.Pa != 1) { /* fault */
				if (MrSetMemRegion(&Map, pElfHdl, fp))
					goto _ERR;
				Map.Pa = 1; /* ��Ч */
			}
		}
	} while (++i <= (int)ARY_CNT(pTbl) - 1);
	if (Map.Pa != 1 && MrSetMemRegion(&Map, pElfHdl, fp))
		goto _ERR;
	rt = 0;
_ERR:
	for (i = 0; i < (int)ARY_CNT(pTbl); i++) {
		if (pTbl[i])
			FpUnmap(pFpHdl, pTbl[i], (1 << Bits[i]) * sizeof(**pTbl));
	}
	return rt;
}
#endif

/*************************************************************************
* DESCRIPTION
 *	����ҳ�� ����VMA
*
* PARAMETERS
*	MmuBase: �ں�ҳ�������ַ
*	pFpHdl: fp���
*	pElfHdl: elf���
*	fp: �쳣������
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ForEachMmuPage(TADDR MmuBase, FP_HDL *pFpHdl, ELF_HDL *pElfHdl, FILE *fp)
{
	MMU_MAP tMap, Map = {1, 0, 0, 0};
#ifdef SST64
#define VADDR_BASE START_KERNEL
#define PADDR_MASK 0xFFFFFFFFFFFF /* 48bit PA */
#define ISFAULT(x) (!((x)&1))
#define ISTABLE(x) ((x)&2)
	const TSIZE *pTbl[] = {NULL, NULL, NULL};
	const TUINT Bits[] = {9, 9, 9};
	TUINT Idx[] = {0, 0, ~(TUINT)0};
#else
#define VADDR_BASE 0
#define PADDR_MASK 0xFFFFFFFF
#define ISFAULT(x) (!((x)&3))
#define ISTABLE(x) ((x)&1)
	const TSIZE *pTbl[] = {NULL, NULL};
	const TUINT Bits[] = {8, 12};
	TUINT Idx[] = {0, (START_KERNEL >> 20) - 1};
#endif
	int i = ARY_CNT(pTbl) - 1, j, rt = 1;
	TSIZE Entry = MmuBase, FOff;
	TADDR KasanZeroPa = 2;
	TUINT Sz;

	(void)KasanZeroPa;
#ifndef SST64
	if ((MmuBase&0xFFFF) == 0x3000) /* LPAEһ����0xyyyy3000 */
		return ForEachMmuPageLpae(MmuBase, pFpHdl, pElfHdl, fp);
#endif
	goto _START;
	do {
		while (!(++Idx[i] >> Bits[i])) {
			Entry = pTbl[i][Idx[i]];
			if (!ISFAULT(Entry)) { /* ҳ����bit0:1 ��Ч��0 ��Ч*/
				if (ISTABLE(Entry) && i > 0) { /* ҳ����bit1:1 ��ʾΪtable��0 ��ʾֱ��ָ��PA */
					Idx[--i] = ~(TUINT)0; /* reset */
_START:				Sz = (1 << Bits[i]) * sizeof(**pTbl);
					if (pTbl[i])
						FpUnmap(pFpHdl, pTbl[i], Sz);
					FOff = Phy2Off(pElfHdl, Entry&PADDR_MASK&~(TSIZE)(Sz - 1), Sz);
					if (FOff && (pTbl[i] = FpMapRo(pFpHdl, FOff, Sz)) != NULL) {
						i--; /* ����i++ */
						break;
					}
					SSTERR(RS_EADDR, KDInf.KExpInf.pCoreFile, "MMU table", Entry&PADDR_MASK&~(TSIZE)(Sz - 1));
					pTbl[i] = NULL;
					tMap.Pa = 1;
					if (/* �������table */++i >= (int)ARY_CNT(pTbl))
						goto _ERR;
					goto _BYPASS_TBL;
				}
				for (tMap.Va = VADDR_BASE, Sz = ADDR_BITS, j = ARY_CNT(pTbl) - 1;
						j >= i/* according to tabel ,generate VMA section */;
						j--) {
					Sz -= Bits[j];
					tMap.Va += ((TADDR)Idx[j] << Sz);
				}/* sz:1G, 2M, 4KB */
				tMap.Sz = (TSIZE)1 << Sz;
				tMap.Pa = Entry&PADDR_MASK&~(tMap.Sz - 1);
#ifdef SST64
				if (tMap.Va == (~(TADDR)0 << ADDR_BITS))
					KasanZeroPa = tMap.Pa;
				if (tMap.Pa == KasanZeroPa)
					tMap.Pa = 2; /* �������ֵ��������ӳ�� */
				if ((Entry >> 48)&0x07) { /* RES0 */
					SSTERR(RS_MMU_ENTRY, Entry, tMap.Va);
					tMap.Pa = 1;
					goto _BYPASS_TBL;
				}
				/* MMUVMA attribute��ҳ������entry bit7 not writer�� bit53 not execute�� ����ARM64������ */
				tMap.Attr = (VMA_R|(unsigned)FLAG_TRANS(Entry, 1 << 7, VMA_W)|(unsigned)FLAG_TRANS(Entry, (TSIZE)1 << 53, VMA_X))^(VMA_W|VMA_X);
#else
				tMap.Attr = (VMA_R|(i ? FLAG_TRANS(Entry, 1 << 15, VMA_W)|FLAG_TRANS(Entry, 1 << 4, VMA_X)
					: FLAG_TRANS(Entry, 1 << 9, VMA_W)|FLAG_TRANS(Entry, 1 << 0, VMA_X)))^(VMA_W|VMA_X);
#endif
				/* VMA����MMUTable����������������һ��MAP�Ƚϼ��ɣ�VMA ����  ���� KasanZeroMap   ��������ͬʱ��ϲ� */
				if ((Map.Pa + Map.Sz == tMap.Pa || (tMap.Pa == Map.Pa && Map.Pa == 2)) && Map.Attr == tMap.Attr) { /* �ϲ� */
					Map.Sz += tMap.Sz;
				} else {
					_BYPASS_TBL: if (Map.Pa != 1
							&& MrSetMemRegion(&Map, pElfHdl, fp))
						goto _ERR;
					Map = tMap;
				}
			} else if (Map.Pa != 1) { /* fault */
				if (MrSetMemRegion(&Map, pElfHdl, fp))
					goto _ERR;
				Map.Pa = 1; /* ��Ч */
			}
		}
	} while (++i <= (int)ARY_CNT(pTbl) - 1);
	if (Map.Pa != 1 && MrSetMemRegion(&Map, pElfHdl, fp))
		goto _ERR;
	rt = 0;
_ERR:
	for (i = 0; i < (int)ARY_CNT(pTbl); i++) {
		if (pTbl[i])
			FpUnmap(pFpHdl, pTbl[i], (1 << Bits[i]) * sizeof(**pTbl));
	}
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	����coredump����ȡnote��
*
* PARAMETERS
 *	pFpHdl: fp��� , SYS_COREDUMP ,FP_INT->ex  export info
 *	fp: �쳣��Ϣ����ļ����, �������档txt
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	2: ��mem region���޼Ĵ�����Ϣ
*
* LOCAL AFFECTED
*
*************************************************************************/
static int LoadCoreDump(FP_HDL *pFpHdl, FILE *fp)
{
	CD_ARG CdArg;
	TADDR Addr;
	TSIZE Off;
	int rt = 1;

	CdArg.pFpHdl = pFpHdl;
	CdArg.pElfHdl = GetElfHdl(pFpHdl, ET_CORE); // elf file ELF_INT *pelf
	if (!CdArg.pElfHdl)
		return rt;
	if (ElfForEachNote(CdArg.pElfHdl, ParseNote, &CdArg)) //parsar PT_NOTE COREx register, KDINF
		goto _ERR;
	KDInf.KExpInf.ValidBit = KDInf.CoreVMap;
	if (KDInf.KExpInf.mType == MRD_T_MRDUMP) {
		if (FindMmuBase(pFpHdl, CdArg.pElfHdl)
				|| ForEachMmuPage(KDInf.KExpInf.MmuBase, pFpHdl, CdArg.pElfHdl,
						fp)/* ����MMU ����VMA */)
			goto _ERR;
	} else {/* minidump */
		if (!KDInf.KExpInf.MemMapAddr)
			ElfForEachLoad(CdArg.pElfHdl, FindCtrlBlock, &CdArg);
		if (ElfForEachLoad(CdArg.pElfHdl, MiSetMemRegion, fp))
			goto _ERR;
	}
	if (!KDInf.KExpInf.CoreCnt) { //û�мĴ�����Ϣ�� note ������ ����Ƿ���PT_LOAD ����ȡcoreContext ExtrctControlInof�������
		if (ElfGetSegInf(CdArg.pElfHdl, &Addr, &Off, PT_LOAD, 0)) /* ����Ƿ���PT_LOAD */
			rt = 2;
		SSTWARN(RS_NOCOREINF, pFpHdl->pPath);
		goto _ERR;
	}
	rt = 0;
_ERR:
	PutElfHdl(CdArg.pElfHdl);
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	��ʼ��moduleģ��
*
* PARAMETERS
*	ModAddr: module��ַ
*	pArg: �ļ����
*
* RETURNS
*	0: ����
*	1: �쳣
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SetupModFromDump(TADDR ModAddr, void *pArg)
{
	(void)pArg;
	if (ModuleSymInit(ModAddr, 0, 0, NULL) && KDInf.KExpInf.mType != MRD_T_MRDUMP)
		return 1;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	��SYS_MODULES_INFO��ʼ��moduleģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void SetupModsFromFile(void)
{
	char data[1024 * 4], *p;
	const char *pName;
	TADDR LoadBias;
	TUINT Size;
	FILE *fp;

	fp = fopen("SYS_MODULES_INFO", "r");
	if (!fp)
		return;
	p = fgets(data, sizeof(data), fp);
	fclose(fp);
	if (!p || strncmp(p, "Modules linked in:", sizeof("Modules linked in:") - 1))
		return;
	for (p += sizeof("Modules linked in:") - 1; p; GetSubStr(&p, ' ')/* ����init_size */) {
		pName = GetSubStr(&p, ' ');
		if (pName[0] == '(') /* ����module_flags */
			pName = GetSubStr(&p, ' ');
		if (pName[0] == '[') /* ����������[last unloaded: */
			break;
		if (sscanf(GetSubStr(&p, ' '), "%"TLONGFMT"x", &LoadBias) != 1)
			break;
		GetSubStr(&p, ' '); /* ����module_init */
		if (sscanf(GetSubStr(&p, ' '), "%d", &Size) != 1 || !(LoadBias && Size))
			break;
		ModuleSymInit(0, LoadBias, Size, pName);
	}
}

/*************************************************************************
* DESCRIPTION
 *	��ʼ��coredump������sym�ļ���Ϣ, load vmlinux ���ӵ��ڴ��
*
* PARAMETERS
*	fp: �쳣������
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int SetupSymfiles(FILE *fp)
{
	TADDR ModListAddr;

	if (PrepareSymFiles(KDInf.KExpInf.CoreCnt, KDInf.CodeStart,
			fp)/*parser vmlinux*/)
		return 1;
	ModListAddr = SymName2Addr(pVmSHdl, NULL, "modules", TLCLVAR);
	if (ModListAddr && !ForEachList(ModListAddr, sizeof(TSIZE), SetupModFromDump, NULL))
		return 0;
	SetupModsFromFile();
	return 0;
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡKE�����Ϣ
*
* PARAMETERS
*	��
*
* RETURNS
*	KE�����Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
KEXP_INF *GetKExpInf(void)
{
	return &KDInf.KExpInf;
}

/*************************************************************************
* DESCRIPTION
*	��ȡ��ӦCPU�˵ĵ�ǰ����������Ϣ
*
* PARAMETERS
*	CpuId: CPU id
*
* RETURNS
*	��Ӧ��������Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
CORE_CNTX *GetCoreCntx(TUINT CpuId)
{
	return CpuId < sizeof(KDInf.CoreVMap) * 8 && KDInf.CoreVMap&(1 << CpuId) ? &KDInf.Cpu[CpuId] : NULL;
}

/*************************************************************************
* DESCRIPTION
*	����TEE��Ǹ�ָ����CPU
*
* PARAMETERS
*	CpuId: CPU id
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SetCoreInSec(TUINT CpuId)
{
	if (CpuId < MAX_CORE)
		KDInf.Cpu[CpuId].InSec = 1;
}

/*************************************************************************
* DESCRIPTION
*	����ָ����CPU��Ϣ
*
* PARAMETERS
*	CpuId: CPU id
*	pRegAry/Cpsr: CPU�Ĵ�����ΪNULL�򲻸���
*	TskAddr: task_struct��ַ��Ϊ0�򲻸���
*	Pid: ���̱�ʶ��
*	pProcName: ������, ΪNULL�򲻸���ProcName��Pid
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateCoreCntx(TUINT CpuId, const TSIZE *pRegAry, TSIZE Cpsr, TADDR TskAddr, TUINT Pid, const char *pProcName)
{
	if (CpuId >= MAX_CORE)
		return;
	if (KDInf.KExpInf.CoreCnt < CpuId + 1)
		KDInf.KExpInf.CoreCnt = CpuId + 1;
	if (pRegAry) {
		if (!(KDInf.CoreVMap&(1 << CpuId))) {
			KDInf.CoreVMap |= (1 << CpuId);
			strcpy(KDInf.Cpu[CpuId].ProcName, "(null)");
		}
		memcpy(KDInf.Cpu[CpuId].RegAry, pRegAry, sizeof(KDInf.Cpu[0].RegAry));
#ifdef SST64
		if (Cpsr >> 32) /* �쳣���� */
			Cpsr = 0x05;
#endif
		KDInf.Cpu[CpuId].Cpsr = Cpsr;
		if (!KDInf.Cpu[CpuId].Sctlr) { /* ExtractCtrlBlock()û���趨, ��ʹ��Ĭ��ֵ */
#ifdef SST64
			KDInf.Cpu[CpuId].Tcr = 0x32B5193519; /* Trans Ctrl register ,���忴spec ��trace32 register*/
			KDInf.Cpu[CpuId].Sctlr = 0x4C5D93D; /* system ctrl register */
#else
			KDInf.Cpu[CpuId].Tcr = 0x35003500;
			if ((KDInf.KExpInf.MmuBase&0xFFFF) == 0x3000) /* ����LPAE */
				KDInf.Cpu[CpuId].Tcr |= 0x80000000;
			KDInf.Cpu[CpuId].Sctlr	= 0x30C5383D;
#endif
		}
	}
	if (pProcName) {
		KDInf.Cpu[CpuId].Pid = Pid;
		KDInf.Cpu[CpuId].ProcName[sizeof(KDInf.Cpu[0].ProcName) - 1] = '\0';
		snprintf(KDInf.Cpu[CpuId].ProcName, sizeof(KDInf.Cpu[0].ProcName) - 1, "%s", pProcName);
	}
	if (TskAddr)
		KDInf.Cpu[CpuId].TskAddr = TskAddr;
}

/*************************************************************************
* DESCRIPTION
*	����HWT��Ϣ
*
* PARAMETERS
*	KickBit: ι��CPUλͼ
*	ValidBit: ����CPUλͼ
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateHwtInf(TUINT KickBit, TUINT ValidBit)
{
	KDInf.KExpInf.KickBit = KickBit;
	KDInf.KExpInf.ValidBit = ValidBit;
}

/*************************************************************************
* DESCRIPTION
*	�����쳣kernelʱ��
*
* PARAMETERS
*	Sec/USec: ��/΢��, ��Ϊ0�򲻸���!!!
*	eType: �쳣����
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateExcpTime(TUINT Sec, TUINT USec, EXP_TYPE eType)
{
	if (!(Sec|USec)) {
		KDInf.KExpInf.eType = eType;
		return;
	}
	if (KDInf.KExpInf.Sec - 1 < Sec - 1 || (KDInf.KExpInf.Sec == Sec && KDInf.KExpInf.USec - 1 < USec - 1))
		return;
	/* ��ֹ�ӳ�ʼHWT���segv */
	if ((KDInf.KExpInf.Sec|KDInf.KExpInf.USec) || KDInf.KExpInf.eType != EXP_HWT || eType != EXP_SEGV)
		KDInf.KExpInf.eType = eType;
	KDInf.KExpInf.Sec   = Sec;
	KDInf.KExpInf.USec  = USec;
}

/*************************************************************************
* DESCRIPTION
*	�����쳣utcʱ��
*
* PARAMETERS
*	pExcpUtc: �쳣ʱ���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void UpdateExcpUtc(const char *pExcpUtc)
{
	pExcpUtc = strdup(pExcpUtc);
	if (!pExcpUtc)
		return;
	Free((void *)KDInf.KExpInf.pExcpUtc);
	KDInf.KExpInf.pExcpUtc = pExcpUtc;
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��mrdumpģ��
*
* PARAMETERS
 *	fp: �쳣��Ϣ����ļ������ ��������
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PrepareMrdump(FILE *fp)
{
	FP_HDL *pFpHdl;
	int rt;

	KDInf.KExpInf.mType = MRD_T_MRDUMP;
	KDInf.KExpInf.pCoreFile = DEF_MRFILE;
	pFpHdl = GetFpHdl(KDInf.KExpInf.pCoreFile, 0);
	if (pFpHdl) {
		if (pFpHdl->FileSz > 1024 * 16)
			goto _LOAD;
		SSTWARN(RS_FILE_DAMAGED, pFpHdl->pPath);
		PutFpHdl(pFpHdl);
	}
_TRY_MI:
	KDInf.KExpInf.mType = MRD_T_MIDUMP;
	KDInf.KExpInf.pCoreFile = DEF_MIFILE;
	pFpHdl = GetFpHdl(KDInf.KExpInf.pCoreFile, 0);
	if (pFpHdl) {
_LOAD:  SSTINFO(RS_LOADING, KDInf.KExpInf.pCoreFile);
		PrepareMemSpace(pFpHdl);
		rt = LoadCoreDump(pFpHdl, fp); //0 success
		PutFpHdl(pFpHdl);
		if (rt) {
			if (KDInf.KExpInf.mType == MRD_T_MRDUMP) {
				DestoryMrdump();
				goto _TRY_MI;
			}
			if (rt != 2) {
				DestoryMrdump();
				KDInf.KExpInf.mType = MRD_T_NONE;
			}
		}
	} else {
		KDInf.KExpInf.mType = MRD_T_NONE;
		SSTERR(RS_FILE_NOT_EXIST, "dump");
	}
	if (SetupSymfiles(fp)) {
		DestoryMemSpace();
		return 1;
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	ע��mrdumpģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void DestoryMrdump(void)
{
	DestorySymFiles();
	DestoryMemSpace();
	Free((void *)KDInf.KExpInf.pExcpUtc);
	memset(&KDInf, 0, sizeof(KDInf));
}
