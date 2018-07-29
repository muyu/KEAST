/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	exception.c
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: �쳣����
**********************************************************************************/
#include <stdio.h>
#include <string.h>
#include "os.h"
#include "string_res.h"
#include <module.h>
#include <mrdump.h>
#include <callstack.h>
#include <armdis.h>
#include <exception.h>
#include <interact/feedback.h>
#include <interact/reference.h>
#include <interact/command.h>
#include <interact/logeigenclient.h>
#include <db/rb_reason.h>
#include <db/rb_reason_tracker.h>
#include <db/interrupt_maps.h>
/* ����������ļ��м����ͷ�ļ� */
#include <db/db.h>
#include <mm/mm.h>
#include <kernel/kernel.h>
#include <drivers/drivers.h>
#include <script/script_gen.h>


#define ONLINE_QS   " MediaTek On-Line> Quick Start> "
#define BUG_REF_FMT "�������Linux kernel exception���> ʵ��ƪ: ��������> BUG at %s\n"

typedef const struct _BUG_DISPATCH
{
	const char *pFName;
	unsigned Len, RsId;
	void (*BugProcess)(FILE *fp, CORE_CNTX *pExcpCntx, const struct _BUG_DISPATCH *pBugDispath);
} BUG_DISPATCH;


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	���thermal�����Ϣ
*
* PARAMETERS
*	fp: ����ļ����
*	pExcpCntx: �쳣cpu������
*	pBugDispath: bug��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void BugThermalReboot(FILE *fp, CORE_CNTX *pExcpCntx, const struct _BUG_DISPATCH *pBugDispath)
{
	char Module[5];

	(void)pExcpCntx;
	if (sscanf(pBugDispath->pFName, "ts%4[^_]", Module) == 1)
		LOG(fp, RS_THERMAL_1, Module);
	else
		LOG(fp, RS_THERMAL_0);
}

/*************************************************************************
* DESCRIPTION
*	���ͨ�õ�bug�����Ϣ
*
* PARAMETERS
*	fp: ����ļ����
*	pExcpCntx: �쳣cpu������
*	pBugDispath: bug��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void BugCommonHandler(FILE *fp, CORE_CNTX *pExcpCntx, const struct _BUG_DISPATCH *pBugDispath)
{
	switch (pBugDispath->RsId) {
	case RS_SCHED2BUG_1: ShowPreemptInf(fp, pExcpCntx->TskAddr, pExcpCntx->ProcName); break;
	case RS_IRQ_CALL2BUG: LOG(fp, RS_IRQ_CALL2BUG, pBugDispath->pFName); break;
	default: LOG(fp, pBugDispath->RsId);
	}
	LOG(fp, RS_REFERENCE);
	LOGC(fp, ONLINE_QS);
	LOGC(fp, BUG_REF_FMT, pBugDispath->pFName);
}

/*************************************************************************
* DESCRIPTION
*	���sspm ipi timeout�����Ϣ
*
* PARAMETERS
*	fp: ����ļ����
*	pExcpCntx: �쳣cpu������
*	pBugDispath: bug��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void BugSspmIpiHandler(FILE *fp, CORE_CNTX *pExcpCntx, const struct _BUG_DISPATCH *pBugDispath)
{
	(void)pExcpCntx, (void)pBugDispath;
	LOG(fp, RS_SSPM_IPI2BUG);
#ifdef INTERNAL_BUILD
	LOG(fp, RS_REFERENCE);
	LOGC(fp, " http://mtkteams.mediatek.inc/sites/WSD/Project/TinySys/Shared%%20Documents/0_General/SSPM_IPI_Pins.pptx\n");
#endif
}

/*************************************************************************
* DESCRIPTION
*	���swt trigger hwt��Ϣ
*
* PARAMETERS
*	fp: ����ļ����
*	pExcpCntx: �쳣cpu������
*	pBugDispath: bug��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void Swt2HwtHandler(FILE *fp, CORE_CNTX *pExcpCntx, const struct _BUG_DISPATCH *pBugDispath)
{
	GLB_RBINF * const pGlbRbInf = GlobalRbInf();

	(void)pExcpCntx, (void)pBugDispath;
	switch (pGlbRbInf->HdState) {
	case HANGD_NORMAL:	LOG(fp, RS_HDT2HWT1, "Hang detect", "SYS_HANG_DETECT_RAW"); break;
	case HANGD_BT:		LOG(fp, RS_HDT2HWT2, "Hang detect", "SYS_HANG_DETECT_RAW"); break;
	case HANGD_SWT:		LOG(fp, RS_HDT2HWT3, "Hang detect", "SYS_HANG_DETECT_RAW"); break;
	case HANGD_NE:		LOG(fp, RS_HDT2HWT4, "Hang detect", "SYS_HANG_DETECT_RAW"); break;
	case HANGD_REBOOT:	LOG(fp, RS_HDT2HWT5, "Hang detect", "SYS_HANG_DETECT_RAW"); break;
	default: LOG(fp, RS_SWT2HWT); break;
	}
	LOG(fp, RS_REFERENCE);
	LOGC(fp, ONLINE_QS);
	LOGC(fp, "Hang Detect ������ٷ���\n");
}

/*************************************************************************
* DESCRIPTION
*	���ջ֡Խ����Ϣ
*
* PARAMETERS
*	fp: ����ļ����
*	pExcpCntx: �쳣cpu������
*	pBugDispath: bug��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void StkFrameOfHandler(FILE *fp, CORE_CNTX *pExcpCntx, const struct _BUG_DISPATCH *pBugDispath)
{
	const char *pFName;
	SYM_HDL *pSymHdl;
	char FunName[20];

	(void)pBugDispath;
	pSymHdl = GetSymHdlByAddr(pExcpCntx->RegAry[REG_LR]);
	if (!pSymHdl || (pFName = SymAddr2Name(pSymHdl, NULL, pExcpCntx->RegAry[REG_LR], TGLBFUN)) == NULL) {
		FunName[sizeof(FunName) - 1] = '\0';
		snprintf(FunName, sizeof(FunName) - 1, TADDRFMT, pExcpCntx->RegAry[REG_LR]);
		pFName = FunName;
	}
	LOG(fp, RS_STK_FRAME_OF, pFName);
#ifdef INTERNAL_BUILD
	LOG(fp, RS_REFERENCE);
	LOGC(fp, " http://wiki.mediatek.inc/pages/viewpage.action?pageId=117214286\n");
#endif
}

/*************************************************************************
* DESCRIPTION
*	����Ƿ���irq�����HWT
*
* PARAMETERS
*	fp: ����ļ����
*	pCsF: ջ֡, �����к�����
*
* RETURNS
*	0: ������irq����HWT
*	1: ����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int CheckIrq2Hwt(FILE *fp, CS_FRAME *pCsF)
{
	char IrqStr[64];
	int IrqIdx;

	if (!strcmp(pCsF->pFName, "generic_handle_irq")) {
		IrqIdx = 0;
		goto _FOUND;
	}
	if (!strcmp(pCsF->pFName, "__handle_domain_irq")) {
		IrqIdx = 1;
		goto _FOUND;
	}
	return 1;
_FOUND:
	IrqStr[sizeof(IrqStr) - 1] = '\0';
	if (pCsF->ArgCnt && pCsF->Para[IrqIdx].Type != CS_NOVAL && pCsF->Para[IrqIdx].Val.n < 0xFFFF/* Ӧ��û��ô���ж� */) {
		if (IrqIdx || IrqIdx2Name(pCsF->Para[IrqIdx].Val.n, IrqStr, sizeof(IrqStr)))
			snprintf(IrqStr, sizeof(IrqStr) - 1, "%d", pCsF->Para[IrqIdx].Val.n);
	} else {
		snprintf(IrqStr, sizeof(IrqStr) - 1, "??");
	}
	LOG(fp, RS_IRQ2HWT, IrqStr);
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	���wd kikerûι��ԭ��
*
* PARAMETERS
*	fp: ����ļ����
*	pCpuCntx: ������Ϣ
*	CpuId: cpu id
*
* RETURNS
*	0: ��֪����ʲôԭ��
*	1: �Ѳ鵽ûι��ԭ��
*
* LOCAL AFFECTED
*
*************************************************************************/
static int CheckWdKicker(FILE *fp, CORE_CNTX *pCpuCntx, TUINT CpuId)
{
	if (PerCpuRbInf(CpuId)->HpState == CPU_DOWN) { /* �µ�״̬, cpu up/down��ס */
		LOG(fp, RS_HOTPLUG2HWT0, GetCStr(pCpuCntx ? RS_CPUDOWN : RS_CPUUP), CpuId);
		return 1;
	}
	//���rt throttle
	return 0;
}

/*************************************************************************
* DESCRIPTION
*	��ʾƽ̨��ص�RD window
*
* PARAMETERS
*	fp: �ļ����
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ShowPlatRdWindow(FILE *fp)
{
#ifdef INTERNAL_BUILD
	const char *pRdWindow;

	switch (GetPlatform()) {
	case 6570: pRdWindow = "Mars Cheng";	break;
	case 6580: pRdWindow = "Sagy Shih";		break;
	case 6595: pRdWindow = "Hanks Chen";	break;
	case 6735:
	case 6737: pRdWindow = "Shun-Chih Yu";	break;
	case 6739: pRdWindow = "Hanks Chen";	break;
	case 6750: pRdWindow = "Lovefool Tai";	break;
	case 6752: pRdWindow = "Chieh-Jay Liu";	break;
	case 6753: pRdWindow = "Hata Tang";		break;
	case 6755: pRdWindow = "Lovefool Tai";	break;
	case 6757:
	case 6758: pRdWindow = "Chieh-Jay Liu";	break;
	case 6759:
	case 6762:
	case 6765: pRdWindow = "Hata Tang";		break;
	case 6771: pRdWindow = "Mars Cheng";	break;
	case 6795: pRdWindow = "Chia-Hao Hsu";	break;
	case 6797: pRdWindow = "Mars Cheng";	break;
	case 6799: pRdWindow = "Hata Tang";		break;
	default: pRdWindow = "http://wiki.mediatek.inc/display/WCP2SS/hw_reboot+owner+table";
	}
	LOG(fp, RS_REFERENCE);
	LOGC(fp, " RD Window(%s)\n", pRdWindow);
#else
	(void)fp;
#endif
}

/*************************************************************************
* DESCRIPTION
*	����BUG
*
* PARAMETERS
*	fp: �ļ����
*	pExcpCntx: �쳣cpu������
*	IsBug: �Ƿ���һ��BUG
*
* RETURNS
*	0: û����
*	1: ����
*
* LOCAL AFFECTED
*
*************************************************************************/
static int HandleBug(FILE *fp, CORE_CNTX *pExcpCntx, int IsBug)
{
#define _BUG_FUNC(Str, RsId, Handler) {Str, sizeof(Str) - 1, RsId, Handler}
	static BUG_DISPATCH BugDispatch[] =
	{
		/* �µ�bug��������� */
		_BUG_FUNC("ipi_check_ack", 0, BugSspmIpiHandler),
		_BUG_FUNC("sysrq_handle_crash", RS_SYSRQ2BUG, BugCommonHandler),
		_BUG_FUNC("__stack_chk_fail", 0, StkFrameOfHandler),
		_BUG_FUNC("tsbat_sysrst_set_cur_state", 0, BugThermalReboot),
		_BUG_FUNC("tscpu_sysrst_set_cur_state", 0, BugThermalReboot),
		_BUG_FUNC("tsabb_sysrst_set_cur_state", 0, BugThermalReboot),
		_BUG_FUNC("tspa_sysrst_set_cur_state", 0, BugThermalReboot),
		_BUG_FUNC("tspmic_sysrst_set_cur_state", 0, BugThermalReboot),
		_BUG_FUNC("sysrst_set_cur_state", 0, BugThermalReboot),
		_BUG_FUNC("_i2c_deal_result", RS_I2C2BUG, BugCommonHandler),
		_BUG_FUNC("check_bytes_and_report", RS_SLUB2BUG, BugCommonHandler),
		_BUG_FUNC("slab_err", RS_SLUB2BUG, BugCommonHandler),
		_BUG_FUNC("object_err", RS_SLUB2BUG, BugCommonHandler),
		_BUG_FUNC("__get_vm_area_node", RS_IRQ_CALL2BUG, BugCommonHandler),
		_BUG_FUNC("mt_pmic_wrap_irq", RS_PMIC2BUG, BugCommonHandler),
		_BUG_FUNC("dpm_wd_handler", RS_DPM_WD_REBOOT, BugCommonHandler), /* <kernel-3.18 */
		_BUG_FUNC("dpm_drv_timeout", RS_DPM_WD_REBOOT, BugCommonHandler), /* >=kernel-3.18 */
		_BUG_FUNC("__schedule_bug", RS_SCHED2BUG_1, BugCommonHandler),
		_BUG_FUNC("hang_detect_thread", 0, Swt2HwtHandler),
	};
	const char *pFName;
	SYM_HDL *pSymHdl;
	int i;

	if (IsBug)
		UpdateExcpTime(0, 0, EXP_BUG);
	pSymHdl = GetSymHdlByAddr(pExcpCntx->RegAry[REG_PC]);
	if (pSymHdl && (pFName = SymAddr2Name(pSymHdl, NULL, pExcpCntx->RegAry[REG_PC], TALLFUN)) != NULL) {
		for (i = ARY_CNT(BugDispatch); i-- > 0; ) {
			if (!strncmp(pFName, BugDispatch[i].pFName, BugDispatch[i].Len)) { /* 89 thermal?? */
				BugDispatch[i].BugProcess(fp, pExcpCntx, &BugDispatch[i]);
				return 1;
			}
		}
	}
	if (!IsBug)
		return 0;
	LOG(fp, RS_BUG_0);
	LOG(fp, RS_CSTIP);
#ifdef INTERNAL_BUILD
	LOG(fp, RS_REFERENCE);
	LOGC(fp, " http://wiki.mediatek.inc/pages/viewpage.action?pageId=105452586\n");
#endif
	return 1;
}

/*************************************************************************
* DESCRIPTION
*	����CPU abort/und�쳣
*
* PARAMETERS
*	fp: �ļ����
*	pExcpCntx: �쳣cpu������
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void HandleFault(FILE *fp, CORE_CNTX *pExcpCntx)
{
	TADDR FaultAddr;
	CS_HDL *pCsHdl;
	TSIZE Val;
	
	switch (GetExInf(&FaultAddr, &Val, pExcpCntx->RegAry, pExcpCntx->Cpsr)) {
	case EX_FAIL:	 LOG(fp, RS_DIE); LOG(fp, RS_CSTIP); break;
	case EX_INST_EA:
		pCsHdl = GetNativeCsHdl(pExcpCntx->RegAry, 2);
		if (pCsHdl && pCsHdl->pNext && pCsHdl->pNext->pFName)
			LOG(fp, RS_INST_EA, pCsHdl->pNext->pFName, FaultAddr);
		else
			LOG(fp, RS_INST_EA2, FaultAddr);
		LOG(fp, RS_CSTIP);
		if (pCsHdl)
			PutCsHdl(pCsHdl);
		break;
	case EX_INST_UND:
#ifndef SST64
		if (Val == 0xE7F001F2) { /* undef */
			HandleBug(fp, pExcpCntx, 1);
			break;
		}
#endif
		LOG(fp, RS_INST_UND, Val);
		LOG(fp, RS_CSTIP);
		break;
	case EX_DATA_UNA: LOG(fp, RS_DATA_UNA, FaultAddr); LOG(fp, RS_CSTIP); break;
	case EX_DATA_EAR: LOG(fp, RS_DATA_EAR, FaultAddr); LOG(fp, RS_CSTIP); break;
	case EX_DATA_EAW:
#ifdef SST64
		if ((FaultAddr&0xFFFF) == 0xDEAD && Val == 0xAEE) {
			HandleBug(fp, pExcpCntx, 1);
			break;
		}
#endif
		if (!FaultAddr && Val == 1 && HandleBug(fp, pExcpCntx, 0)) /* sysrq_handle_crash */
			break;
		LOG(fp, RS_DATA_EAW, Val, FaultAddr);
		LOG(fp, RS_CSTIP);
		break;
	default:; /* normal */
#ifdef SST64
		if (Val == 0xD4210000) { /* brk #0x800 */
			HandleBug(fp, pExcpCntx, 1);
			break;
		}
#endif
		LOG(fp, RS_SEGV_NORMAL);
		UpdateExcpTime(0, 0, EXP_NO_EXP);
	}
}

/*************************************************************************
* DESCRIPTION
*	����thermal reboot
*
* PARAMETERS
*	fp: �ļ����
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void HandleThermalRb(FILE *fp)
{
	LOG(fp, RS_THERMAL_RB);
}

/*************************************************************************
* DESCRIPTION
*	����ATF crash
*
* PARAMETERS
*	fp: �ļ����
*	pKExpInf: �ں��쳣��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void HandleAtfCrash(FILE *fp, KEXP_INF *pKExpInf)
{
	(void)pKExpInf;
	LOG(fp, RS_ATF_CRASH, "SYS_ATF_CRASH");
}

/*************************************************************************
* DESCRIPTION
*	����HW reboot
*
* PARAMETERS
*	fp: �ļ����
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void HandleHwReboot(FILE *fp)
{
	static const char *pMods[] = {"SPM suspend", "deep idle", "sodi", "sodi3"};
	GLB_RBINF * const pGlbRbInf = GlobalRbInf();
	size_t i;

	/* low power��� */
	for (i = 0; i < ARY_CNT(pGlbRbInf->LpState); i++) {
		switch (pGlbRbInf->LpState[i]) {
		case LP_AP:
			LOG(fp, RS_SPM_HW_REBOOT, "AP", pMods[i]);
			LOG(fp, RS_SPM_FLOW);
			goto _LP_REF;
		case LP_FW:
			UpdateExcpTime(0, 0, EXP_SPM_RB);
			LOG(fp, RS_SPM_HW_REBOOT, "SPM", pMods[i]);
			LOG(fp, RS_SPM_FW);
			goto _LP_REF;
		default:;
		}
	}
	LOG(fp, RS_HW_REBOOT);
	ShowPlatRdWindow(fp);
	return;
_LP_REF:;
#ifdef INTERNAL_BUILD
	LOG(fp, RS_REFERENCE);
	LOGC(fp, " http://wiki.mediatek.inc/display/SDA/The+SPM+debug+information+in+SYS_REBOOT_REASON+of+DB\n");
	ShowPlatRdWindow(fp);
#endif
}

/*************************************************************************
* DESCRIPTION
*	����HWT
*
* PARAMETERS
*	fp: �ļ����
*	pKExpInf: �ں��쳣��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void HandleHWT(FILE *fp, KEXP_INF *pKExpInf)
{
	TUINT i, CpuBitMap, tBitMap;
	CORE_CNTX *pCpuCntx;
	CS_HDL *pCsHdl;
	CS_FRAME *pCsF;

	if ((pKExpInf->ValidBit|((1 << pKExpInf->CoreCnt) - 1)) == 0xFFFF) { /* Powerkey Tick fail */
		LOG(fp, RS_PWR_HWT);
		goto _OUT;
	}
	CpuBitMap = pKExpInf->KickBit&~pKExpInf->ValidBit;
	if (CpuBitMap) { /* cup up timeout������������ */
		LOG(fp, RS_HWT_0);
		for (tBitMap = i = 0; CpuBitMap != tBitMap; i++) {
			if (!(CpuBitMap&(1 << i)))
				continue;
			if (tBitMap)
				LOGC(fp, ",");
			tBitMap |= (1 << i);
			LOGC(fp, "%d", i);
		}
		LOG(fp, RS_HOTPLUG2HWT1);
		goto _OUT;
	}
	CpuBitMap = pKExpInf->ValidBit&~pKExpInf->KickBit;
	for (tBitMap = i = 0; CpuBitMap != tBitMap; i++) { /* ���δι����CPU����ջ */
		if (!(CpuBitMap&(1 << i)))
			continue;
		tBitMap |= (1 << i);
		pCpuCntx = GetCoreCntx(i);
		if (CpuBitMap == tBitMap && CheckWdKicker(fp, pCpuCntx, i)) /* ��1��CPUδι�� */
			goto _OUT;
		if (!pCpuCntx)
			continue;
		if (!strcmp(pCpuCntx->ProcName, "hang_detect")) {
			Swt2HwtHandler(fp, NULL, NULL);
			return;
		}
		pCsF = pCsHdl = GetNativeCsHdl(pCpuCntx->RegAry, 3);
		if (!pCsHdl)
			continue;
		if (pCsF->pFName) {
			if (!strcmp(pCsF->pFName, "hang_detect_thread")) {
				PutCsHdl(pCsHdl);
				Swt2HwtHandler(fp, NULL, NULL);
				return;
			}
			if (!strcmp(pCsF->pFName, "__raw_spin_lock")) { /* ��ʱ��������������.. */
				PutCsHdl(pCsHdl);
				break;
			}
		}
		do {
			if (!pCsF->FAddr || (pCsF->Flag&CS_EXCP)) /* ��չ֡/�쳣֡ */
				break;
			if (pCsF->pFName && !CheckIrq2Hwt(fp, pCsF)) {
				PutCsHdl(pCsHdl);
				goto _OUT;
			}
		} while ((pCsF = pCsF->pNext) != NULL);
		PutCsHdl(pCsHdl);
	}
	LOG(fp, RS_HWT_0);
	for (tBitMap = i = 0; CpuBitMap != tBitMap; i++) {
		if (!(CpuBitMap&(1 << i)))
			continue;
		if (tBitMap)
			LOGC(fp, ",");
		tBitMap |= (1 << i);
		LOGC(fp, "%d", i);
	}
	LOG(fp, RS_HWT_1);
	if (pKExpInf->Sec < 30)
		LOG(fp, RS_HWT_2);
	else if (pKExpInf->Sec < 40)
		LOG(fp, RS_HWT_3);
	else
		LOG(fp, RS_HWT_4);
_OUT:
	LOG(fp, RS_REFERENCE);
	LOGC(fp, ONLINE_QS);
	LOGC(fp, "����������Ź����\n");
}

/*************************************************************************
* DESCRIPTION
*	����panic
*
* PARAMETERS
*	fp: �ļ����
*	pExcpCntx: �쳣cpu������
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void HandlePanic(FILE *fp, CORE_CNTX *pExcpCntx)
{
	CS_HDL *pCsHdl;
	CS_FRAME *pCsF;

	pCsF = pCsHdl = GetNativeCsHdl(pExcpCntx->RegAry, 2);
	if (pCsHdl) {
		do {
			if (!pCsF->FAddr || (pCsF->Flag&CS_EXCP)) /* ��չ֡/�쳣֡ */
				break;
			if (!pCsF->pFName)
				continue;
			if (!strcmp(pCsF->pFName, "out_of_memory")) {
				LOG(fp, RS_OOM2PANIC);
				PutCsHdl(pCsHdl);
				return;
			}
			if (!strcmp(pCsF->pFName, "find_child_reaper") || !strcmp(pCsF->pFName, "find_new_reaper")) {
				LOG(fp, access("zcore-1.zip", 0) ? RS_INIT_KILL2PANIC : RS_INIT_NE2PANIC);
				PutCsHdl(pCsHdl);
				return;
			}
		} while ((pCsF = pCsF->pNext) != NULL);
		PutCsHdl(pCsHdl);
	}
	LOG(fp, RS_PANIC_0);
	LOG(fp, RS_CSTIP);
}

/*************************************************************************
* DESCRIPTION
*	�쳣��Դ����
*
* PARAMETERS
*	fp: �ļ����
*	pKExpInf: �ں��쳣��Ϣ
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void FindRootCause(FILE *fp, KEXP_INF *pKExpInf)
{
	CORE_CNTX *pCpuCntx;
	
	LOG(fp, RS_DETAIL);
	if (LastDbgInfCheck(fp)) {
		ShowPlatRdWindow(fp);
		return;
	}
	pCpuCntx = GetCoreCntx(pKExpInf->CpuId);
	if (!pCpuCntx && (pKExpInf->eType != EXP_HW_REBOOT && pKExpInf->eType != EXP_ATF_CRASH && pKExpInf->eType != EXP_THERMAL_RB)) {
		LOG(fp, RS_NOCORE);
		return;
	}
	switch (pKExpInf->eType) {
	case EXP_PANIC: HandlePanic(fp, pCpuCntx); break;
	case EXP_HWT: HandleHWT(fp, pKExpInf); break;
	case EXP_HW_REBOOT: HandleHwReboot(fp); break;
	case EXP_ATF_CRASH: HandleAtfCrash(fp, pKExpInf); break;
	case EXP_THERMAL_RB: HandleThermalRb(fp); break;
	default: HandleFault(fp, pCpuCntx);
	}
}

/*************************************************************************
* DESCRIPTION
*	��ʾ��ϸ���쳣ͷ
*
* PARAMETERS
*	fp: �ļ����
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
__inline static void DisplayExHeader(FILE *fp)
{
	KEXP_INF * const pKExpInf = GetKExpInf();
	const unsigned PlatId = GetPlatform();

	SSTINFO(RS_COLLECT_EX);
	LOG(fp, RS_REPORT_REF);
	LOGC(fp, ONLINE_QS);
	LOGC(fp, "E-Consulter֮NE/KE����������> KE��������\n");
	FindRootCause(fp, pKExpInf);
	if (PlatId)
		LOG(fp, RS_PLATFORM, PlatId);
	LOG(fp, RS_AVER, MtkVer());
	switch (BuildVar()) {
	case BUILD_USER: LOGC(fp, "/user build\n"); break;
	case BUILD_USERDBG: LOGC(fp, "/userdebug build\n"); break;
	case BUILD_ENG: LOGC(fp, "/eng build\n");  break;
	default: LOGC(fp, "/??\n");
	}
	LOG(fp, RS_EXP_TIME, pKExpInf->Sec, pKExpInf->USec);
	if (pKExpInf->pExcpUtc)
		LOGC(fp, ", %s", pKExpInf->pExcpUtc);
	LOGC(fp, "\n\n\n");
}

/*************************************************************************
* DESCRIPTION
*	��ʾ��Ҫ�ķ����ļ�
*
* PARAMETERS
*	fp: �ļ����
*
* RETURNS
*	��
*
* LOCAL AFFECTED
*
*************************************************************************/
static void ShowNeededSym(FILE *fp)
{
	char Buf[1024], *pStr;
	FILE *tp;
	int Len;

	if (GetKExpInf()->eType == EXP_ATF_CRASH) {
		LOGC(fp, "���⻹��bl31.elf�ļ�, ���ļ������dbƥ�䡣\n������鿴MOL�ϵ�'[FAQ15094]����ATF CRASH��Ҫ�ṩ��Щ�����ļ���MTK'\n");
		return;
	}
	Buf[sizeof(Buf) - 1] = '\0';
	pStr = GetString(SymName2Addr(pVmSHdl, NULL, "linux_banner", TGLBVAR));
	if (pStr) {
		strncpy(Buf, pStr, sizeof(Buf) - 1);
		Free(pStr);
	} else if ((tp = fopen("SYS_VERSION_INFO", "r")) != NULL) {
		if (!fgets(Buf, sizeof(Buf), tp))
			strcpy(Buf, "?");
		fclose(tp);
	} else {
		strcpy(Buf, "?");
	}
	Len = strlen(Buf);
	if (Len && Buf[Len - 1] == '\n')
		Buf[Len - 1] = '\0';
	if ((pVmSHdl->Flag&(SYM_LOAD|SYM_LCLSYM|SYM_MATCH)) != (SYM_LOAD|SYM_LCLSYM|SYM_MATCH)) /* ȱ��vmlinux */
		LOG(fp, RS_NEED_VMFILE, "out/target/product/$proj/obj/KERNEL_OBJ/vmlinux", Buf);
}


/*************************************************************************
*============================= track section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	�����쳣�����ɱ���
*
* PARAMETERS
*	fp: �ļ����
*	pOutFile: ����ļ���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
void ExceptionTrack(FILE *fp, const char *pOutFile)
{	
	SSTINFO(RS_INIT_MODULE);
	DbInit();
	ModuleInit();
	MmInit();
	KernelInit();
	DriversInit();

	DisplayExHeader(fp);
	if (IS_LOGEIGEN())
		DispAndRecRelatedCr(fp, SstOpt.pCrId);
	TaskTrack(fp);
	SlubTrack(fp);
	ShowErrLog(fp);
	ShowNeededSym(fp); /* ���������û�л���!!! */
	fflush(fp);
	FeedBack();
#ifdef _WIN32
	if (!(SstOpt.Flag&SST_HVIEW_BIT))
		ShellExecute(NULL, NULL, pOutFile, NULL, NULL, SW_SHOWNORMAL);
#endif
	if (GetKExpInf()->mType != MRD_T_NONE)
		GenScript();

	DriversDeInit();
	KernelDeInit();
	MmDeInit();
	ModuleDeInit();
	DbDeInit();
	
	SSTINFO(RS_ANS_DONE);
	if (GetKExpInf()->mType != MRD_T_NONE)
		CommandLoop();
}
