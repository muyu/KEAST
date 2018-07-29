/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	ac.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/3/11
* Description: AC��ģʽ�ַ���ƥ��ģ��
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "os.h"
#include <ac.h>

typedef struct _TRIE_NODE
{
	unsigned Idx; /* �ýڵ��Ƿ�Ϊĳģʽ�����ս�� */
	struct _TRIE_NODE *pFail, *pNext;
	struct _TRIE_NODE *pChar[1]; /* ���������� */
} TRIE_NODE;

typedef struct _AC_TRIE
{
	unsigned char StartC, EndC; /* [StartC, EndC], �Զ������ַ����ķ�Χ */
	TRIE_NODE Root; /* ���������� */
} AC_TRIE;

/*************************************************************************
* DESCRIPTION
*	����pattern���ַ�����
*
* PARAMETERS
*	pStartC/pEndC: ������ʼ�ַ�, ��ֹ�ַ�
*	ppPattern: pattern�ַ�������,NULL��β
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static void CalcCharSetRange(unsigned char *pStartC, unsigned char *pEndC, const char * const *ppPattern)
{
	const char *p;
	unsigned char c;

	*pStartC = 0xFF;
	*pEndC   = 0;
	while ((p = *ppPattern++) != NULL) {
		while ((c = (unsigned)*p++) != '\0') {
			if (c < *pStartC)
				*pStartC = c;
			if (c > *pEndC)
				*pEndC = c;
		}
	}
}

/*************************************************************************
* DESCRIPTION
*	����pattern�ַ������������ұ�
*
* PARAMETERS
*	pRoot: ac root
*	StartC: start of char
*	NodeSize: node size
*	s: pattern�ַ���
*	Idx: string idx
*
* RETURNS
*	0: �ɹ�
*	1: ʧ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int AcTrieInsert(TRIE_NODE *pRoot, unsigned char StartC, unsigned NodeSize, const char *s, unsigned Idx)
{
	TRIE_NODE *pNew;
	unsigned char c;

	while ((c = (unsigned char)*s++) != '\0') {
		c -= StartC; /* ����idx */
		if (!pRoot->pChar[c]) {
			pNew = Calloc(NodeSize);
			if (!pNew)
				return 1;
			pRoot->pChar[c] = pNew;
		}
		pRoot = pRoot->pChar[c];
	}
	if (pRoot->Idx) /* ģʽ���������ظ��ս�� */
		return 1;
	pRoot->Idx = Idx;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	����AC�Զ���failָ��
*
* PARAMETERS
*	pRoot: ac root
*	RangeC: range of char set
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void AcTrieBuildFail(TRIE_NODE *pRoot, unsigned RangeC)
{
	TRIE_NODE *pFront, *pRear, *pCur, *pFail;
	unsigned i;
	
	for (pRear = pFront = pRoot; pFront; pFront = pFront->pNext) {
		for (i = 0; i < RangeC; i++) {
			pCur = pFront->pChar[i];
			if (pCur) {
				pRear->pNext = pCur;
				pRear = pCur; /* ����next���� */
				if (pFront == pRoot) { /* root������һ���ڵ��ʧ��ָ��ȫ��ָ��root */
					pCur->pFail = pRoot;
				} else {
					/*
						  ����ʧ��ָ��Ĺ��̸���������һ�仰��
							  1. ������ڵ��ϵ���ĸΪC�����������׵�ʧ��ָ���ߣ�ֱ���ߵ�һ���ڵ㣬���Ķ�����Ҳ����ĸΪC�Ľڵ㡣Ȼ��ѵ�ǰ�ڵ��ʧ��ָ��ָ���Ǹ���ĸҲΪC�Ķ��ӡ�
							  2. ���һֱ�ߵ���root��û�ҵ����ǾͰ�ʧ��ָ��ָ��root
					   */
					for (pFail = pFront->pFail; pFail; pFail = pFail->pFail) {
						if (pFail->pChar[i]) {
							pCur->pFail = pFail->pChar[i];
							goto END;
						}
					}
					pCur->pFail = pRoot; /* û���ҵ�����ô��ʧ��ָ��ָ��root */
END:;		   }
			}
		}
	}
}

/*************************************************************************
* DESCRIPTION
*	AC�Զ��������ַ���
*
* PARAMETERS
*	Hdl: ac handle
*	ppStr: input: �������ַ���, output: ���ҳɹ�����ַ���λ��
*
* RETURNS
*	0: ʧ��
*	����: string idx + 1
*
* LOCAL AFFECTED
*
*************************************************************************/
unsigned AcTrieSearch(AC_HDL *pHdl, const char **ppStr)
{
	TRIE_NODE *pRoot = &((AC_TRIE *)pHdl)->Root;
	unsigned char c, StartC = ((AC_TRIE *)pHdl)->StartC, EndC = ((AC_TRIE *)pHdl)->EndC;
	TRIE_NODE *p = pRoot;
	const char *pStr = *ppStr;

	while ((c = (unsigned char)*pStr++) != '\0') {
		if (c < StartC || c > EndC) {
			p = pRoot;
			continue;
		}
		c -= StartC; /* ����idx */
		while (!p->pChar[c] && p != pRoot)
			p = p->pFail;
		p = p->pChar[c];
		if (!p) { /* û���ҵ�ƥ��㣬���root����ʼƥ�� */
			p = pRoot;
		} else if (p->Idx) {
			*ppStr = pStr;
			return p->Idx;
		}
	}
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	����pattern�ַ�������ACģ��
*
* PARAMETERS
*	ppPattern: ��ƥ����ַ�������
*
* RETURNS
*	NULL: ʧ��
*	����: ae handle
*
* LOCAL AFFECTED
*
*************************************************************************/
AC_HDL *GetAcTrie(const char * const *ppPattern)
{
	AC_TRIE *pAcTrie;
	unsigned RangeC, NodeSize;
	int rt = 0, Cnt = 1;
	unsigned char StartC, EndC;

	CalcCharSetRange(&StartC, &EndC, ppPattern);
	RangeC = EndC + 1 - StartC;
	pAcTrie = Calloc(sizeof(AC_TRIE) + (RangeC - 1) * sizeof(TRIE_NODE *));
	if (pAcTrie) {
		pAcTrie->StartC = StartC;
		pAcTrie->EndC   = EndC;
		NodeSize = sizeof(TRIE_NODE) + (RangeC - 1) * sizeof(TRIE_NODE *);
		while (*ppPattern && !rt) {
			rt = AcTrieInsert(&pAcTrie->Root, StartC, NodeSize, *ppPattern++, Cnt++);
		}
		AcTrieBuildFail(&pAcTrie->Root, RangeC);
		if (rt) {
			PutAcTrie((AC_HDL *)pAcTrie);
			pAcTrie = NULL;
		}
	}
	return (AC_HDL *)pAcTrie;
}

/*************************************************************************
* DESCRIPTION
*	�ͷ�acģ��
*
* PARAMETERS
*	Hdl: ac handle
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
void PutAcTrie(AC_HDL *pHdl)
{
	TRIE_NODE *pNode = ((AC_TRIE *)pHdl)->Root.pNext;
	TRIE_NODE *p;
	
	while ((p = pNode) != NULL) {
		pNode = pNode->pNext;
		Free(p);
	}
	Free((AC_TRIE *)pHdl);
}
