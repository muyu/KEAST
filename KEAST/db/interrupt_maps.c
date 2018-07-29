/*********************************************************************************
* Copyright(C),2018-2018,MediaTek Inc.
* FileName:	interrupt_maps.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2018/4/25
* Description: INTERRUPT_NUM_NAME_MAPS信息提取
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include <string_res.h>
#include "os.h"
#include "interrupt_maps.h"


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据irq index获取irq的名称
*
* PARAMETERS
*	Irq: irq index
*	pStr: 返回irq名称
*	Sz: pStr指向内存的大小
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int IrqIdx2Name(TUINT Irq, char *pStr, size_t Sz)
{
	char data[1024], *p;
	unsigned IrqIdx;
	int rt = 1;
	size_t i;
	FILE *fp;

	fp = fopen("INTERRUPT_NUM_NAME_MAPS", "r");
	if (!fp)
		return rt;
	while (fgets(data, sizeof(data), fp)) {
		if (sscanf(data, "%d:", &IrqIdx) != 1 || IrqIdx != Irq)
			continue;
		p = strstr(data, " Level ");
		if (p) {
			p += sizeof(" Level ");
		} else {
			p = strstr(data, " Edge ");
			if (!p)
				break;
			p += sizeof(" Edge ");
		}
		while (*p == ' ') {
			p++;
		}
		for (i = 0; i < Sz - 1 && p[i] && p[i] != '\n'; i++) {
			pStr[i] = p[i];
		}
		pStr[i] = '\0';
		rt = 0;
		break;
	}
	fclose(fp);
	return rt;
}
