/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	task.h
* Author:		Guangye.Yang
* Version:	V1.2
* Date:		2013/9/29
* Description: task����
**********************************************************************************/
#ifndef _TASK_H
#define _TASK_H

#include <ttypes.h>
#include <mrdump.h>

#ifdef SST64
#define THREAD_SIZE		 0x4000
#else
#define THREAD_SIZE		 0x2000
#endif
#define THDSTK_BASE(SP)	 ((SP)&~(THREAD_SIZE - 1))

typedef const struct _PT_REGS
{
	TSIZE RegAry[REG_NUM], Cpsr, OrgR0;
#ifdef SST64
	TSIZE SysCallNo; /* kernel4.4: orig_addr_limit, unused��ȱ��ûӰ�� */
#endif
} PT_REGS;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ȡtask������Ϣ
*
* PARAMETERS
*	pTskInf: ����task������Ϣ
*	TskAddr: task_struct��ַ
*
* RETURNS
*	NULL: ʧ��
*	����: task������Ϣ
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef const struct _TSK_INF
{
#define TASK_RUNNING			0x0000
#define TASK_INTERRUPTIBLE		0x0001
#define TASK_UNINTERRUPTIBLE	0x0002
#define __TASK_STOPPED			0x0004
#define __TASK_TRACED			0x0008
#define MTK_TASK_IO_WAIT		0x000C
/* in tsk->exit_state */
#define EXIT_ZOMBIE				0x0010
#define EXIT_DEAD				0x0020
/* in tsk->state again */
#define TASK_DEAD				0x0040
#define TASK_WAKEKILL			0x0080
#define TASK_WAKING				0x0100
#define TASK_STATE_MAX			0x0200

/* Convenience macros for the sake of set_task_state */
#define TASK_KILLABLE			(TASK_WAKEKILL|TASK_UNINTERRUPTIBLE)
#define TASK_STOPPED			(TASK_WAKEKILL|__TASK_STOPPED)
#define TASK_TRACED				(TASK_WAKEKILL|__TASK_TRACED)

/* Convenience macros for the sake of wake_up */
#define TASK_NORMAL				(TASK_INTERRUPTIBLE|TASK_UNINTERRUPTIBLE)
#define TASK_ALL				(TASK_NORMAL|__TASK_STOPPED|__TASK_TRACED)
#define TASK_REPORT				(TASK_RUNNING|TASK_INTERRUPTIBLE|TASK_UNINTERRUPTIBLE|__TASK_STOPPED|__TASK_TRACED) /* get_task_state() */

#define TSK_STATE2STR			"RSDTtZXxKW"
	TSSIZE State;
	TADDR StkAddr;
	TSIZE RegAry[REG_NUM];

#define PF_EXITING		0x00000004	/* getting shut down */
#define PF_EXITPIDONE	0x00000008	/* pi exit done on shut down */
#define PF_VCPU			0x00000010	/* I'm a virtual CPU */
#define PF_WQ_WORKER	0x00000020	/* I'm a workqueue worker */
#define PF_FORKNOEXEC	0x00000040	/* forked but didn't exec */
#define PF_MCE_PROCESS	0x00000080	/* process policy on mce errors */
#define PF_SUPERPRIV	0x00000100	/* used super-user privileges */
#define PF_DUMPCORE		0x00000200	/* dumped core */
#define PF_SIGNALED		0x00000400	/* killed by a signal */
#define PF_MEMALLOC		0x00000800	/* Allocating memory */
#define PF_NPROC_EXCEEDED 0x00001000 /* set_user noticed that RLIMIT_NPROC was exceeded */
#define PF_USED_MATH	0x00002000	/* if unset the fpu must be initialized before use */
#define PF_USED_ASYNC	0x00004000	/* used async_schedule*(), used by module init */
#define PF_NOFREEZE		0x00008000	/* this thread should not be frozen */
#define PF_FROZEN		0x00010000	/* frozen for system suspend */
#define PF_FSTRANS		0x00020000	/* inside a filesystem transaction */
#define PF_KSWAPD		0x00040000	/* I am kswapd */
#define PF_MEMALLOC_NOIO 0x00080000 /* Allocating memory without IO involved */
#define PF_LESS_THROTTLE 0x00100000 /* Throttle me less: I clean memory */
#define PF_KTHREAD		0x00200000	/* I am a kernel thread */
#define PF_RANDOMIZE	0x00400000	/* randomize virtual address space */
#define PF_SWAPWRITE	0x00800000	/* Allowed to write to swap */
#define PF_VIP_TASK		0x02000000	/* Set vip task */
#define PF_NO_SETAFFINITY 0x04000000 /* Userland is not allowed to meddle with cpus_allowed */
#define PF_MCE_EARLY	0x08000000	/* Early kill for mce process policy */
#define PF_MUTEX_TESTER 0x20000000	/* Thread belongs to the rt mutex tester */
#define PF_FREEZER_SKIP 0x40000000	/* Freezer should not count it as freezable */
#define PF_SUSPEND_TASK 0x80000000	/* this thread called freeze_processes and should not be frozen */
	TUINT Flags;

#define TSK_THREAD				0x0001 /* �Ƿ����߳� */
#define TSK_ONCPU				0x0002 /* �Ƿ���CPU���� */
#define TSK_ONRQ				0x0004 /* �Ƿ���run queue */
#define TSK_CPUIDX				28 /* ��4λΪCPU idx */
	TUINT ExtFlag;

#define SCHED_NORMAL			0
#define SCHED_FIFO				1
#define SCHED_RR				2
#define SCHED_BATCH				3
/* SCHED_ISO: reserved but not implemented yet */
#define SCHED_IDLE				5
	TUINT Policy;

#define MAX_RT_PRIO				100
#define MAX_PRIO				(MAX_RT_PRIO + 40)
#define DEFAULT_PRIO			(MAX_RT_PRIO + 20)
	TUINT CorePrio; /* �ں�ʹ�õ����ȼ������ܱ���ʱ����(RT mutex), [0, MAX_PRIO), ֵԽС���ȼ�Խ�󣬵ȼ�task_struct.prio */
	TUINT Prio;		/* �û���ʾ�����ȼ���[1, MAX_RT_PRIO): ʵʱ���ȼ�,ֵԽ�����ȼ�Խ��[MAX_RT_PRIO, MAX_PRIO): ��ͨ���ȼ�,ֵԽС���ȼ�Խ�� */
	TINT Pid, Ppid;
	TUINT WaitTime; /* �ȴ�ʱ��(��λ: ��) */
	char Comm[TASK_COMM_LEN + 1];
} TSK_INF;
TSK_INF *GetTskInf(struct _TSK_INF *pTskInf, TADDR TskAddr);

/*************************************************************************
* DESCRIPTION
*	�������̵�task_struct
*
* PARAMETERS
*	pFunc: �ص�����
*		TskAddr: task_struct��ַ
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
int ForEachProcAddr(int (*pFunc)(TADDR TskAddr, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	���������������̵߳�task_struct
*
* PARAMETERS
*	TskAddr: ����task_struct��ַ
*	pFunc: �ص�����
*		TskAddr: task_struct��ַ
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
int ForEachThdAddr(TADDR TskAddr, int (*pFunc)(TADDR TskAddr, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	���SP�Ƿ����
*
* PARAMETERS
*	Sp: stack pointer
*	CpuId: cpu id
*
* RETURNS
*	0: ��������Ϣ����
*	1: ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
int StackIsOverflow(TSIZE Sp, TUINT CpuId);

/*************************************************************************
* DESCRIPTION
*	��ȡpreempt_disable_ip
*
* PARAMETERS
*	TskAddr: ����task_struct��ַ
*
* RETURNS
*	0: ����/û�п�CONFIG_DEBUG_PREEMPT
*	����: preempt_disable_ipֵ
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR GetTskPreemptDisIp(TADDR TskAddr);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	��ʼ��taskģ��
*
* PARAMETERS
*	��
*
* RETURNS
*	0: taskģ��ر�
*	1: taskģ�����
*
* GLOBAL AFFECTED
*
*************************************************************************/
int TaskInit(void);

/*************************************************************************
* DESCRIPTION
*	ע��taskģ��
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
void TaskDeInit(void);

#endif
