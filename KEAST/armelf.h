/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	armelf.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: arm elf��ض���
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
*	��ȡelf�����ָ�������ͺ����ƵĽ���Ϣ
*
* PARAMETERS
*	pElfHdl: elf���
*	pAddr: ���������ַ
*	pFOff: �����ļ�ƫ����(�ֽ�)
*	Type: SHT_*��־
*	pName: ������, NULL��ʾ������ƥ��
*
* RETURNS
*	0: ʧ��
*	����: �ڴ�С(�ֽ�)
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
*	��ȡelf����з��ű���Ϣ
*
* PARAMETERS
*	pElfHdl: elf���
*	pSymFOff: ���ű��ļ�ƫ����(�ֽ�)
*	pStrsFOff: �ַ������ļ�ƫ����(�ֽ�)
*	pStrsSz: �ַ������С(�ֽ�)
*	pGlbIdx: ȫ�ַ�������
*
* RETURNS
*	0: ʧ��
*	����: ���ű��С(�ֽ�)
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE ElfGetSymInf(ELF_HDL *pElfHdl, TSIZE *pSymFOff, TSIZE *pStrsFOff, TSIZE *pStrsSz, TUINT *pGlbIdx);

/*************************************************************************
* DESCRIPTION
*	��ȡelf�����ָ�������͵Ķ���Ϣ
*
* PARAMETERS
*	pElfHdl: elf���
*	pAddr: ���������ַ
*	pFOff: �����ļ�ƫ����(�ֽ�)
*	Type: PT_*��־
*	Flag: PF_*��־
*
* RETURNS
*	0: ʧ��
*	����: �δ�С(�ֽ�)
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
*	����elf����Ŀɼ���sec��
*
* PARAMETERS
*	pElfHdl: elf���
*	pFunc: �ص�����
*		pSecName: ������
*		Flag: PF_*��־
*		FOff: �ļ�ƫ����(�ֽ�)
*		Size: ռ���ļ���С(�ֽ�)
*		Addr: �����ַ
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define PF_X	(1 << 0) /* �ο�ִ�� */
#define PF_W	(1 << 1) /* �ο�д */
#define PF_R	(1 << 2) /* �οɶ� */
int ElfForEachSec(ELF_HDL *pElfHdl
	, int (*pFunc)(const char *pSecName, TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Addr, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	����elf�����load��
*
* PARAMETERS
*	pElfHdl: elf���
*	pFunc: �ص�����
*		Flag: PF_*��־
*		FOff: �ļ�ƫ����(�ֽ�)
*		Size: ռ���ļ���С(�ֽ�)
*		Va/Pa: ����/�����ַ
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ElfForEachLoad(ELF_HDL *pElfHdl
	, int (*pFunc)(TUINT Flag, TSIZE FOff, TSIZE Size, TADDR Va, TADDR Pa, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	����elf�����note��
*	ע��: ELF�ļ�����ΪET_COREʱ����note��!!!
*
* PARAMETERS
*	pElfHdl: elf���
*	pFunc: �ص�����
*		Type: NT_*��־
*		pName: ����, �뿪�ú������ַ��Ч!!!
*		pBuf: buffer, �뿪�ú������ַ��Ч!!!
*		Size: buffer��С(�ֽ�)
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	����: �ص���������ֵ
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
*	��¼һ��������Ϣ��ֻ������CreateSymFile()�Ļص�·����!!!
*
* PARAMETERS
*	pName: ����
*	Val: ֵ
*	Size: ��С
*	Info: ������Ϣ
*	pArg: ����Ĳ���
*
* RETURNS
*	0: ����
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int BuildOneSym(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg);

/*************************************************************************
* DESCRIPTION
*	���ɷ����ļ�
*
* PARAMETERS
*	pFile: �����ļ�·��
*	BuildSyms: �ṩsym�Ļص�����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
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
*	����fp�����ȡelf���
*
* PARAMETERS
*	pFpHdl: fp���
*	Type: ����
*
* RETURNS
*	NULL: ʧ��
*	����: elf���
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define ET_REL	(1 << 1) /* �ض�λELF */
#define ET_EXEC	(1 << 2) /* ��ִ��ELF */
#define ET_DYN	(1 << 3) /* ����ELF */
#define ET_CORE	(1 << 4) /* �ڴ�ELF */
ELF_HDL *GetElfHdl(const void *pFpHdl, TUINT Type);

/*************************************************************************
* DESCRIPTION
*	�ͷ�elf���
*
* PARAMETERS
*	pElfHdl: elf���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutElfHdl(ELF_HDL *pElfHdl);

#endif
