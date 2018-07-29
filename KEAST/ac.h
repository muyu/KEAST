/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	ac.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/3/11
* Description: AC多模式字符串匹配模块
**********************************************************************************/
#ifndef _AC_H
#define _AC_H

typedef void * AC_HDL;

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
unsigned AcTrieSearch(AC_HDL *pHdl, const char **ppStr);

/*************************************************************************
* DESCRIPTION
*	根据pattern字符串建立AC模块
*
* PARAMETERS
*	ppPattern: 待匹配的字符串数组,NULL结尾
*
* RETURNS
*	NULL: 失败
*	其他: ae handle
*
* LOCAL AFFECTED
*
*************************************************************************/
AC_HDL *GetAcTrie(const char * const *ppPattern);

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
void PutAcTrie(AC_HDL *pHdl);

#endif