/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	string_res.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: �ַ�����Դ
**********************************************************************************/
#ifndef _STRING_RES_H
#define _STRING_RES_H

#include <stdio.h>
#include <ttypes.h>

#define INTERNAL_BUILD
#define RS_MAP \
	_NL(RS_USAGE,					"�÷�: %s [ -s|-lang|-t|-cmm|-gdb|-l|-h|<dir> ]\n" \
									"	-s Ŀ¼: ָ�������ļ�Ŀ¼, Ĭ��Ϊ��ǰĿ¼" \
									"	-lang l: lΪ'eng'ʱ�л�ΪӢ��, lΪ'tw'ʱ�л�Ϊ����, Ĭ��Ϊ����\n" \
									"	-t:      ��ʾ���н�����Ϣ\n" \
									"	-cmm:    ����cmm�ű�\n" \
									"	-gdb:    ����gdb�ű�\n" \
									"	-l Ŀ¼: ָ����װĿ¼, ���ڲ��ҿ��ýű�\n" \
									"	-h:      ��ʾ�ð���\n" \
									"	<dir>:   ��������Ŀ¼, Ĭ��Ϊ��ǰĿ¼\n", \
									"Usage: %s [ -s|-lang|-t|-cmm|-gdb|-l|-h|<path> ]\n" \
									"	-s dir:  Specify symbol files directory, use current directory by default\n" \
									"	-lang l: Specify english while l is 'eng', traditional while l is 'tw', simplified by default\n" \
									"	-t:      Display all processes info\n" \
									"	-cmm:    Generate cmm script\n" \
									"	-gdb:    Generate gdb script\n" \
									"	-l dir:  Specify install directory, use to find script\n" \
									"	-h:      Display this help\n" \
									"	<dir>:   Directory waiting analyze, current directory by default\n") \
									\
	_NL(RS_ERROR,					"����: ", \
									"Error: ") \
									\
	_NL(RS_WARNING,					"����: ", \
									"Warning: ") \
									\
	_NL(RS_AVER,					"�汾    : %s", \
									"Version  : %s") \
									\
	_NL(RS_EXP_TIME,				"�쳣ʱ��: %d.%06d��", \
									"Exp time : %d.%06ds") \
									\
	_NL(RS_PLATFORM,				"ƽ̨    : MT%d\n", \
									"Platform : MT%d\n") \
									\
	_NL(RS_UNKNOWN_ARG,				"δ֪�Ĳ���: %s\n", \
									"Unrecognized argument: %s\n") \
									\
	_NL(RS_EDIR,					"�����Ŀ¼: %s\n", \
									"Incorrect directory: %s\n") \
									\
	_NL(RS_NEED_ARG,				"ѡ��%s��Ҫ%d������\n", \
									"Option %s requires %d argument(s)\n") \
									\
	_NL(RS_LOADING,					"����%s ...\n", \
									"Loading %s ...\n") \
									\
	_NL(RS_FILE_MAP,				"%sӳ��%sʧ��\n", \
									"Fail to map %s's %s\n") \
									\
	_NL(RS_FILE_UNMAP,				"����%sӳ��ʧ��\n", \
									"Fail to unmap %s\n") \
									\
	_NL(RS_FILE_WRITE,				"д��%sʧ��\n", \
									"Fail to write %s\n") \
									\
	_NL(RS_FILE_DAMAGED,			"�ļ�%s��\n", \
									"File %s damaged\n") \
									\
	_NL(RS_NO_DBGINF,				"%sû�е�����Ϣ\n", \
									"No symbols in %s\n") \
									\
	_NL(RS_ELF_HEADER,				"%s��ELF�ļ�, ͷ����Ϣ%c%c%c%c\n", \
									"'%s' isn't ELF file, magic header: %c%c%c%c\n") \
									\
	_NL(RS_ELF_TYPE,				"%s����%s ELF�ļ�\n", \
									"'%s' isn't %s ELF file\n") \
									\
	_NL(RS_ELF_FORMAT,				"��֧��%s���͵�ELF�ļ�(%s)\n", \
									"Don't support %s format of ELF file(%s)\n") \
									\
	_NL(RS_NO_LOAD,					"�Ҳ���%s��%s�ڴ��\n", \
									"Fail to find '%s' of %s memory segment\n") \
									\
	_NL(RS_NO_SEG,					"�Ҳ���%s��%s��\n", \
									"Fail to find <%s> %s segment\n") \
									\
	_NL(RS_NO_SEC,					"�Ҳ���%s��%s��\n", \
									"Fail to find <%s> %s section\n") \
									\
	_NL(RS_MMUTBL,					"%sҳ����\n", \
									"Damaged %s page table\n") \
									\
	_NL(RS_CTRLTBL,					"δ֪��ctrl block��ʶ: %s\n", \
									"Unrecognized ctrl block magic: %s\n") \
									\
	_NL(RS_EADDR,					"�����%s��%s�ĵ�ַ: "TADDRFMT"\n", \
									"%s of %s Incorrect address: "TADDRFMT"\n") \
									\
	_NL(RS_VM_OVERFLOW,				"���ݶ����\n", \
									"Region overflow\n") \
									\
	_NL(RS_CORE_FORMAT,				"��֧������%s�ļ�\n", \
									"Don't support this %s format\n") \
									\
	_NL(RS_NOCORE,					"�Ҳ����쳣������Ϣ�򲻴���mrdump�ļ�\n", \
									"No exception found or no mrdump file exist\n") \
									\
	_NL(RS_SYM_UNMATCH,				"%s��mrdump��ƥ��, ���ڲ���ǰ����symbolsĿ¼\n", \
									"%s and mrdump are unmatched, please leave symbols folder before test\n") \
									\
	_NL(RS_VM_MR_UNMATCHED1,		"����%d������ƥ��ʧ��\n", \
									"%d symbols are unmatched\n") \
									\
	_NL(RS_DEMAGED1,				"mrdump�������򱻲Ȼ���bit��ת\n", \
									"Mrdump is demaged or bit reversed\n") \
									\
	_NL(RS_DEMAGED_REG,				"��%d������, "TADDRFMT, \
									"Region %d, "TADDRFMT) \
									\
	_NL(RS_OOM,						"���벻���ڴ�\n", \
									"Out of memory\n") \
									\
	_NL(RS_INCOMPLETE_NOTE,			"NOTE��ȱ��%s��Ϣ\n", \
									"No %s in NOTE segment\n") \
									\
	_NL(RS_PT_NOTE_SIZE,			"mrdump���PT_NOTE size����, ���%dB\n", \
									"Incorrect PT_NOTE size in mrdump, larger %dB\n") \
									\
	_NL(RS_ANS_DONE,				"�������\n", \
									"Analysis is completed\n") \
									\
	_NL(RS_REPORT,					"��������.txt", \
									"Analysis report.txt") \
									\
	_NL(RS_CREATE,					"����%sʧ��\n", \
									"Fail to create %s\n") \
									\
	_NL(RS_SEG_SIZE,				"%s��%s�δ�С(%dB)����\n", \
									"%s of %s segment size(%dB) is incorrect\n") \
									\
	_NL(RS_SEG_VERSION,				"��֧��%s��%s�İ汾0x%X\n", \
									"Don't support %s of %s version 0x%X\n") \
									\
	_NL(RS_GNU_AUG,					"��֧��%s��%s�Ĳ���%c\n", \
									"Don't support %s of %s augmentation %c\n") \
									\
	_NL(RS_DWFOP_UNDERFLOW,			"%s��%s�Ĵ���ջ����\n", \
									"%s of %s register stack underflow\n") \
									\
	_NL(RS_INVALID_DWFSP_RULE,		"��Ч��%s��%s��SP����\n", \
									"Invalid %s of %s's SP rule\n") \
									\
	_NL(RS_DWEXPROP_OVERFLOW,		"%s��%s���ʽջ���\n", \
									"%s of %s expression stack overflow\n") \
									\
	_NL(RS_DWEXPROP_UNDERFLOW,		"%s��%s���ʽջ����\n", \
									"%s of %s expression stack underflow\n") \
									\
	_NL(RS_INVALID_IDX,				"��Ч��%s��%s%s����: %d\n", \
									"Invalid %s of %s %s index: %d\n") \
									\
	_NL(RS_REGISTER,				"�Ĵ���", \
									"register") \
									\
	_NL(RS_STACK,					"ջ", \
									"stack") \
									\
	_NL(RS_SIZE,					"�ߴ�", \
									"size") \
									\
	_NL(RS_INIT_MODULE,				"��ʼ��ģ�� ...\n", \
									"Init module ...\n") \
									\
	_NL(RS_COLLECT_EX,				"��ȡ�쳣��Ϣ ...\n", \
									"Collect exception info ...\n") \
									\
	_NL(RS_EX_HEAD,					"== �쳣����v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"(�����ο�) ==\n", \
									"== Exception report v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"(For Reference) ==\n") \
									\
	_NL(RS_REPORT_REF,				"������:", \
									"Manual   :") \
									\
	_NL(RS_REFERENCE,				"�ο���Ϣ:", \
									"Reference:") \
									\
	_NL(RS_DIE,						"����Oops", /* need follow RS_CSTIP */ \
									"Oops happened") \
									\
	_NL(RS_BUG_0,					"����ʧ��{��������BUG()/BUG_ON()}", /* need follow RS_CSTIP */ \
									"Assert fail{call BUG()/BUG_ON()}") \
									\
	_NL(RS_PANIC_0,					"Kernel panic{��������panic()}", /* need follow RS_CSTIP */ \
									"Kernel panic{call panic()}") \
									\
	_NL(RS_OOM2PANIC,				"�ڴ�ľ������ڴ���ͷ�, �����SYS_KERNEL_LOG����ڴ���Ϣ������/�ں��Ƿ�����ڴ�й©\n", \
									"Out of memory and no killable processes. Plase check SYS_KERNEL_LOG mem info to see if process/kernel has mem leak or not\n") \
									\
	_NL(RS_INIT_NE2PANIC,			"init����NE, ��Ҫ����NE(coredump��db���zcore-1.zip, ����logviewer�⿪)\n", \
									"init process NE, need analyze NE(zcore-1.zip in db is NE db which can be opened by logviewer)\n") \
									\
	_NL(RS_INIT_KILL2PANIC,			"init���̱�ɱ, ��Ҫ������ɱԭ��\n", \
									"init process is killed by other process, need find murderer\n") \
									\
	_NL(RS_HWT_0,					"���Ź���λ, ����CPU", \
									"Watch dog timeout while CPU") \
									\
	_NL(RS_HWT_1,					"û��ι��", \
									"don't kick") \
									\
	_NL(RS_HWT_2,					", ��ʱʱ�� < 30s������Ӳ��ʱ���Ƿ�����\n", \
									", overtime < 30s, please check HW clock work normal or not\n") \
									\
	_NL(RS_HWT_3,					", 30s��ʱ, ��ο�[FAQ09189]\n", \
									", timeout 30s, please refer [FAQ09189]\n") \
									\
	_NL(RS_HWT_4,					", �����Ӧ����ջ�Ƿ����������ʱ����ж�\n", \
									", please check the callstacks have dead lock or long time disable interrupt\n") \
									\
	_NL(RS_SWT2HWT,					"SWT���̿��������Ź���λ����Ҫ������SWT����\n", \
									"SWT hang up cause watch dog timeout, need check SWT problem\n") \
									\
	_NL(RS_HDT2HWT1,				"%s��鵽watchdog�޷���֪�Ŀ���, ���kernel�Ƿ�����ART����, �����뿴%s\n", \
									"'%s' detect hang while watchdog not detected, check kernel hang or ART deadlock, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT2,				"%s��鵽��������ջʱ����, ���debuggerd״̬, �����뿴%s\n", \
									"'%s' detect hang when dump callstack, check debuggerd status, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT3,				"%s��鵽SWTʱ����, ֱ�ӷ���SWT db, �����뿴%s\n", \
									"'%s' detect hang while SWT, check SWT db, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT4,				"%s��鵽SS/SF NEץdbʱ����, �����뿴%s\n", \
									"'%s' detect hang when SS/SF NE, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT5,				"%s��鵽SS/SF����ʱ����, �����뿴%s\n", \
									"'%s' detect hang when SS/SF reboot, refer %s for detail\n") \
									\
	_NL(RS_IRQ2HWT,					"�ж�%sƵ�����������Ź���λ����Ҫ���IRQ��������\n", \
									"IRQ(%s) trigger frequently cause watch dog timeout, need check the corresponding IRQ's driver\n") \
									\
	_NL(RS_PWR_HWT,					"����/���ѿ����������Ź���λ����Ҫ��������������\n", \
									"system hang up cause watch dog timeout when resume/suspend, need check hang problem\n") \
									\
	_NL(RS_CPUUP,					"����", \
									"power up") \
									\
	_NL(RS_CPUDOWN,					"�ر�", \
									"shut down") \
									\
	_NL(RS_HOTPLUG2HWT0,			"%sCPU%dʱ��ס, ��Ҫ���hps_main����ջ��������\n", \
									"blocked when %s CPU%d, need check hps_main's callstack for hang issue\n") \
									\
	_NL(RS_HOTPLUG2HWT1,			"��������ʧ��, ����kernel log����failed to come online�ؼ��ֲ�����ԭ��\n", \
									" failed to come online before, search the keyword on kernel log analyze why\n") \
									\
	_NL(RS_THERMAL_0,				"��������������thermal owner����\n", \
									"Thermal reboot, please check with thermal owner\n") \
									\
	_NL(RS_THERMAL_1,				"%s��������, ����thermal owner����\n", \
									"%s thermal reboot, please check with thermal owner\n") \
									\
	_NL(RS_DPM_WD_REBOOT,			"��ģ���ڻ���/���߹��̳�ʱ������kernel log�鿴oopsǰ����ջ��Ϣ\n", \
									"There has module time out in suspend/resume flow, check kernel log for backtrace to debug\n") \
									\
	_NL(RS_COMMAND_LINE,			"������  : %s\n", \
									"Command line : %s\n") \
									\
	_NL(RS_OUT_CPUS,				"��ȡCPU��Ϣ ...\n", \
									"Output cpu info ...\n") \
									\
	_NL(RS_CPUS_INFO,				"== CPU��Ϣ ==\n", \
									"== CPUs info ==\n") \
									\
	_NL(RS_CRASH_CPU,				"����CPU��Ϣ:\n", \
									"Crashed CPU info:\n") \
									\
	_NL(RS_NOT_KICK,				"��ι��CPU��Ϣ:\n", \
									"Not kick cpu info:\n") \
									\
	_NL(RS_OTHER_CPU,				"����CPU��Ϣ:\n", \
									"Others CPU info:\n") \
									\
	_NL(RS_EXEC_T,					", ��ִ��%d��", \
									", executed %ds") \
									\
	_NL(RS_CPU_OFF,					"�ر�\n\n", \
									"offline\n\n") \
									\
	_NL(RS_CPU_DIS_INT,				", �ж�: ��", \
									", Interrupt: disable") \
									\
	_NL(RS_CPU_IDLE,				"����\n", \
									"idle\n") \
									\
	_NL(RS_REG_EXP,					"   �Ĵ����쳣: ", \
									"   registers exception: ") \
									\
	_NL(RS_STK_OVERFLOW,			"�ں�ջ���\n", \
									"Kernel stack overflow\n") \
									\
	_NL(RS_STK_FRAME_OF,			"%s()��������ջ֡Խ������, ����ֲ�����ı���/�����Ƿ����Խ����Ϊ\n", \
									"%s() function stack frame corruption! Please check all local variable/array has overflow or not.\n") \
									\
	_NL(RS_SP_INCORRECT,			"SP("TADDRFMT")�쳣\n", \
									"SP("TADDRFMT") is abnormal\n") \
									\
	_NL(RS_CS_END,					"\t== ջ���� ==\n", \
									"\t== end of frame ==\n") \
									\
	_NL(RS_PARA,					"����", \
									"Para") \
									\
	/* ============================== malloc_tracker.c ============================== */ \
	_NL(RS_VAR_VAL,					"�޷���ȡ����%s��ֵ\n", \
									"Fail to get %s variable value\n") \
									\
	_NL(RS_NO_VAR,					"�Ҳ���%s����\n", \
									"Fail to find %s variable\n") \
									\
	_NL(RS_NO_FUN,					"�Ҳ���%s����\n", \
									"Fail to find %s function\n") \
									\
	_NL(RS_NO_KSYM,					"�Ҳ���%s������Ϣ\n", \
									"Fail to find %s symbols info\n") \
									\
	_NL(RS_UN_PATTERN,				"δ֪��%sʶ����(0x%08X)������ϵowner���\n", \
									"Unrecognized %s pattern(0x%08X), please contact owner to add\n") \
									\
	_NL(RS_CORRUPTED,				"%s�������󣬿��ܱ��Ȼ�\n", \
									"%s is incorrect, maybe is corrupted\n") \
									\
	_NL(RS_LK_CORRUPTED,			"%s������󣬿��ܱ��Ȼ�\n", \
									"%s list is incorrect, maybe is corrupted\n") \
									\
	_NL(RS_UNKNOWN_CODE,			"δ֪��%s�����룬����ϵowner���\n", \
									"Unrecognized %s error code, please contact owner to add\n") \
									\
	_NL(RS_GET_ADDR,				"��ȡ%s�ĵ�ַʧ��\n", \
									"Fail to get address of %s\n") \
									\
	_NL(RS_ADDR,					"%s�ĵ�ַ����\n", \
									"Address of %s is incorrect\n") \
									\
	_NL(RS_SOBJ_E_REDZ,				" ��ɫ��", \
									" red zone") \
									\
	_NL(RS_SOBJ_E_PAD,				" ���������", \
									" pading zone") \
									\
	_NL(RS_SOBJ_E_FP,				" ��������ָ��", \
									" freepointer") \
									\
	_NL(RS_SOBJ_CUR,				"���Ȼ��������ǵ�ǰslub objԽ�������\n", \
									" is corrupted, maybe current slub obj overflow, please check it\n") \
									\
	_NL(RS_SOBJ_UAF,				"�ÿ�slub obj�ͷź���ʹ����\n", \
									"This slub obj used after Free\n") \
									\
	_NL(RS_BUDDY_CHK,				"== ���ϵͳ ==\n", \
									"== buddy system ==\n") \
									\
	_NL(RS_SLUB_CHK,				"== slub��� ==\n", \
									"== slub check ==\n") \
									\
	_NL(RS_CHK_BUDDY,				"�����ϵͳ ...\n", \
									"Checking buddy ...\n") \
									\
	_NL(RS_CHK_SLUB,				"���slub ...\n", \
									"Checking slub ...\n") \
									\
	_NL(RS_OK,						"����\n", \
									"Healthy\n") \
									\
	_NL(RS_VAR_CHANGE,				"%s�����ṹ�б仯������ϵowner���\n", \
									"%s variable structure has changed, please contact owner to add\n") \
									\
	_NL(RS_BUDDY_INFO,				"�ܹ�: %d MB, �ɹ���: %d MB, ����: %d KB, ��ʼ: 0x%08X\n", \
									"Total: %d MB, managed: %d MB, Free: %d KB, start address: 0x%08X\n") \
									\
	_NL(RS_BUDDY_ZONE_INFO,			"%s: �ܹ�: %d MB, �ɹ���: %d MB, ����: %d KB, ��ʼ: 0x%08X\n", \
									"%s total: %d MB, managed: %d MB, Free: %d KB, start address: 0x%08X\n") \
									\
	/* ============================== callstack.c ============================== */ \
	_NL(RS_NEED_VMFILE,				"�����KE�����⣬�鷳��˾�Ժ��ṩdb��ͬʱ�ṩ��Ӧ��vmlinux����˾����Ѹ�ٷ��������ٲ���Ҫ������\n���⻹��%s�ļ�, ���ļ������dbƥ��, ��Ӧʱ�����:\n%s\n������鿴MOL�ϵ�'[FAQ06985]KE����������ж�vmlinux��log�Ƿ�ƥ��'\n", \
									"If KE(kernel exception) problems, trouble later to provide db while providing corresponding vmlinux, so we can quickly analyze and reduce unnecessary delays.\nNeed %s. The file should be compiled at the same time, The timestamp is:\n%s\nPlease check '[FAQ06985]How to check if vmlinux matches log after KE takes place?' on MOL\n") \
									\
	_NL(RS_MBT,						"�������ջ:\n", \
									"Malloc callstack:\n") \
									\
	_NL(RS_FBT,						"�ͷŵ���ջ:\n", \
									"Free callstack:\n") \
									\
	_NL(RS_PAUSE,					"�밴���������...", \
									"Press any key to continue...") \
									\
	_NL(RS_DETAIL,					"��ϸ����: ", \
									"Detail   : ") \
									\
	_NL(RS_DATA_UNA,				"����δ�����ַ("TADDRFMT")", /* need follow RS_CSTIP */ \
									"Access unalign address("TADDRFMT")") \
									\
	_NL(RS_DATA_EAR,				"�Ӵ���ĵ�ַ("TADDRFMT")������", /* need follow RS_CSTIP */ \
									"Read data from illegal address("TADDRFMT")") \
									\
	_NL(RS_DATA_EAW,				"д����"TSIZEFMT2"������ĵ�ַ("TADDRFMT")", /* need follow RS_CSTIP */ \
									"Write data("TSIZEFMT2") to illegal address("TADDRFMT")") \
									\
	_NL(RS_INST_EA2,				"���������ܷ�("TADDRFMT"), ������ջ֡���Ȼ�", /* need follow RS_CSTIP */ \
									"Jump to illegal address("TADDRFMT") when function return, maybe stack frame corruption") \
									\
	_NL(RS_INST_EA,					"%s()�����˴���ĺ���ָ��ʹ�����ܵ��Ƿ���ַ("TADDRFMT")ִ��", /* need follow RS_CSTIP */ \
									"%s() calling incorrect function pointer cause program execute at illegal address("TADDRFMT")") \
									\
	_NL(RS_INST_UND,				"δ����ָ��(0x%08X)", /* need follow RS_CSTIP */ \
									"Undefine Inst.(0x%08X)") \
									\
	_NL(RS_SEGV_NORMAL,				"����������������Ӳ������(���PCB�Ƿ񾭹�PDN����)��MMU��������\n", \
									"Program is normal, maybe hardware problem(check PCB has do PDN or not) or MMU protected\n") \
									\
	_NL(RS_CSTIP,					"�����ϱ������̵���ջ�����ش���\n", \
									", please check code with crashed process's callstack\n") \
									\
	_NL(RS_UNKNOWN_DWOP,			"δ֪��%s��%s��չָ��: 0x%x\n", \
									"Unrecognized %s of %s op code: 0x%x\n") \
									\
	_NL(RS_UNKNOWN_TYPE,			"δ֪��%s����: 0x%X\n", \
									"Unrecognized %s type: 0x%X\n") \
									\
	_NL(RS_MIGRATE_TYPE,			"\t��С(KB) �����ƶ� �ɻ��յ� ���ƶ��� ������   �����   �ܹ�(��)\n", \
									"\tsize(KB) unmove   reclaim  movable  reserve  isolate  total(nr)\n") \
									\
	_NL(RS_GENSCRIPT,				"����%s�ű� ...\n", \
									"Generating %s script ...\n") \
									\
	_NL(RS_GEN_NOTE,				"���ļ���"SST_NAME" v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"�Զ�����\n", \
									"Generated by "SST_NAME" v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"\n") \
									\
	_NL(RS_UNMATCH_DATA,			"���ݲ�ƥ��["TADDRFMT"-"TADDRFMT")\n", \
									"Unmatch data["TADDRFMT"-"TADDRFMT")\n") \
									\
	_NL(RS_CMP_REGION,				"��ƥ�����������: "TADDRFMT" ++ %d", \
									"Unmatched data region: "TADDRFMT" ++ %d") \
									\
	_NL(RS_MATCH_ORNOT,				"��Ϣ���㣬�޷��ж�%s�Ƿ�ƥ��\n", \
									"Message is not enough, can't check if %s is matched\n") \
									\
	_NL(RS_CORE_INF,				"δ֪��CPU��Ϣ\n", \
									"Unrecognized CPU info.\n") \
									\
	_NL(RS_READ_SYMF,				"����%s������Ϣ ...\n", \
									"Loading symbol info from '%s' ...\n") \
									\
	_NL(RS_DISAS,					"   ��Ӧ���ָ��:", \
									"   assembly inst.:") \
									\
	_NL(RS_DISAS_INFO,				"\n\t�к�  ��ַ%*cָ��\t\t\t��ʾ", \
									"\n\tline  addr%*cinst.\t\t\ttips") \
									\
	_NL(RS_EXCP_HERE,				"\t; ����ֹͣ������", \
									"\t; process stop here") \
									\
	_NL(RS_EXCP_REG,				"\n\t��ʱ�ļĴ���ֵ:\n\t", \
									"\n\tcurrent register values:\n\t") \
									\
	_NL(RS_OUTLOG,					"����%s ...\n", \
									"Output %s ...\n") \
									\
	_NL(RS_LOG_INCOMPLETE,			"%s log(%d)������\n", \
									"%s log(%d)incomplete\n") \
									\
	_NL(RS_DUMP_ELOG,				"��ȡ��־ ...\n", \
									"Dump log ...\n") \
									\
	_NL(RS_ELOG_DUMP,				"== ��־��Ϣ ==\n", \
									"== log info ==\n") \
									\
	_NL(RS_GAT_PATH,				"δ֪��GAT·��������ϵowner�޸�\n", \
									"Unrecognized GAT path, please contact owner to modify\n") \
									\
	_NL(RS_NO_PYTHONVAR,			"\techo δ����%s�����������밲װpython����\n\tpause\n\texit\n", \
									"\techo not define '%s' envionment variable, please install python and define it\n\tpause\n\texit\n") \
									\
	_NL(RS_NO_GDB,					"������gdb���밲װNDK����Ŀ¼��ӵ���������path�С�", \
									"Not exist gdb, please install NDK and add file path to $PATH envionment variable") \
									\
	_NL(RS_FILE_NOT_EXIST,			"������%s�ļ�\n", \
									"'%s' file not exist\n") \
									\
	_NL(RS_NO_T32,					"û�й���trace32, �����cmm�ļ���trace32, ��û��trac32������%s\n", \
									"cmm file is not associated with trace32, please associate or download %s if no trace32\n") \
									\
	_NL(RS_NOCOREINF,				"%sû���κ�CPU��Ϣ\n", \
									"%s has no any CPU info\n") \
									\
	_NL(RS_FILE_RDY,				"����%s���\n", \
									"Generate %s done\n") \
									\
	_NL(RS_NO_FEATURE,				"��֧�ָù���\n", \
									"Don't support this feature\n") \
									\
	_NL(RS_COMMAND,					"��t����trace32, g����gdb, �س�����������, ����������...", \
									"Press 't' start trace32, 'g' start gdb, 'enter' start command line, others to continue...") \
									\
	_NL(RS_CMDHELP,					"��ʾ: q: �˳�, m: ����maps\n", \
									"Tips: q: exit, m: generate maps\n") \
									\
	_NL(RS_ALL_THREAD,				"�����߳���Ϣ:\n", \
									"All threads info:\n") \
									\
	_NL(RS_UNKNOWN_CMD,				"δ֪������\n", \
									"Unrecognized command\n") \
									\
	_NL(RS_CS,						"   ���ص���ջ:\n", \
									"   Native callstack:\n") \
									\
	_NL(RS_MMU_ENTRY,				"MMUҳ����("TADDRFMT")�쳣, VA:"TADDRFMT"\n", \
									"MMU page table entry("TADDRFMT") abnormal, VA:"TADDRFMT"\n") \
									\
	_NL(RS_PATH_EXIST_SPACE,		"·�����ڿո�, ��ŵ������ո��·����������\n", \
									"Space is exist in the path, please run at the path which no space\n") \
									\
	_NL(RS_SYSTRACKER_W,			"APд", \
									"AP write") \
									\
	_NL(RS_SYSTRACKER_R,			"AP��", \
									"AP read") \
									\
	_NL(RS_SYST_TIMEOUT,			"��ַ��ʱ", \
									" address timeout") \
									\
	_NL(RS_SCHED2BUG_1,				"����%s��ԭ��������(�ر�����)�������, ", \
									"Process %s scheduling while atomic, ") \
									\
	_NL(RS_SCHED2BUG_2,				"��ӵ���ջ���ҹ����ϴ��벢�޸����ߴ�CONFIG_DEBUG_PREEMPTЭ������\n", \
									"please find disable preemption code from callstack and fix. or enable CONFIG_DEBUG_PREEMPT for debugging\n") \
									\
	_NL(RS_SCHED2BUG_3,				"%s()�����ر������ϣ�������벢�޸�\n", \
									"%s() function disable preemption, please check and fix.\n") \
									\
	_NL(RS_IRQ_CALL2BUG,			"��ֹ���ж������%s()����\n", \
									"calling %s() function in interrupt is forbidden\n") \
									\
	_NL(RS_SLUB2BUG,				"slub���Ȼ���bit flip������SYS_KENREL_LOG���slub��Ϣ\n", \
									"slub corruption or bit flip, please check slub log in SYS_KENREL_LOG\n") \
									\
	_NL(RS_PMIC2BUG,				"PMICͨ�ų�ʱ, ����Ӳ��\n", \
									"PMIC communication timeout, please check HW\n") \
									\
	_NL(RS_I2C2BUG,					"I2Cͨ���쳣, ����Ӳ���Ͷ�Ӧ������\n", \
									"I2C communication error, please check HW and driver\n") \
									\
	_NL(RS_SYSRQ2BUG,				"�ϲ�echo c > /proc/sysrq-trigger�����KE������Ϊ��������\n", \
									"Native layer 'echo c > /proc/sysrq-trigger' cause KE, please check why do it\n") \
									\
	_NL(RS_HAS_PROBLEM,				"����������", \
									" maybe have problem") \
									\
	_NL(RS_EXCP_GVAR,				"\t; ȫ�ֱ���", \
									"\t; global variable ") \
									\
	_NL(RS_EXCP_LVAR,				"\t; �ֲ�����", \
									"\t; local variable ") \
									\
	_NL(RS_EXCP_FUNC,				"\t; ��������ֵ", \
									"\t; function return value ") \
									\
	_NL(RS_MMU_BASE,				"�Ҳ����ں�ҳ��\n", \
									"Fail to find kernel page table\n") \
									\
	_NL(RS_DRAMC,					"�쳣, ����Ӳ��Vcore,Vm����Ҫpower�Ƿ�����쳣\n", \
									" error, check if power(e.g. Vcore,Vm) is abnormal or not\n") \
									\
	_NL(RS_LAST_BUS,				"PERISYS����0x%08X��ַ��ʱ", \
									"PERISYS access address 0x%08X timeout") \
									\
	_NL(RS_LAST_BUS2,				", �漰��ģ����:", \
									", related module:") \
									\
	_NL(RS_LAST_BUS3,				"INFRASYS���ʳ�ʱ\n", \
									"INFRASYS access timeout\n") \
									\
	_NL(RS_SPM_HW_REBOOT,			"��%s��%s�����﷢��HW reboot, ", /* ��Ҫ��RS_SPM_FW��RS_SPM_FLOW */\
									"HW reboot in %s %s flow, ") \
									\
	_NL(RS_SPM_FW,					"��Ҫ����SPM�̼�\n", \
									"need analyze SPM firmware\n") \
									\
	_NL(RS_SPM_FLOW,				"���CPU�Ƿ���\n", \
									"check if CPU hang or not\n") \
									\
	_NL(RS_HW_REBOOT,				"����HW reboot, ���CPU�Ƿ�����Ӳ������\n", \
									"HW reboot, check if CPU hang or HW issue\n") \
									\
	_NL(RS_ATF_CRASH,				"����ATF crash, �����%s\n", \
									"ATF crash, please analyze %s\n") \
									\
	_NL(RS_THERMAL_RB,				"��������, ���log������ģ���Ƿ񳬹�Ԥ���¶�\n", \
									"Thermal reboot, check each module is over temperature or not from log\n") \
									\
	_NL(RS_SSPM_IPI2BUG,			"SSPM IPI��ʱ, ��kernel log�ҳ�ipi�ź����Ӧ��RD����\n", \
									"SSPM IPI timeout, find ipi number from kernel log and check with corresponding RD from table\n") \
									\
	_NL(RS_RELATED_CR,				"== ������Ϣ ==\n", \
									"== Related Info ==\n") \
									\
	_NL(RS_OUT_RELATED_INF,			"��ȡ������Ϣ ...\n", \
									"Output related info ...\n") \
									\
	_NL(RS_EIGEN_INF0,				"CrId", \
									"CrId") \
									\
	_NL(RS_EIGEN_INF1,				"��������", \
									"BuildType") \
									\
	_NL(RS_EIGEN_INF2,				"PatchId", \
									"PatchId") \
									\
	_NL(RS_EIGEN_INF3,				"ƽ̨", \
									"Platfrom") \
									\
	_NL(RS_EIGEN_INF4,				"�汾", \
									"Branch") \
									\
	_NL(RS_EIGEN_INF5,				"�ͻ�", \
									"Customer") \
									\
	_NL(RS_EIGEN_INF6,				"����", \
									"AssignDept") \
									\
	_NL(RS_EIGEN_INF7,				"Assignee", \
									"Assignee") \
									\
	_NL(RS_UNKNOWN_HDTO,			"δ֪��hang detectֵ(%d)\n", \
									"Unrecognized hang detect value(%d)\n") \
									\



typedef enum _RS_ID
{
#define _NL(Enum, str1, str2) Enum,
	RS_MAP
#undef _NL
} RS_ID;

typedef const struct _RS_UNIT
{
	const char *pStr;
} RS_UNIT;

/*************************************************************************
* DESCRIPTION
*	��Դ����
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern RS_UNIT *pRsAry;

/*************************************************************************
* DESCRIPTION
*	������Դ������ȡ�ַ���
*
* PARAMETERS
*	Id: ��Դ����
*
* RETURNS
*	�ַ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define GetCStr(Id) pRsAry[Id].pStr

/*************************************************************************
* DESCRIPTION
*	�����Ϣ���ļ�
*
* PARAMETERS
*	fp: �ļ����
*	Id: ��Դ����
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define LOG(fp, Id, ...) fprintf(fp, GetCStr(Id), ##__VA_ARGS__)

/*************************************************************************
* DESCRIPTION
*	��������ַ������ļ�
*
* PARAMETERS
*	fp: �ļ����
*	ConstStr: �����ַ���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define LOGC(fp, ConstStr, ...) fprintf(fp, ConstStr, ##__VA_ARGS__)

/*************************************************************************
* DESCRIPTION
*	����쳣��Ϣ
*
* PARAMETERS
*	Id: ��Դ����
*	arg: �������
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern int SstErr[16], SstErrIdx;
#define __SSTERR(Id)	SstErr[SstErrIdx++&(sizeof(SstErr) / sizeof(SstErr[0]) - 1)] = Id /* ע��: ֻ����DEBUG�汾ֱ��ʹ��!!! */
#define SSTERR(Id, ...) do { __SSTERR(Id); LOG(stderr, RS_ERROR); LOG(stderr, Id, ##__VA_ARGS__); } while (0)

/*************************************************************************
* DESCRIPTION
*	debug�汾����쳣��Ϣ
*
* PARAMETERS
*	ConstStr: �����ַ���
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
#ifdef _DEBUG
#define DBGERR(ConstStr, ...)   do { LOG(stderr, RS_ERROR); LOGC(stderr, ConstStr, ##__VA_ARGS__); } while (0)
#else
#define DBGERR(ConstStr, ...)   do { } while (0)
#endif

/*************************************************************************
* DESCRIPTION
*	���������Ϣ
*
* PARAMETERS
*	Id: ��Դ����
*	arg: �������
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define SSTWARN(Id, ...) do { LOG(stderr, RS_WARNING); LOG(stderr, Id, ##__VA_ARGS__); } while (0)

/*************************************************************************
* DESCRIPTION
*	�����Ϣ
*
* PARAMETERS
*	Id: ��Դ����
*	arg: �������
*
* RETURNS
*	��
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define SSTINFO(Id, ...) LOG(stdout, Id, ##__VA_ARGS__)


typedef struct _SST_OPT
{
	const char *pInstallDir; /* ��װĿ¼, ΪNULL��ر�������Ŀ¼�Ĺ���(sc, ref, fb) */

#define DEF_SYMDIR  "symbols"
	const char *pSymDir;
	const char *pCrId; /* ΪNULL����ӵ�logeigenϵͳ */
	unsigned SymDirLen;

#define SST_HPAUSE_BIT		(1 << 0) /* ������ɺ�����ͣ */
#define SST_HVIEW_BIT		(1 << 1) /* ������ɺ��������±���ʾ���� */
#define SST_LAUNCH_MASK		(3 << 2) /* ������ʽ */
#define SST_DIRECT_LAUNCH   (0 << 2) /* ֱ������ */
#define SST_RIGHT_LAUNCH	(1 << 2) /* �Ҽ����� */
#define SST_QAAT_LAUNCH		(2 << 2) /* qaat���� */
#define SST_GENCMM_BIT		(1 << 4) /* �Ƿ�����cmm�ű� */
#define SST_GENGDB_BIT		(1 << 5) /* �Ƿ�����gdb�ű� */
#define SST_SCRLOADALL_BIT	(1 << 6) /* �ű��������п��ܵĿ� */
#define SST_ALLTHREADS_BIT	(1 << 7) /* �Ƿ���ʾ���н��� */
#define SST_LANG_MASK		(3 << 8) /* ����ģʽ */
#define SST_CHS_LANG		(0 << 8) /* ����ģʽ */
#define SST_ENG_LANG		(1 << 8) /* Ӣ��ģʽ */
#define SST_TW_LANG			(2 << 8) /* ����ģʽ */
#define SST_REF_BIT			(1 << 10) /* �ο�������Ϣ */
#define SST_FB_BIT			(1 << 11) /* ������Ϣ */
#define SST_SC_BIT			(1 << 12) /* symcache���� */
#define SST_LOGEIGEN_BIT	(1 << 13) /* logeigen���� */
	unsigned Flag;
} SST_OPT;

/*************************************************************************
* DESCRIPTION
*	���ܿ���
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define IS_GENCMM()			(SstOpt.Flag&SST_GENCMM_BIT)
#define IS_GENGDB()			(SstOpt.Flag&SST_GENGDB_BIT)
#define IS_SCRLOADALL()		(SstOpt.Flag&SST_SCRLOADALL_BIT)
#define IS_ALLTHREADS()		(SstOpt.Flag&SST_ALLTHREADS_BIT)
#define IS_ENGLISH()		((SstOpt.Flag&SST_LANG_MASK) == SST_ENG_LANG)
#define IS_TRADITIONAL()	((SstOpt.Flag&SST_LANG_MASK) == SST_TW_LANG)
#define IS_REFERENCE()		(SstOpt.Flag&SST_REF_BIT)
#define IS_FEEDBACK()		(SstOpt.Flag&SST_FB_BIT)
#define IS_SYMCACHE()		(SstOpt.Flag&SST_SC_BIT)
#define IS_LOGEIGEN()		(SstOpt.Flag&SST_LOGEIGEN_BIT)

/*************************************************************************
* DESCRIPTION
*	���ýṹ��
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern SST_OPT SstOpt;

#endif
