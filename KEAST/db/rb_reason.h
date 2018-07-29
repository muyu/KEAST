/*********************************************************************************
* Copyright(C),2016-2017,MediaTek Inc.
* FileName:	rb_reason.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2016/3/19
* Description: SYS_REBOOT_REASON��Ϣ��ȡ
**********************************************************************************/
#ifndef _RB_REASON_H
#define _RB_REASON_H


typedef const struct _PCPU_RBINF
{
	TSIZE Dormant;
	enum CPU_HPSTATE
	{
		CPU_NONE, CPU_UPING, CPU_UP, CPU_DOWNING, CPU_DOWN
	} HpState;
	TUINT IrqSIdx, IrqEIdx, IrqSTime, IrqETime; /* ����/�˳�irq�ı�ź�ʱ��(��λ: ms) */
	/* ���������item */
} PCPU_RBINF;

enum LP_IDX
{
	LP_SPM_SUSPEND,
	LP_DEEPIDLE,
	LP_SODI,
	LP_SODI3,
	LP_MAX
};

typedef const struct _GLB_RBINF
{
	enum LP_STATE
	{
		LP_NONE, LP_AP, LP_FW
	} LpState[LP_MAX];
	enum HP_EVENT /* ��kernelһ�� */
	{
		CPU_ONLINE = 2,
		CPU_UP_PREPARE,
		CPU_UP_CANCELED,
		CPU_DOWN_PREPARE,
		CPU_DOWN_FAILED,
		CPU_DEAD,
		CPU_DYING,
		CPU_POST_DEAD,
		CPU_STARTING,
	} HpEvent;
	TUINT SrcCpuId, DstCpuId;
	TUINT HpFuncIdx, HpFuncSec/* �������õ�ʱ��(��λ: s) */, InHotPlug;
	TSIZE HpFuncAddr;
	enum
	{
		DRAMC_GATING = 1,
		DRAMC_SELFTEST = 2,
		DRAMC_INIT = 4,
		DRAMC_TA2 = 8,
	} DRamFlag;
	TUINT RbCnt;
	TUINT SysTRAddr[8], SysTWAddr[8];
	TUINT PeriMon[5];
	TUINT InfraInf;
	enum
	{
		HANGD_NONE, /* ������ڵ�1λ,δ��ʼ��ֵ */
		HANGD_NORMAL,
		HANGD_BT,
		HANGD_SWT, /* db������׶�ץȡ */
		HANGD_NE,
		HANGD_REBOOT, /* surfaceflinger/system_server */
	} HdState;
	/* ���������item */
} GLB_RBINF;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡper cpu�������Ϣ
*
* PARAMETERS
*	CpuId: cpu����
*
* RETURNS
*	��Ϣ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
PCPU_RBINF *PerCpuRbInf(TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	��ȡȫ�ֵ������Ϣ
*
* PARAMETERS
*	��
*
* RETURNS
*	��Ϣ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
GLB_RBINF *GlobalRbInf(void);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��reboot reasonģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: reboot reasonģ��ر�
*	1: reboot reasonģ�����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int RbReasonInit(void);

/*************************************************************************
* DESCRIPTION
*	ע��reboot reasonģ��
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
void RbReasonDeInit(void);

#endif
