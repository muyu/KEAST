/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	module.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/3/11
* Description: ģ��ͨ�ú�������
**********************************************************************************/
#ifndef _MODULE_H
#define _MODULE_H

#include <stdio.h>
#include <ttypes.h>

/* ת��: ģ���ⲿ�ṹ <=> ģ���ڲ��ṹ */
#define OFFSET_OF(TYPE, MEMBER)	((size_t)&((TYPE *)0)->MEMBER)
#define MOD_INT(Type, Hdl)		((Type##_INT *)((char *)(Hdl) - OFFSET_OF(Type##_INT, Ex)))
#define MOD_HDL(Hdl)			(&(Hdl)->Ex)

#define ARY_CNT(a)				(sizeof(a) / sizeof((a)[0]))
#define ARY_END(a)				a + ARY_CNT(a) - 1

/* ��Ч��:(x&bit1) ? bit2 : 0, ע��: bit1/bit2������2��ָ��!!! */
#define FLAG_TRANS(x, bit1, bit2) ((bit1) <= (bit2) ? ((x)&(bit1)) * ((bit2) / (bit1)) : ((x)&(bit1)) / ((bit1) / (bit2)))

#define OFFVAL_U1(Ptr, Off)		(*((unsigned char *)(Ptr) + (Off)))
#define OFFVAL_S1(Ptr, Off)		(*((char *)(Ptr) + (Off)))
#define OFFVAL_U2(Ptr, Off)		(*(unsigned short *)((char *)(Ptr) + (Off)))
#define OFFVAL_S2(Ptr, Off)		(*(short	 *)((char *)(Ptr) + (Off)))
#define OFFVAL_U4(Ptr, Off)		(*(unsigned  *)((char *)(Ptr) + (Off)))
#define OFFVAL_S4(Ptr, Off)		(*(int	   *)((char *)(Ptr) + (Off)))
#define OFFVAL_U8(Ptr, Off)		(*(unsigned long long *)((char *)(Ptr) + (Off)))
#define OFFVAL_S8(Ptr, Off)		(*(long long *)((char *)(Ptr) + (Off)))
#define OFFVAL_F(Ptr, Off)		(*(float	 *)((char *)(Ptr) + (Off)))
#define OFFVAL_D(Ptr, Off)		(*(double	*)((char *)(Ptr) + (Off)))
#define OFFVAL_P(Ptr, Off)		(*(void	 **)((char *)(Ptr) + (Off)))
#define OFFVAL(Type, Ptr, Off)	(*(Type	  *)((char *)(Ptr) + (Off)))
#define OFFVAL_ADDR(Ptr, Off)	(*(TADDR	 *)((char *)(Ptr) + (Off)))
#define OFFVAL_SIZE(Ptr, Off)	(*(TSIZE	 *)((char *)(Ptr) + (Off)))


#define MOD_NOT_SUPPORT		0 /* ��֧�ֻ�δ��ʼ��, ����Ϊ0!!! */
#define MOD_OOM				1 /* �ڴ治��, ������MOD_NOT_SUPPORT֮��!!! */
#define MOD_ADDR			2 /* ��ȡ��ַʧ�� */
#define MOD_CORRUPTED		3 /* ������Ȼ� */
#define MOD_OK				4 /* ���� */

#define MOD_USR_BIT			3 /* ��ģ����Զ�����ʼbitλ */
#define MOD_SYS_MAX			(1 << MOD_USR_BIT)
#define MOD_STATUS(ModErr)	((ModErr)&(MOD_SYS_MAX - 1))


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	����ģ���쳣��Ϣ���ļ�
*
* PARAMETERS
*	fp: �ļ����
*	ModErr: ģ���쳣��Ϣ
*	pModName: ģ������
*
* RETURNS
*	0: ����
*	1: �쳣
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DispModErr(FILE *fp, unsigned ModErr, const char *pModName);


typedef const struct _LIST_HEAD
{
	TADDR NextAddr;
	TADDR PrevAddr;
} LIST_HEAD;

#define LIST_ENTRY_ADDR(ListAddr, Type, Member) ((TADDR)(ListAddr) - OFFSET_OF(Type, Member))

/*************************************************************************
* DESCRIPTION
*	���һ�����ϣ��
*
* PARAMETERS
*	pHtblHdl: ��ϣ����
*	HashVal: ��ϣֵ
*	pData: ���һ���������, ����ΪNULL!!!
*	pCmpFunc: �ȽϺ���, ����0��ʾһ��, ����һ��
*		pTblData: ��ϣ������
*		pData: �Ա�����
*	DoAdd: �Ƿ����
*
* RETURNS
*	NULL: ʧ��
*	����: ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef void * HTBL_HDL;
void *HtblLookup(HTBL_HDL *pHtblHdl, unsigned HashVal, void *pData, int (*pCmpFunc)(void *pTblData, void *pData), int DoAdd);

/*************************************************************************
* DESCRIPTION
*	�ûص���������ɨ��һ��hash��
*
* PARAMETERS
*	pHtblHdl: hash table handle
*
* RETURNS
*	hash���¼�ĸ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
unsigned HtblGetUsedCnt(HTBL_HDL *pHtblHdl);

/*************************************************************************
* DESCRIPTION
*	����hash��
*
* PARAMETERS
*	pHtblHdl: ��ϣ����
*	pFunc: �ص�����
*		pData: ����
*		pArg: arg
*	pArg: ����Ĳ���
*
* RETURNS
*	0: �ɹ�
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef int (*HTBL_CB)(void *pData, void *pArg);
int HtblForEach(HTBL_HDL *pHtblHdl, HTBL_CB pFunc, void *pArg);

/*************************************************************************
* DESCRIPTION
*	������ϣ��
*
* PARAMETERS
*	Size: ��ʼ��С
*	pFreeFunc: �ͷź���(�ͷŹ�ϣ��ʱ����), ����ΪNULL
*
* RETURNS
*	NULL: ʧ��
*	����: ��ϣ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
HTBL_HDL *GetHtblHdl(unsigned Size, void (*pFreeFunc)(void *pData));

/*************************************************************************
* DESCRIPTION
*	�ͷŹ�ϣ��, ��ÿ�����ݵ����ͷź���(�����ע��Ļ�)
*
* PARAMETERS
*	HtblHdl: ��ϣ����
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutHtblHdl(HTBL_HDL *pHtblHdl);

/*************************************************************************
* DESCRIPTION
*	��ȡָ��cpu��per cpu������ַ
*
* PARAMETERS
*	PerCpuBase: per cpu������ַ
*	CpuId: CPU id
*
* RETURNS
*	0: ʧ��
*	����: ָ��cpu��per cpu������ַ
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR PerCpuAddr(TADDR PerCpuBase, TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	��������
*
* PARAMETERS
*	ListAddr: list_head��ַ
*	ListOff: �ṹ�������list_head��ƫ����
*	pFunc: �ص�����
*		HdlAddr: �����ַ
*		pArg: ����
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: �����쳣
*	����: �ص���������ֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachList(TADDR ListAddr, TUINT ListOff, int (*pFunc)(TADDR HdlAddr, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	���λΪ1��bitλ��(0~32)
*	0����Ϊ0, 1����Ϊ1, 3����Ϊ2
*
* PARAMETERS
*	Mask: ֵ
*
* RETURNS
*	���λΪ1��bitλ��(0~32)
*
* GLOBAL AFFECTED
*
*************************************************************************/
TUINT Fhs(TUINT Mask);

/*************************************************************************
* DESCRIPTION
*	��ʾ����(�����ǵ�ַ)��Ӧ�ķ���
*
* PARAMETERS
*	pDispFunc: ��ʾ�ص�����
*	fp: �ļ����
*	Val: ����
*
* RETURNS
*	����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ShowSymVal(int (*pDispFunc)(FILE *fp, const char *pFmt, ...), FILE *fp, TADDR Val);

/*************************************************************************
* DESCRIPTION
*	�ӿո�������ַ�������ȡ���ַ���
*
* PARAMETERS
*	ppStr: �ַ���ָ���ַ���ᱻ����
*	cc: �����ַ�
*
* RETURNS
*	���ַ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *GetSubStr(char **ppStr, char cc);

/*************************************************************************
* DESCRIPTION
*	�����ļ�
*
* PARAMETERS
*	pDstFile: Ŀ���ļ�
*	pSrcFile: Դ�ļ�
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int CCpyFile(const char *pDstFile, const char *pSrcFile);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��ģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: ģ��ر�
*	1: ģ�����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ModuleInit(void);

/*************************************************************************
* DESCRIPTION
*	ע��ģ��
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
void ModuleDeInit(void);

#endif
