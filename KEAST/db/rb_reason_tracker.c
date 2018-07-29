/*********************************************************************************
* Copyright(C),2016-2017,MediaTek Inc.
* FileName:	rb_reason_tracker.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2016/7/1
* Description: ������Ϣģ�����
**********************************************************************************/
#include <stdio.h>
#include "string_res.h"
#include <module.h>
#include "db.h"
#include "rb_reason.h"
#include "rb_reason_tracker.h"


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʾdram�쳣��Ϣ
*
* PARAMETERS
*	pGlbRbInf: ȫ�ֵ������Ϣ
*	fp: ����������
*
* RETURNS
*	0: û��dram����
*	1: ����dram����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowDramInf(GLB_RBINF *pGlbRbInf, FILE *fp)
{
	if (pGlbRbInf->DRamFlag&DRAMC_GATING) {
		LOGC(fp, "DRAM gating");
		goto _ERR;
	}
	if (pGlbRbInf->DRamFlag&DRAMC_TA2) {
		LOGC(fp, "DRAM test agent");
		goto _ERR;
	}
	if (pGlbRbInf->DRamFlag&DRAMC_SELFTEST) {
		LOGC(fp, "DRAM self test");
		goto _ERR;
	}
	if (pGlbRbInf->DRamFlag&DRAMC_INIT) {
		LOGC(fp, "DRAM init");
		goto _ERR;
	}
	return 0;
_ERR:
	LOG(fp, RS_DRAMC);
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	��ʾsystracker��Ϣ
*
* PARAMETERS
*	pGlbRbInf: ȫ�ֵ������Ϣ
*	fp: ����������
*
* RETURNS
*	0: û��bus����
*	1: ����bus����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowSystrackerInf(GLB_RBINF *pGlbRbInf, FILE *fp)
{
	int NoSysTW = 1, NoSysTR = 1;
	size_t i;

	/* ���write */
	for (i = ARY_CNT(pGlbRbInf->SysTWAddr); i-- > 0; ) {
		if (!pGlbRbInf->SysTWAddr[i])
			continue;
		if (NoSysTW) {
			NoSysTW = 0;
			LOG(fp, RS_SYSTRACKER_W);
		}
		LOGC(fp, " 0x%08X", pGlbRbInf->SysTWAddr[i]);
	}
	if (!NoSysTW)
		LOG(fp, RS_SYST_TIMEOUT);
	/* ���read */
	for (i = ARY_CNT(pGlbRbInf->SysTRAddr); i-- > 0; ) {
		if (!pGlbRbInf->SysTRAddr[i])
			continue;
		if (NoSysTR) {
			NoSysTR = 0;
			LOG(fp, RS_SYSTRACKER_R);
		}
		LOGC(fp, " 0x%08X", pGlbRbInf->SysTRAddr[i]);
	}
	if (!NoSysTR)
		LOG(fp, RS_SYST_TIMEOUT);
	/* ��ʾ */
	if (NoSysTW && NoSysTR)
		return 0;
	LOGC(fp, "\n");
#ifdef INTERNAL_BUILD
	LOG(fp, RS_REFERENCE);
	LOGC(fp, " http://wiki.mediatek.inc/pages/viewpage.action?pageId=93753425\n");
#endif
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	��ʾperi sys��Ϣ
*
* PARAMETERS
*	pGlbRbInf: ȫ�ֵ������Ϣ
*	fp: ����������
*
* RETURNS
*	0: û��bus����
*	1: ����bus����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowPeriSysInf(GLB_RBINF *pGlbRbInf, FILE *fp)
{
	if (!pGlbRbInf->PeriMon[2])
		return 0;
	LOG(fp, RS_LAST_BUS, (pGlbRbInf->PeriMon[2]&0xFFFF0000)|(pGlbRbInf->PeriMon[3]&0xFFFF));
#ifdef INTERNAL_BUILD
{
	static const char *pMaster1[] = {"AUDIO", "MSDC3", "USB1"};
	static const char *pMaster2[] = {"PWM", "MSDC1", "MSDC2", "SPI0"};
	static const char *pMaster3[] = {"SPM", "MD32", "Thermal"};
	static const char *pSlave[] = {"USB1P", "USB1PSIF", "AUDIO", "MSDC0", "MSDC1", "MSDC2", "MSDC3", "USB3.0", "Conn_Peri"
	  , "MD_Peri", "C2K_Peri", "USB3.0_SIF", "USB3.0_SIF2"};
	size_t i;

	LOG(fp, RS_LAST_BUS2);
	if (pGlbRbInf->PeriMon[1]&(1 << 15)) {
		if (pGlbRbInf->PeriMon[0]&(1 << 20)) {
			LOGC(fp, " SPI1");
			for (i = 0; i < ARY_CNT(pMaster1); i++) {
				if (pGlbRbInf->PeriMon[0]&(1 << (i + 13)))
					LOGC(fp, " %s", pMaster1[i]);
			}
		}
		if (pGlbRbInf->PeriMon[4]&((1 << 13)|(1 << 14))) {
			if (pGlbRbInf->PeriMon[0]&(1 << 19)) {
				for (i = 0; i < ARY_CNT(pMaster2); i++) {
					if (pGlbRbInf->PeriMon[0]&(1 << (i + 7)))
						LOGC(fp, " %s", pMaster2[i]);
				}
			}
			if (!(pGlbRbInf->PeriMon[2]&(1 << 3)))
				LOGC(fp, " MSDC0");
		}
		LOGC(fp, " USB0");
	}
	if (pGlbRbInf->PeriMon[0]&(1 << 18)) {
		LOGC(fp, " SPM2");
		for (i = 0; i < ARY_CNT(pMaster3); i++) {
			if (!(pGlbRbInf->PeriMon[0]&(1 << (i + 24))))
				LOGC(fp, " %s", pMaster3[i]);
		}
	}
	if (pGlbRbInf->PeriMon[0]&(1 << 17))
		LOGC(fp, " infra_md");
	if ((pGlbRbInf->PeriMon[1]&((1 << 13)|(1 << 14))) && (pGlbRbInf->PeriMon[1]&((1 << 17)|(1 << 18))) == ((1 << 17)|(1 << 18)))
		LOGC(fp, " DMA Top_Fabric");
	if (pGlbRbInf->PeriMon[1]&((1 << 9)|(1 << 10))) {
		LOGC(fp, "|");
		for (i = 0; i < ARY_CNT(pSlave); i++) {
			if (!(pGlbRbInf->PeriMon[4]&(1 << i)))
				LOGC(fp, " %s", pSlave[i]);
		}
	}
}
#endif
	LOGC(fp, "\n");
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	��ʾinfra sys��Ϣ
*
* PARAMETERS
*	pGlbRbInf: ȫ�ֵ������Ϣ
*	fp: ����������
*
* RETURNS
*	0: û��bus����
*	1: ����bus����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowInfraSysInf(GLB_RBINF *pGlbRbInf, FILE *fp)
{
#if 0 /* û��̫������ */
	if (!pGlbRbInf->InfraInf)
		return 0;
	LOG(fp, RS_LAST_BUS3);
	return 1;
#else
	(void)pGlbRbInf, (void)fp;
	return 0;
#endif
}

/*************************************************************************
* DESCRIPTION
*	��ʾlast bus��Ϣ
*
* PARAMETERS
*	pGlbRbInf: ȫ�ֵ������Ϣ
*	fp: ����������
*
* RETURNS
*	0: û��bus����
*	1: ����bus����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int ShowLastBusInf(GLB_RBINF *pGlbRbInf, FILE *fp)
{
	switch (GetPlatform()) {
	case 6750: case 6755: case 6757: return ShowPeriSysInf(pGlbRbInf, fp);
	case 6799: return ShowInfraSysInf(pGlbRbInf, fp);
	default: return 0;
	}
}


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	����last dbg��Ϣ
*
* PARAMETERS
*	fp: �ļ����
*
* RETURNS
*	0: ����
*	1: �����쳣
*
* GLOBAL AFFECTED
*
*************************************************************************/
int LastDbgInfCheck(FILE *fp)
{
	GLB_RBINF * const pGlbRbInf = GlobalRbInf();

	return ShowDramInf(pGlbRbInf, fp) || ShowSystrackerInf(pGlbRbInf, fp) || ShowLastBusInf(pGlbRbInf, fp);
}
