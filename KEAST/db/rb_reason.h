/*********************************************************************************
* Copyright(C),2016-2017,MediaTek Inc.
* FileName:	rb_reason.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2016/3/19
* Description: SYS_REBOOT_REASON信息提取
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
	TUINT IrqSIdx, IrqEIdx, IrqSTime, IrqETime; /* 进入/退出irq的编号和时间(单位: ms) */
	/* 这里添加新item */
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
	enum HP_EVENT /* 和kernel一致 */
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
	TUINT HpFuncIdx, HpFuncSec/* 函数调用的时间(单位: s) */, InHotPlug;
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
		HANGD_NONE, /* 必须放在第1位,未初始化值 */
		HANGD_NORMAL,
		HANGD_BT,
		HANGD_SWT, /* db在这个阶段抓取 */
		HANGD_NE,
		HANGD_REBOOT, /* surfaceflinger/system_server */
	} HdState;
	/* 这里添加新item */
} GLB_RBINF;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	提取per cpu的最后信息
*
* PARAMETERS
*	CpuId: cpu索引
*
* RETURNS
*	信息内容
*
* GLOBAL AFFECTED
*
*************************************************************************/
PCPU_RBINF *PerCpuRbInf(TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	提取全局的最后信息
*
* PARAMETERS
*	无
*
* RETURNS
*	信息内容
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
*	初始化reboot reason模块
*
* PARAMETERS
*	无
*
* RETURNS
*	0: reboot reason模块关闭
*	1: reboot reason模块可用
*
* GLOBAL AFFECTED
*
*************************************************************************/
int RbReasonInit(void);

/*************************************************************************
* DESCRIPTION
*	注销reboot reason模块
*
* PARAMETERS
*	无
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void RbReasonDeInit(void);

#endif
