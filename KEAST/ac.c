/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	ac.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/3/11
* Description: AC多模式字符串匹配模块
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "os.h"
#include <ac.h>

typedef struct _TRIE_NODE
{
	unsigned Idx; /* 该节点是否为某模式串的终结点 */
	struct _TRIE_NODE *pFail, *pNext;
	struct _TRIE_NODE *pChar[1]; /* 必须放在最后 */
} TRIE_NODE;

typedef struct _AC_TRIE
{
	unsigned char StartC, EndC; /* [StartC, EndC], 自动机里字符集的范围 */
	TRIE_NODE Root; /* 必须放在最后 */
} AC_TRIE;

/*************************************************************************
* DESCRIPTION
*	设置pattern的字符集合
*
* PARAMETERS
*	pStartC/pEndC: 返回起始字符, 终止字符
*	ppPattern: pattern字符串数组,NULL结尾
*
* RETURNS
*	无
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
*	插入pattern字符串，建立查找表
*
* PARAMETERS
*	pRoot: ac root
*	StartC: start of char
*	NodeSize: node size
*	s: pattern字符串
*	Idx: string idx
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int AcTrieInsert(TRIE_NODE *pRoot, unsigned char StartC, unsigned NodeSize, const char *s, unsigned Idx)
{
	TRIE_NODE *pNew;
	unsigned char c;

	while ((c = (unsigned char)*s++) != '\0') {
		c -= StartC; /* 计算idx */
		if (!pRoot->pChar[c]) {
			pNew = Calloc(NodeSize);
			if (!pNew)
				return 1;
			pRoot->pChar[c] = pNew;
		}
		pRoot = pRoot->pChar[c];
	}
	if (pRoot->Idx) /* 模式串不能有重复终结点 */
		return 1;
	pRoot->Idx = Idx;
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	建立AC自动机fail指针
*
* PARAMETERS
*	pRoot: ac root
*	RangeC: range of char set
*
* RETURNS
*	无
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
				pRear = pCur; /* 加入next链表 */
				if (pFront == pRoot) { /* root所有下一级节点的失败指针全部指向root */
					pCur->pFail = pRoot;
				} else {
					/*
						  构造失败指针的过程概括起来就一句话：
							  1. 设这个节点上的字母为C，沿着他父亲的失败指针走，直到走到一个节点，他的儿子中也有字母为C的节点。然后把当前节点的失败指针指向那个字母也为C的儿子。
							  2. 如果一直走到了root都没找到，那就把失败指针指向root
					   */
					for (pFail = pFront->pFail; pFail; pFail = pFail->pFail) {
						if (pFail->pChar[i]) {
							pCur->pFail = pFail->pChar[i];
							goto END;
						}
					}
					pCur->pFail = pRoot; /* 没有找到，那么将失败指针指向root */
END:;		   }
			}
		}
	}
}

/*************************************************************************
* DESCRIPTION
*	AC自动机查找字符串
*
* PARAMETERS
*	Hdl: ac handle
*	ppStr: input: 待搜索字符串, output: 查找成功后的字符串位置
*
* RETURNS
*	0: 失败
*	其他: string idx + 1
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
		c -= StartC; /* 计算idx */
		while (!p->pChar[c] && p != pRoot)
			p = p->pFail;
		p = p->pChar[c];
		if (!p) { /* 没有找到匹配点，则从root出开始匹配 */
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
*	根据pattern字符串建立AC模块
*
* PARAMETERS
*	ppPattern: 待匹配的字符串数组
*
* RETURNS
*	NULL: 失败
*	其他: ae handle
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
*	释放ac模块
*
* PARAMETERS
*	Hdl: ac handle
*
* RETURNS
*	无
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
