/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	memspace.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/9/17
* Description: �ڴ������
**********************************************************************************/
#ifndef _MEMSPACE_H
#define _MEMSPACE_H

#include <ttypes.h>

#define VMA_SIZE(Hdl)	   ((Hdl)->EAddr - (Hdl)->SAddr)

#if defined(__MINGW32__)
#include <stdint.h>   /*For define uintptr_t*/
#endif



typedef const struct _VMA_HDL
{
	TADDR SAddr, EAddr, LinkAddr; /* [��ʼ��ַ, ������ַ), ���ӵ�ַ */
	const char *pName; /* ����ΪNULL!!! */
	const void *pHdl; /* ΪVMA_ELFʱ��SYM_HDL, ΪVMA_DEXʱ��DEX_HDL, ����ΪNULL!! */

#define VMA_NAME	(1 << 3) /* ���ֲ��ǳ����ַ��� */
#define VMA_RBITS	4 /* Ԥ��bitλ��(�ڲ�ʹ��) */

#define VMA_MSHIFT	VMA_RBITS
#define VMA_X		(1 << (VMA_MSHIFT + 0)) /* ��ִ�� */
#define VMA_W		(1 << (VMA_MSHIFT + 1)) /* ��д */
#define VMA_R		(1 << (VMA_MSHIFT + 2)) /* �ɶ� */
#define VMA_RW		(VMA_R|VMA_W)
#define VMA_MBITS	3
#define VMA_MMASK	(((1 << VMA_MBITS) - 1) << VMA_MSHIFT)
#define VMA_ATTR(pVmaHdl) ((pVmaHdl)->Flag&VMA_MMASK)

#define VMA_TSHIFT	(VMA_MSHIFT + VMA_MBITS)
#define VMA_NONE	(0 << VMA_TSHIFT)
#define VMA_ELF		(1 << VMA_TSHIFT)
#define VMA_FILE	(2 << VMA_TSHIFT) /* ��ͨ�ļ� */
#define VMA_HEAP	(3 << VMA_TSHIFT)
#define VMA_STACK	(4 << VMA_TSHIFT)
#define VMA_IGNORE	(5 << VMA_TSHIFT)
#define VMA_TBITS	4
#define VMA_TMASK	(((1 << VMA_TBITS) - 1) << VMA_TSHIFT)
#define VMA_TYPE(pVmaHdl)	((pVmaHdl)->Flag&VMA_TMASK)
	unsigned Flag;
} VMA_HDL;


/*************************************************************************
*============================ Memory section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡbuffer
*	ע��: pVmaHdl���ͷź�buffer��������!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	Addr: ��ַ
*	Size: ��С(�ֽ�)
*
* RETURNS
*	NULL: ʧ��
*	����: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *Vma2Mem(VMA_HDL *pVmaHdl, TADDR Addr, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡ���ݲ������buffer
*
* PARAMETERS
*	Addr: ��ַ
*	pBuf/Size: ������buffer/��С(�ֽ�)
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DumpMem(TADDR Addr, void *pBuf, TSIZE Size);

/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡ�ַ���
*
* PARAMETERS
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: string, ����ʱ��Ҫ�ͷ�!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *GetString(TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	����vma�������, ���Ǿ�����!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	pName: ����
*	Flag: ֻ����VMA_NAME/VMA_MMASK/VMA_TMASK!!!
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void VmaSetAttr(VMA_HDL *pVmaHdl, const char *pName, unsigned Flag);

/*************************************************************************
* DESCRIPTION
*	��vma�������1������ͻ�ȡbuffer����, ���Ǿɾ���ͻ�ȡbuffer����!!!
*	���غ������ܵ���TryMergeVma()!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	pHdl: ���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void VmaRegHdl(VMA_HDL *pVmaHdl, const void *pHdl);

/*************************************************************************
* DESCRIPTION
*	��vma�������1����ַ, ���Ǿɵ�ַ!!!
*
* PARAMETERS
*	pVmaHdl: vma���
*	Addr: �ϸ�vma��ʼ��ַ
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void VmaLink(VMA_HDL *pVmaHdl, TADDR Addr);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���ݵ�ַ��ȡvma���
*
* PARAMETERS
*	Addr: ��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: vma���
*
* GLOBAL AFFECTED
*
*************************************************************************/
VMA_HDL *GetVmaHdl(TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	�ͷ�vma���
*
* PARAMETERS
*	pVmaHdl: vma���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutVmaHdl(VMA_HDL *pVmaHdl);

/*************************************************************************
* DESCRIPTION
*	���Ժϲ�ָ����Χ�ڵ�vma(���ͱ���ΪVMA_NONE)�����ûص�����
*
* PARAMETERS
*	SAddr/EAddr: [��ʼ��ַ, ������ַ)
*	pFunc: �ص�����
*		pVmaHdl: vma���, �뿪�ص����������ͷ�!!!
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
int TryMergeVma(TADDR SAddr, TADDR EAddr, int (*pFunc)(VMA_HDL *pVmaHdl, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	ָ����Χ�ڵ�ַ��С�������vma
*
* PARAMETERS
*	SAddr/EAddr: ��Χ: [��ʼ��ַ, ������ַ)
*	pFunc: �ص�����
*		SAddr/EAddr: [��ʼ��ַ, ������ַ)
*		Flag: ��־, VMA_*
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
#define ForEachVmaAll(pFunc, pArg)  ForEachVma(0, ~(TADDR)0, pFunc, pArg)
int ForEachVma(TADDR SAddr, TADDR EAddr, int (*pFunc)(TADDR SAddr, TADDR EAddr, unsigned Flag, void *pArg), void *pArg);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�����ڴ��׼��
*	ʹ�÷���: PrepareMemSpace() -> CreateMemRegion() -> DestoryMemSpace()
*
* PARAMETERS
*	pFpHdl: fp���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PrepareMemSpace(const void *pFpHdl);

/*************************************************************************
* DESCRIPTION
*	����1���ڴ�����
*
* PARAMETERS
*	Addr/Size: ��ʼ��ַ�ʹ�С(�ֽ�), ����PAGE����!!!
*	Attr: ��д����(VMA_R/VMA_W/VMA_X)
*	FOff: �ļ�ƫ����
*	pBuf: buffer, ��ΪNULLʱ, FOffΪ0!!!
*	pCmpFunc: �ص��ԱȺ���
*	pArg: ����
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*	2: �ص�
*
* GLOBAL AFFECTED
*
*************************************************************************/
int CreateMemRegion(TADDR Addr, TSIZE Size, unsigned Attr, TSIZE FOff, const void *pBuf
	, int (*pCmpFunc)(TADDR Addr, TSIZE Size, const void *pBuf1, const void *pBuf2, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	ע�������ڴ��
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
void DestoryMemSpace(void);

#endif
