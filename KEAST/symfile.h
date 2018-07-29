/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	symfile.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/5/22
* Description: �����ļ�����
**********************************************************************************/
#ifndef _SYMFILE_H
#define _SYMFILE_H

#include <ttypes.h>
#include <stdio.h>


#ifdef SST64
#define START_KERNEL	(~(TADDR)0 << ADDR_BITS)
#define KVM_OFF			0x80000
#define MOD_NAME_LEN	55 /* ������'\0', ����ʹ��sizeof!!! */
#else
#define START_KERNEL	0xBF000000
#define KVM_OFF			0x8000
#define MOD_NAME_LEN	59
#endif

typedef const struct _SYM_HDL
{
	TADDR LoadBias; /* LoadBias + vmlinux_vaddr = coredump addr */

#define SYM_LOAD	(1 << 0) /* �Ѽ��� */
#define SYM_MATCH	(1 << 1) /* �ļ�ƥ�� */
#define SYM_LCLSYM	(1 << 4) /* ���ھֲ�������Ϣ(����sym�ļ������, �п�����diff�ļ���) */
#define SYM_RBITS	14 /* Ԥ��bitλ��(�ڲ�ʹ��) */
#define SYM_NOTICE	(1 << SYM_RBITS) /* ����ʾ�� */
/* �����������bit��Ϣ */
	unsigned Flag;
	const char *pName; /* elf���֣�������Ŀ¼ */
} SYM_HDL;

extern SYM_HDL *pVmSHdl; /* vmlinux�����ļ���� */

typedef const struct _DBG_HDL
{
	void *u[5];
} DBG_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/* ����, ��elf����ͬ��!!! */
#define OPT_SHIFT	 0
#define OPT_NOTYPE	(1 << (OPT_SHIFT + 0)) /* ������ */
#define OPT_OBJECT	(1 << (OPT_SHIFT + 1)) /* ���� */
#define OPT_FUNC	((1 << (OPT_SHIFT + 2))|(1 << (OPT_SHIFT + 13))) /* ���� */
#define OPT_MASK(Op) ((Op)&0xFFFF)
/* ��Χ */
#define OPB_SHIFT	16
#define OPB_LOCAL	(1 << (OPB_SHIFT + 0)) /* �ֲ����� */
#define OPB_GLOBAL	(1 << (OPB_SHIFT + 1)) /* ȫ�ַ��� */
#define OPB_MASK(Op) ((Op) >> 16)

#define TGLBVAR		(OPB_GLOBAL|OPT_OBJECT) /* ȫ�ֱ��� */
#define TLCLVAR		(OPB_LOCAL|OPT_OBJECT) /* �ֲ����� */
#define TALLVAR		(OPB_LOCAL|OPB_GLOBAL|OPT_OBJECT) /* ���б��� */
#define TGLBFUN		(OPB_GLOBAL|OPT_FUNC) /* ȫ�ֺ��� */
#define TLCLFUN		(OPB_LOCAL|OPT_FUNC) /* �ֲ����� */
#define TALLFUN		(OPB_LOCAL|OPB_GLOBAL|OPT_FUNC) /* ���к��� */

/*************************************************************************
* DESCRIPTION
*	���ݷ������ƻ�ȡ��ַ
*
* PARAMETERS
*	pSymHdl: sym���
*	pSize: ��ΪNULLʱ���ط��Ŵ�С(�ֽ�), 0��ʾ��ȷ��
*	pName: ������
*	Op: ��ѯ����: OP*
*
* RETURNS
*	0: ʧ��/�޸÷���
*	����: ��ַ, ����Ǻ��������THUMB bit
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR SymName2Addr(SYM_HDL *pSymHdl, TSIZE *pSize, const char *pName, unsigned Op);

/*************************************************************************
* DESCRIPTION
*	�����ļ�����ȡdbg���
*
* PARAMETERS
*	pSymHdl: sym���
*	pDbgHdl: ����dbg���
*	pFileName: �ļ���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��/�޸�dbg��Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymFile2DbgHdl(SYM_HDL *pSymHdl, struct _DBG_HDL *pDbgHdl, const char *pFileName);

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡ���ڷ�����
*
* PARAMETERS
*	pSymHdl: sym���
*	pAddr: ��ΪNULLʱ���ط��ŵ�ַ, ����Ǻ��������THUMB bit
*	Addr: ��ַ, ����thumb�����׵�ַ������THUMB bit�Ҳ���!!!
*	Op: ��ѯ����: OP*
*
* RETURNS
*	NULL: ʧ��/�޸õ�ַ��Ӧ�ķ���
*	����: ��������pUndFuncName
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern const char * const pUndFuncName; /* δ֪�������� */
const char *SymAddr2Name(SYM_HDL *pSymHdl, TADDR *pAddr, TADDR Addr, unsigned Op);

/*************************************************************************
* DESCRIPTION
*	ͨ����ַ��ȡsym�����Ӧ·�����к�
*
* PARAMETERS
*	pSymHdl: sym���
*	pLineNo: �����к�
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: ·��������ʱ�����Free()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *SymAddr2Line(SYM_HDL *pSymHdl, TUINT *pLineNo, TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡǶ�׺�����Ϣ
*
* PARAMETERS
*	pSymHdl: sym���
*	Addr: ������ַ
*	pFunc: �ص�����, ��Ƕ��˳��ص�
*		pSymHdl: sym���
*		FAddr: �����׵�ַ
*		pFName: ������
*		pDbgHdl: ����dbg���
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
int SymAddr2Func(SYM_HDL *pSymHdl, TADDR Addr
  , int (*pFunc)(SYM_HDL *pSymHdl, TADDR FAddr, const char *pFName, DBG_HDL *pDbgHdl, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	��ȡ����������Ϣ
*
* PARAMETERS
*	pSymHdl: sym���
*	pDbgHdl: ����dbg���
*	pFunc: �ص�����, ������˳��ص�
*		pParaName: ����������, ����ΪNULL
*		Idx: ��������, ��0��ʼ
*		Type: ������������
*		Val: ��������ֵ
*		pArg: ����
*	pRegAry/RegMask: �Ĵ�����/�Ĵ�����Чλͼ
*	pCallerRegAry/CParaMask/CParaMask2: ���ú���ʱ�Ĵ�����/������Чλͼ(ָ���Ƶ�)/������Чλͼ(ջ�Ƶ�), ΪNULL��ʹ��ABI�����Ƶ�
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
typedef enum _BASE_TYPE
{
	CS_NOVAL, /* ֵδ֪, ����Ϊ0!!! */
	CS_BOOL,
	CS_INT,
	CS_UINT,
	CS_I64,
	CS_FLOAT,
	CS_DOUBLE,
	CS_STRING, /* �����ַ��� */
	CS_ADDR,
} BASE_TYPE;
typedef union _BASE_VAL
{
	TUINT n;
	TINT i;
	long long j;
	float f;
	double d;
	const char *pStr;
	TADDR p;
} BASE_VAL;
int SymGetFuncPara(SYM_HDL *pSymHdl, DBG_HDL *pDbgHdl, int (*pFunc)(const char *pParaName, int Idx, BASE_TYPE Type, BASE_VAL Val, void *pArg)
  , const TSIZE *pRegAry, TSIZE RegMask, const TSIZE *pCallerRegAry, TSIZE CParaMask, TSIZE CParaMask2, void *pArg);

/*************************************************************************
* DESCRIPTION
*	����sym����ı�־
*
* PARAMETERS
*	pSymHdl: sym���
*	Flag: ��־
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SymSetFlag(SYM_HDL *pSymHdl, unsigned Flag);

/*************************************************************************
* DESCRIPTION
*	�������ű�
*
* PARAMETERS
*	pSymHdl: sym���
*	pFunc: �ص�����
*		pName: ������
*		Val: ֵ
*		Size: ��С
*		Info: ������Ϣ(st_info/st_other/st_shndx)
*		pArg: ����
*	pArg: ����
*	Flag: Ҫɨ��ķ��ű�
*		bit0: ��̬���ű�
*		bit1: plt���ű�
*
* RETURNS
*	0: �ɹ�
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymForEachSym(SYM_HDL *pSymHdl, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag);

/*************************************************************************
* DESCRIPTION
*	ǿ�ƴ���sym����
*
* PARAMETERS
*	pSymHdl: sym���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SymForceLoad(SYM_HDL *pSymHdl);

/*************************************************************************
* DESCRIPTION
*	��ԭ1��ջ֡
*
* PARAMETERS
*	pSymHdl: sym���
*	pRegAry/pRegMask: �Ĵ�����/�Ĵ�����Чλͼ(REG_SP/REG_PC������Ч!!!), �ᱻ����!!!
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymUnwind(SYM_HDL *pSymHdl, TSIZE *pRegAry, TSIZE *pRegMask);

/*************************************************************************
* DESCRIPTION
*	��ȡ�Ѽ����ļ���·��
*
* PARAMETERS
*	pSymHdl: sym���
*
* RETURNS
*	NULL: ʧ��
*	����: ·��
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *SymGetFilePath(SYM_HDL *pSymHdl);


/*************************************************************************
*============================= debug section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡdbg�������
*
* PARAMETERS
*	pDbgHdl: dbg���
*
* RETURNS
*	NULL: ʧ��/�޸�����
*	����: ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *DbgGetName(DBG_HDL *pDbgHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡdbg���ռ���ֽ�
*
* PARAMETERS
*	pDbgHdl: dbg���
*
* RETURNS
*	~(TSIZE)0: ʧ��/�޸�����
*	����: ռ���ֽ�
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DbgGetByteSz(DBG_HDL *pDbgHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡdbg���ƫ����(����ṹ���Աƫ����)
*
* PARAMETERS
*	pDbgHdl: dbg���
*
* RETURNS
*	~(TSIZE)0: ʧ��/�޸�����
*	����: ƫ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DbgGetOffset(DBG_HDL *pDbgHdl);

/*************************************************************************
* DESCRIPTION
*	������ȡdbg������Ӿ����ƫ����(����ṹ���Աƫ����)
*
* PARAMETERS
*	pDbgHdl: dbg���
*	pOffAry: ����ƫ��������, ����Ϊn, û�л�ȡ�����-1
*	pNames: ��Ա��������, ����Ϊn
*	n: ����
*
* RETURNS
*	0: �ɹ�
*	����: ��ȡʧ�ܸ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetChildOffs(DBG_HDL *pDbgHdl, int *pOffAry, const char *pNames[], size_t n);

/*************************************************************************
* DESCRIPTION
*	��ȡdbg�����Ӧ·�����к�
*
* PARAMETERS
*	pDbgHdl: dbg���
*	pLineNo: �����к�
*
* RETURNS
*	NULL: ʧ��
*	����: ·��������ʱ�����Free()�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DbgGetLineInf(DBG_HDL *pDbgHdl, TUINT *pLineNo);

/*************************************************************************
* DESCRIPTION
*	��ȡdbg�����type dbg���
*
* PARAMETERS
*	pDbgHdl: dbg���
*	ptDbgHdl: ����type dbg���
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��/�޸�dbg��Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetTDbgHdl(DBG_HDL *pDbgHdl, struct _DBG_HDL *ptDbgHdl);

/*************************************************************************
* DESCRIPTION
*	�������ƻ�ȡdbg�������dbg���
*
* PARAMETERS
*	pDbgHdl: dbg���
*	pcDbgHdl: ������dbg���
*	pName: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��/�޸�dbg��Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetCDbgHdl(DBG_HDL *pDbgHdl, struct _DBG_HDL *pcDbgHdl, const char *pName);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�������ƻ�ȡsym���, �ᴥ��sym����!!!
*
* PARAMETERS
*	pName: sym����
*
* RETURNS
*	NULL: û��
*	����: sym���
*
* GLOBAL AFFECTED
*
*************************************************************************/
SYM_HDL *GetSymHdlByName(const char *pName);

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡsym���, �ᴥ��sym����!!!
*
* PARAMETERS
*	Addr: ��ַ
*
* RETURNS
*	NULL: ��ַ�����κ�sym�����
*	����: sym���
*
* GLOBAL AFFECTED
*
*************************************************************************/
SYM_HDL *GetSymHdlByAddr(TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	����sym���, ���ᴥ��sym����!!!
*
* PARAMETERS
*	pFunc: �ص�����
*		pSymHdl: sym���
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachSymHdl(int (*pFunc)(SYM_HDL *pSymHdl, void *pArg), void *pArg);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��symfilesģ��
*	ʹ�÷���: PrepareSymFiles() -> ModuleSymInit() -> DestorySymFiles()
*
* PARAMETERS
*	CoreCnt: CPU����
*	CodeStart: kernel������ʼ��ַ, 0��ʾδ֪
*	fp: �쳣������
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PrepareSymFiles(TUINT CoreCnt, TADDR CodeStart, FILE *fp);

/*************************************************************************
* DESCRIPTION
*    ��ʼ��moduleģ��
*
* PARAMETERS
*    ModAddr: ģ��ṹ���ַ�������Ϊ0����LoadBias��pName����Ϊ��
*    LoadBias/Size: ����ƫ������ģ���С
*    pName: ģ������
*
* RETURNS
*    0: �ɹ�
*    1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ModuleSymInit(TADDR ModAddr, TADDR LoadBias, TSIZE Size, const char *pName);

/*************************************************************************
* DESCRIPTION
*	ע��symfilesģ��
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
void DestorySymFiles(void);

#endif
