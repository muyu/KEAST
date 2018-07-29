/*********************************************************************************
* Copyright(C),2013-2017,MediaTek Inc.
* FileName:	string_res.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2013/7/25
* Description: 字符串资源
**********************************************************************************/
#ifndef _STRING_RES_H
#define _STRING_RES_H

#include <stdio.h>
#include <ttypes.h>

#define INTERNAL_BUILD
#define RS_MAP \
	_NL(RS_USAGE,					"用法: %s [ -s|-lang|-t|-cmm|-gdb|-l|-h|<dir> ]\n" \
									"	-s 目录: 指定符号文件目录, 默认为当前目录" \
									"	-lang l: l为'eng'时切换为英文, l为'tw'时切换为繁体, 默认为简体\n" \
									"	-t:      显示所有进程信息\n" \
									"	-cmm:    生成cmm脚本\n" \
									"	-gdb:    生成gdb脚本\n" \
									"	-l 目录: 指定安装目录, 用于查找可用脚本\n" \
									"	-h:      显示该帮助\n" \
									"	<dir>:   待分析的目录, 默认为当前目录\n", \
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
	_NL(RS_ERROR,					"错误: ", \
									"Error: ") \
									\
	_NL(RS_WARNING,					"警告: ", \
									"Warning: ") \
									\
	_NL(RS_AVER,					"版本    : %s", \
									"Version  : %s") \
									\
	_NL(RS_EXP_TIME,				"异常时间: %d.%06d秒", \
									"Exp time : %d.%06ds") \
									\
	_NL(RS_PLATFORM,				"平台    : MT%d\n", \
									"Platform : MT%d\n") \
									\
	_NL(RS_UNKNOWN_ARG,				"未知的参数: %s\n", \
									"Unrecognized argument: %s\n") \
									\
	_NL(RS_EDIR,					"错误的目录: %s\n", \
									"Incorrect directory: %s\n") \
									\
	_NL(RS_NEED_ARG,				"选项%s需要%d个参数\n", \
									"Option %s requires %d argument(s)\n") \
									\
	_NL(RS_LOADING,					"加载%s ...\n", \
									"Loading %s ...\n") \
									\
	_NL(RS_FILE_MAP,				"%s映射%s失败\n", \
									"Fail to map %s's %s\n") \
									\
	_NL(RS_FILE_UNMAP,				"撤销%s映射失败\n", \
									"Fail to unmap %s\n") \
									\
	_NL(RS_FILE_WRITE,				"写入%s失败\n", \
									"Fail to write %s\n") \
									\
	_NL(RS_FILE_DAMAGED,			"文件%s损坏\n", \
									"File %s damaged\n") \
									\
	_NL(RS_NO_DBGINF,				"%s没有调试信息\n", \
									"No symbols in %s\n") \
									\
	_NL(RS_ELF_HEADER,				"%s非ELF文件, 头部信息%c%c%c%c\n", \
									"'%s' isn't ELF file, magic header: %c%c%c%c\n") \
									\
	_NL(RS_ELF_TYPE,				"%s不是%s ELF文件\n", \
									"'%s' isn't %s ELF file\n") \
									\
	_NL(RS_ELF_FORMAT,				"不支持%s类型的ELF文件(%s)\n", \
									"Don't support %s format of ELF file(%s)\n") \
									\
	_NL(RS_NO_LOAD,					"找不到%s的%s内存段\n", \
									"Fail to find '%s' of %s memory segment\n") \
									\
	_NL(RS_NO_SEG,					"找不到%s的%s段\n", \
									"Fail to find <%s> %s segment\n") \
									\
	_NL(RS_NO_SEC,					"找不到%s的%s节\n", \
									"Fail to find <%s> %s section\n") \
									\
	_NL(RS_MMUTBL,					"%s页表损坏\n", \
									"Damaged %s page table\n") \
									\
	_NL(RS_CTRLTBL,					"未知的ctrl block标识: %s\n", \
									"Unrecognized ctrl block magic: %s\n") \
									\
	_NL(RS_EADDR,					"错误的%s的%s的地址: "TADDRFMT"\n", \
									"%s of %s Incorrect address: "TADDRFMT"\n") \
									\
	_NL(RS_VM_OVERFLOW,				"数据段溢出\n", \
									"Region overflow\n") \
									\
	_NL(RS_CORE_FORMAT,				"不支持这种%s文件\n", \
									"Don't support this %s format\n") \
									\
	_NL(RS_NOCORE,					"找不到异常重启信息或不存在mrdump文件\n", \
									"No exception found or no mrdump file exist\n") \
									\
	_NL(RS_SYM_UNMATCH,				"%s和mrdump不匹配, 请在测试前保留symbols目录\n", \
									"%s and mrdump are unmatched, please leave symbols folder before test\n") \
									\
	_NL(RS_VM_MR_UNMATCHED1,		"其中%d个符号匹配失败\n", \
									"%d symbols are unmatched\n") \
									\
	_NL(RS_DEMAGED1,				"mrdump存在区域被踩坏或bit反转\n", \
									"Mrdump is demaged or bit reversed\n") \
									\
	_NL(RS_DEMAGED_REG,				"第%d个区域, "TADDRFMT, \
									"Region %d, "TADDRFMT) \
									\
	_NL(RS_OOM,						"申请不到内存\n", \
									"Out of memory\n") \
									\
	_NL(RS_INCOMPLETE_NOTE,			"NOTE段缺少%s信息\n", \
									"No %s in NOTE segment\n") \
									\
	_NL(RS_PT_NOTE_SIZE,			"mrdump里的PT_NOTE size不对, 多出%dB\n", \
									"Incorrect PT_NOTE size in mrdump, larger %dB\n") \
									\
	_NL(RS_ANS_DONE,				"分析完毕\n", \
									"Analysis is completed\n") \
									\
	_NL(RS_REPORT,					"分析报告.txt", \
									"Analysis report.txt") \
									\
	_NL(RS_CREATE,					"创建%s失败\n", \
									"Fail to create %s\n") \
									\
	_NL(RS_SEG_SIZE,				"%s的%s段大小(%dB)错误\n", \
									"%s of %s segment size(%dB) is incorrect\n") \
									\
	_NL(RS_SEG_VERSION,				"不支持%s的%s的版本0x%X\n", \
									"Don't support %s of %s version 0x%X\n") \
									\
	_NL(RS_GNU_AUG,					"不支持%s的%s的参数%c\n", \
									"Don't support %s of %s augmentation %c\n") \
									\
	_NL(RS_DWFOP_UNDERFLOW,			"%s的%s寄存器栈下溢\n", \
									"%s of %s register stack underflow\n") \
									\
	_NL(RS_INVALID_DWFSP_RULE,		"无效的%s的%s的SP规则\n", \
									"Invalid %s of %s's SP rule\n") \
									\
	_NL(RS_DWEXPROP_OVERFLOW,		"%s的%s表达式栈溢出\n", \
									"%s of %s expression stack overflow\n") \
									\
	_NL(RS_DWEXPROP_UNDERFLOW,		"%s的%s表达式栈下溢\n", \
									"%s of %s expression stack underflow\n") \
									\
	_NL(RS_INVALID_IDX,				"无效的%s的%s%s索引: %d\n", \
									"Invalid %s of %s %s index: %d\n") \
									\
	_NL(RS_REGISTER,				"寄存器", \
									"register") \
									\
	_NL(RS_STACK,					"栈", \
									"stack") \
									\
	_NL(RS_SIZE,					"尺寸", \
									"size") \
									\
	_NL(RS_INIT_MODULE,				"初始化模块 ...\n", \
									"Init module ...\n") \
									\
	_NL(RS_COLLECT_EX,				"获取异常信息 ...\n", \
									"Collect exception info ...\n") \
									\
	_NL(RS_EX_HEAD,					"== 异常报告v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"(仅供参考) ==\n", \
									"== Exception report v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"(For Reference) ==\n") \
									\
	_NL(RS_REPORT_REF,				"报告解读:", \
									"Manual   :") \
									\
	_NL(RS_REFERENCE,				"参考信息:", \
									"Reference:") \
									\
	_NL(RS_DIE,						"发生Oops", /* need follow RS_CSTIP */ \
									"Oops happened") \
									\
	_NL(RS_BUG_0,					"断言失败{主动调用BUG()/BUG_ON()}", /* need follow RS_CSTIP */ \
									"Assert fail{call BUG()/BUG_ON()}") \
									\
	_NL(RS_PANIC_0,					"Kernel panic{主动调用panic()}", /* need follow RS_CSTIP */ \
									"Kernel panic{call panic()}") \
									\
	_NL(RS_OOM2PANIC,				"内存耗尽且无内存可释放, 请根据SYS_KERNEL_LOG里的内存信息检查进程/内核是否存在内存泄漏\n", \
									"Out of memory and no killable processes. Plase check SYS_KERNEL_LOG mem info to see if process/kernel has mem leak or not\n") \
									\
	_NL(RS_INIT_NE2PANIC,			"init进程NE, 需要分析NE(coredump在db里的zcore-1.zip, 请用logviewer解开)\n", \
									"init process NE, need analyze NE(zcore-1.zip in db is NE db which can be opened by logviewer)\n") \
									\
	_NL(RS_INIT_KILL2PANIC,			"init进程被杀, 需要分析被杀原因\n", \
									"init process is killed by other process, need find murderer\n") \
									\
	_NL(RS_HWT_0,					"看门狗复位, 其中CPU", \
									"Watch dog timeout while CPU") \
									\
	_NL(RS_HWT_1,					"没有喂狗", \
									"don't kick") \
									\
	_NL(RS_HWT_2,					", 超时时间 < 30s，请检查硬件时钟是否正常\n", \
									", overtime < 30s, please check HW clock work normal or not\n") \
									\
	_NL(RS_HWT_3,					", 30s超时, 请参考[FAQ09189]\n", \
									", timeout 30s, please refer [FAQ09189]\n") \
									\
	_NL(RS_HWT_4,					", 请检查对应调用栈是否存在死锁或长时间关中断\n", \
									", please check the callstacks have dead lock or long time disable interrupt\n") \
									\
	_NL(RS_SWT2HWT,					"SWT过程卡死引起看门狗复位，需要检查分析SWT问题\n", \
									"SWT hang up cause watch dog timeout, need check SWT problem\n") \
									\
	_NL(RS_HDT2HWT1,				"%s检查到watchdog无法感知的卡死, 检查kernel是否卡死或ART死锁, 具体请看%s\n", \
									"'%s' detect hang while watchdog not detected, check kernel hang or ART deadlock, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT2,				"%s检查到导出调用栈时卡死, 检查debuggerd状态, 具体请看%s\n", \
									"'%s' detect hang when dump callstack, check debuggerd status, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT3,				"%s检查到SWT时卡死, 直接分析SWT db, 具体请看%s\n", \
									"'%s' detect hang while SWT, check SWT db, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT4,				"%s检查到SS/SF NE抓db时卡死, 具体请看%s\n", \
									"'%s' detect hang when SS/SF NE, refer %s for detail\n") \
									\
	_NL(RS_HDT2HWT5,				"%s检查到SS/SF重启时卡死, 具体请看%s\n", \
									"'%s' detect hang when SS/SF reboot, refer %s for detail\n") \
									\
	_NL(RS_IRQ2HWT,					"中断%s频繁触发引起看门狗复位，需要检查IRQ所属驱动\n", \
									"IRQ(%s) trigger frequently cause watch dog timeout, need check the corresponding IRQ's driver\n") \
									\
	_NL(RS_PWR_HWT,					"休眠/唤醒卡死触发看门狗复位，需要检查分析卡死问题\n", \
									"system hang up cause watch dog timeout when resume/suspend, need check hang problem\n") \
									\
	_NL(RS_CPUUP,					"启动", \
									"power up") \
									\
	_NL(RS_CPUDOWN,					"关闭", \
									"shut down") \
									\
	_NL(RS_HOTPLUG2HWT0,			"%sCPU%d时卡住, 需要检查hps_main调用栈卡在哪里\n", \
									"blocked when %s CPU%d, need check hps_main's callstack for hang issue\n") \
									\
	_NL(RS_HOTPLUG2HWT1,			"曾经启动失败, 搜索kernel log查找failed to come online关键字并分析原因\n", \
									" failed to come online before, search the keyword on kernel log analyze why\n") \
									\
	_NL(RS_THERMAL_0,				"过热重启，请找thermal owner分析\n", \
									"Thermal reboot, please check with thermal owner\n") \
									\
	_NL(RS_THERMAL_1,				"%s过热重启, 请找thermal owner分析\n", \
									"%s thermal reboot, please check with thermal owner\n") \
									\
	_NL(RS_DPM_WD_REBOOT,			"有模块在唤醒/休眠过程超时，请检查kernel log查看oops前调用栈信息\n", \
									"There has module time out in suspend/resume flow, check kernel log for backtrace to debug\n") \
									\
	_NL(RS_COMMAND_LINE,			"命令行  : %s\n", \
									"Command line : %s\n") \
									\
	_NL(RS_OUT_CPUS,				"获取CPU信息 ...\n", \
									"Output cpu info ...\n") \
									\
	_NL(RS_CPUS_INFO,				"== CPU信息 ==\n", \
									"== CPUs info ==\n") \
									\
	_NL(RS_CRASH_CPU,				"崩溃CPU信息:\n", \
									"Crashed CPU info:\n") \
									\
	_NL(RS_NOT_KICK,				"无喂狗CPU信息:\n", \
									"Not kick cpu info:\n") \
									\
	_NL(RS_OTHER_CPU,				"其他CPU信息:\n", \
									"Others CPU info:\n") \
									\
	_NL(RS_EXEC_T,					", 已执行%d秒", \
									", executed %ds") \
									\
	_NL(RS_CPU_OFF,					"关闭\n\n", \
									"offline\n\n") \
									\
	_NL(RS_CPU_DIS_INT,				", 中断: 关", \
									", Interrupt: disable") \
									\
	_NL(RS_CPU_IDLE,				"空闲\n", \
									"idle\n") \
									\
	_NL(RS_REG_EXP,					"   寄存器异常: ", \
									"   registers exception: ") \
									\
	_NL(RS_STK_OVERFLOW,			"内核栈溢出\n", \
									"Kernel stack overflow\n") \
									\
	_NL(RS_STK_FRAME_OF,			"%s()函数存在栈帧越界问题, 请检查局部定义的变量/数组是否存在越界行为\n", \
									"%s() function stack frame corruption! Please check all local variable/array has overflow or not.\n") \
									\
	_NL(RS_SP_INCORRECT,			"SP("TADDRFMT")异常\n", \
									"SP("TADDRFMT") is abnormal\n") \
									\
	_NL(RS_CS_END,					"\t== 栈结束 ==\n", \
									"\t== end of frame ==\n") \
									\
	_NL(RS_PARA,					"参数", \
									"Para") \
									\
	/* ============================== malloc_tracker.c ============================== */ \
	_NL(RS_VAR_VAL,					"无法获取变量%s的值\n", \
									"Fail to get %s variable value\n") \
									\
	_NL(RS_NO_VAR,					"找不到%s变量\n", \
									"Fail to find %s variable\n") \
									\
	_NL(RS_NO_FUN,					"找不到%s函数\n", \
									"Fail to find %s function\n") \
									\
	_NL(RS_NO_KSYM,					"找不到%s符号信息\n", \
									"Fail to find %s symbols info\n") \
									\
	_NL(RS_UN_PATTERN,				"未知的%s识别码(0x%08X)，请联系owner添加\n", \
									"Unrecognized %s pattern(0x%08X), please contact owner to add\n") \
									\
	_NL(RS_CORRUPTED,				"%s变量错误，可能被踩坏\n", \
									"%s is incorrect, maybe is corrupted\n") \
									\
	_NL(RS_LK_CORRUPTED,			"%s链表错误，可能被踩坏\n", \
									"%s list is incorrect, maybe is corrupted\n") \
									\
	_NL(RS_UNKNOWN_CODE,			"未知的%s错误码，请联系owner添加\n", \
									"Unrecognized %s error code, please contact owner to add\n") \
									\
	_NL(RS_GET_ADDR,				"获取%s的地址失败\n", \
									"Fail to get address of %s\n") \
									\
	_NL(RS_ADDR,					"%s的地址错误\n", \
									"Address of %s is incorrect\n") \
									\
	_NL(RS_SOBJ_E_REDZ,				" 红色区", \
									" red zone") \
									\
	_NL(RS_SOBJ_E_PAD,				" 对齐填充区", \
									" pading zone") \
									\
	_NL(RS_SOBJ_E_FP,				" 空闲链表指针", \
									" freepointer") \
									\
	_NL(RS_SOBJ_CUR,				"被踩坏，可能是当前slub obj越界访问了\n", \
									" is corrupted, maybe current slub obj overflow, please check it\n") \
									\
	_NL(RS_SOBJ_UAF,				"该块slub obj释放后又使用了\n", \
									"This slub obj used after Free\n") \
									\
	_NL(RS_BUDDY_CHK,				"== 伙伴系统 ==\n", \
									"== buddy system ==\n") \
									\
	_NL(RS_SLUB_CHK,				"== slub检查 ==\n", \
									"== slub check ==\n") \
									\
	_NL(RS_CHK_BUDDY,				"检查伙伴系统 ...\n", \
									"Checking buddy ...\n") \
									\
	_NL(RS_CHK_SLUB,				"检查slub ...\n", \
									"Checking slub ...\n") \
									\
	_NL(RS_OK,						"正常\n", \
									"Healthy\n") \
									\
	_NL(RS_VAR_CHANGE,				"%s变量结构有变化，请联系owner添加\n", \
									"%s variable structure has changed, please contact owner to add\n") \
									\
	_NL(RS_BUDDY_INFO,				"总共: %d MB, 可管理: %d MB, 空闲: %d KB, 起始: 0x%08X\n", \
									"Total: %d MB, managed: %d MB, Free: %d KB, start address: 0x%08X\n") \
									\
	_NL(RS_BUDDY_ZONE_INFO,			"%s: 总共: %d MB, 可管理: %d MB, 空闲: %d KB, 起始: 0x%08X\n", \
									"%s total: %d MB, managed: %d MB, Free: %d KB, start address: 0x%08X\n") \
									\
	/* ============================== callstack.c ============================== */ \
	_NL(RS_NEED_VMFILE,				"如果是KE的问题，麻烦贵司以后提供db的同时提供对应的vmlinux，我司即可迅速分析，减少不必要的延误。\n这题还需%s文件, 该文件必须和db匹配, 对应时间戳是:\n%s\n详情请查看MOL上的'[FAQ06985]KE发生后如何判断vmlinux和log是否匹配'\n", \
									"If KE(kernel exception) problems, trouble later to provide db while providing corresponding vmlinux, so we can quickly analyze and reduce unnecessary delays.\nNeed %s. The file should be compiled at the same time, The timestamp is:\n%s\nPlease check '[FAQ06985]How to check if vmlinux matches log after KE takes place?' on MOL\n") \
									\
	_NL(RS_MBT,						"分配调用栈:\n", \
									"Malloc callstack:\n") \
									\
	_NL(RS_FBT,						"释放调用栈:\n", \
									"Free callstack:\n") \
									\
	_NL(RS_PAUSE,					"请按任意键继续...", \
									"Press any key to continue...") \
									\
	_NL(RS_DETAIL,					"详细描述: ", \
									"Detail   : ") \
									\
	_NL(RS_DATA_UNA,				"访问未对齐地址("TADDRFMT")", /* need follow RS_CSTIP */ \
									"Access unalign address("TADDRFMT")") \
									\
	_NL(RS_DATA_EAR,				"从错误的地址("TADDRFMT")读数据", /* need follow RS_CSTIP */ \
									"Read data from illegal address("TADDRFMT")") \
									\
	_NL(RS_DATA_EAW,				"写数据"TSIZEFMT2"到错误的地址("TADDRFMT")", /* need follow RS_CSTIP */ \
									"Write data("TSIZEFMT2") to illegal address("TADDRFMT")") \
									\
	_NL(RS_INST_EA2,				"函数返回跑飞("TADDRFMT"), 可能是栈帧被踩坏", /* need follow RS_CSTIP */ \
									"Jump to illegal address("TADDRFMT") when function return, maybe stack frame corruption") \
									\
	_NL(RS_INST_EA,					"%s()调用了错误的函数指针使程序跑到非法地址("TADDRFMT")执行", /* need follow RS_CSTIP */ \
									"%s() calling incorrect function pointer cause program execute at illegal address("TADDRFMT")") \
									\
	_NL(RS_INST_UND,				"未定义指令(0x%08X)", /* need follow RS_CSTIP */ \
									"Undefine Inst.(0x%08X)") \
									\
	_NL(RS_SEGV_NORMAL,				"程序正常，可能是硬件问题(检查PCB是否经过PDN仿真)或MMU保护引起\n", \
									"Program is normal, maybe hardware problem(check PCB has do PDN or not) or MMU protected\n") \
									\
	_NL(RS_CSTIP,					"，请结合崩溃进程调用栈检查相关代码\n", \
									", please check code with crashed process's callstack\n") \
									\
	_NL(RS_UNKNOWN_DWOP,			"未知的%s的%s扩展指令: 0x%x\n", \
									"Unrecognized %s of %s op code: 0x%x\n") \
									\
	_NL(RS_UNKNOWN_TYPE,			"未知的%s类型: 0x%X\n", \
									"Unrecognized %s type: 0x%X\n") \
									\
	_NL(RS_MIGRATE_TYPE,			"\t大小(KB) 不可移动 可回收的 可移动的 保留的   隔离的   总共(块)\n", \
									"\tsize(KB) unmove   reclaim  movable  reserve  isolate  total(nr)\n") \
									\
	_NL(RS_GENSCRIPT,				"生成%s脚本 ...\n", \
									"Generating %s script ...\n") \
									\
	_NL(RS_GEN_NOTE,				"该文件由"SST_NAME" v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"自动生成\n", \
									"Generated by "SST_NAME" v"MACRO2STR(SST_VERSION)"."MACRO2STR(SST_VERSION_SUB)"\n") \
									\
	_NL(RS_UNMATCH_DATA,			"数据不匹配["TADDRFMT"-"TADDRFMT")\n", \
									"Unmatch data["TADDRFMT"-"TADDRFMT")\n") \
									\
	_NL(RS_CMP_REGION,				"不匹配的数据区域: "TADDRFMT" ++ %d", \
									"Unmatched data region: "TADDRFMT" ++ %d") \
									\
	_NL(RS_MATCH_ORNOT,				"信息不足，无法判断%s是否匹配\n", \
									"Message is not enough, can't check if %s is matched\n") \
									\
	_NL(RS_CORE_INF,				"未知的CPU信息\n", \
									"Unrecognized CPU info.\n") \
									\
	_NL(RS_READ_SYMF,				"加载%s符号信息 ...\n", \
									"Loading symbol info from '%s' ...\n") \
									\
	_NL(RS_DISAS,					"   对应汇编指令:", \
									"   assembly inst.:") \
									\
	_NL(RS_DISAS_INFO,				"\n\t行号  地址%*c指令\t\t\t提示", \
									"\n\tline  addr%*cinst.\t\t\ttips") \
									\
	_NL(RS_EXCP_HERE,				"\t; 进程停止在这里", \
									"\t; process stop here") \
									\
	_NL(RS_EXCP_REG,				"\n\t当时的寄存器值:\n\t", \
									"\n\tcurrent register values:\n\t") \
									\
	_NL(RS_OUTLOG,					"导出%s ...\n", \
									"Output %s ...\n") \
									\
	_NL(RS_LOG_INCOMPLETE,			"%s log(%d)不完整\n", \
									"%s log(%d)incomplete\n") \
									\
	_NL(RS_DUMP_ELOG,				"提取日志 ...\n", \
									"Dump log ...\n") \
									\
	_NL(RS_ELOG_DUMP,				"== 日志信息 ==\n", \
									"== log info ==\n") \
									\
	_NL(RS_GAT_PATH,				"未知的GAT路径，请联系owner修改\n", \
									"Unrecognized GAT path, please contact owner to modify\n") \
									\
	_NL(RS_NO_PYTHONVAR,			"\techo 未定义%s环境变量，请安装python后定义\n\tpause\n\texit\n", \
									"\techo not define '%s' envionment variable, please install python and define it\n\tpause\n\texit\n") \
									\
	_NL(RS_NO_GDB,					"不存在gdb，请安装NDK后将其目录添加到环境变量path中。", \
									"Not exist gdb, please install NDK and add file path to $PATH envionment variable") \
									\
	_NL(RS_FILE_NOT_EXIST,			"不存在%s文件\n", \
									"'%s' file not exist\n") \
									\
	_NL(RS_NO_T32,					"没有关联trace32, 请关联cmm文件到trace32, 如没有trac32则下载%s\n", \
									"cmm file is not associated with trace32, please associate or download %s if no trace32\n") \
									\
	_NL(RS_NOCOREINF,				"%s没有任何CPU信息\n", \
									"%s has no any CPU info\n") \
									\
	_NL(RS_FILE_RDY,				"生成%s完成\n", \
									"Generate %s done\n") \
									\
	_NL(RS_NO_FEATURE,				"不支持该功能\n", \
									"Don't support this feature\n") \
									\
	_NL(RS_COMMAND,					"按t启动trace32, g启动gdb, 回车进入命令行, 其他键继续...", \
									"Press 't' start trace32, 'g' start gdb, 'enter' start command line, others to continue...") \
									\
	_NL(RS_CMDHELP,					"提示: q: 退出, m: 生成maps\n", \
									"Tips: q: exit, m: generate maps\n") \
									\
	_NL(RS_ALL_THREAD,				"所有线程信息:\n", \
									"All threads info:\n") \
									\
	_NL(RS_UNKNOWN_CMD,				"未知的命令\n", \
									"Unrecognized command\n") \
									\
	_NL(RS_CS,						"   本地调用栈:\n", \
									"   Native callstack:\n") \
									\
	_NL(RS_MMU_ENTRY,				"MMU页表项("TADDRFMT")异常, VA:"TADDRFMT"\n", \
									"MMU page table entry("TADDRFMT") abnormal, VA:"TADDRFMT"\n") \
									\
	_NL(RS_PATH_EXIST_SPACE,		"路径存在空格, 请放到不带空格的路径下在运行\n", \
									"Space is exist in the path, please run at the path which no space\n") \
									\
	_NL(RS_SYSTRACKER_W,			"AP写", \
									"AP write") \
									\
	_NL(RS_SYSTRACKER_R,			"AP读", \
									"AP read") \
									\
	_NL(RS_SYST_TIMEOUT,			"地址超时", \
									" address timeout") \
									\
	_NL(RS_SCHED2BUG_1,				"进程%s在原子上下文(关闭抢断)发起调度, ", \
									"Process %s scheduling while atomic, ") \
									\
	_NL(RS_SCHED2BUG_2,				"请从调用栈查找关抢断代码并修复或者打开CONFIG_DEBUG_PREEMPT协助调试\n", \
									"please find disable preemption code from callstack and fix. or enable CONFIG_DEBUG_PREEMPT for debugging\n") \
									\
	_NL(RS_SCHED2BUG_3,				"%s()函数关闭了抢断，请检查代码并修复\n", \
									"%s() function disable preemption, please check and fix.\n") \
									\
	_NL(RS_IRQ_CALL2BUG,			"禁止在中断里调用%s()函数\n", \
									"calling %s() function in interrupt is forbidden\n") \
									\
	_NL(RS_SLUB2BUG,				"slub被踩坏或bit flip，请检查SYS_KENREL_LOG里的slub信息\n", \
									"slub corruption or bit flip, please check slub log in SYS_KENREL_LOG\n") \
									\
	_NL(RS_PMIC2BUG,				"PMIC通信超时, 请检查硬件\n", \
									"PMIC communication timeout, please check HW\n") \
									\
	_NL(RS_I2C2BUG,					"I2C通信异常, 请检查硬件和对应的驱动\n", \
									"I2C communication error, please check HW and driver\n") \
									\
	_NL(RS_SYSRQ2BUG,				"上层echo c > /proc/sysrq-trigger引起的KE，请检查为何这样做\n", \
									"Native layer 'echo c > /proc/sysrq-trigger' cause KE, please check why do it\n") \
									\
	_NL(RS_HAS_PROBLEM,				"可能有问题", \
									" maybe have problem") \
									\
	_NL(RS_EXCP_GVAR,				"\t; 全局变量", \
									"\t; global variable ") \
									\
	_NL(RS_EXCP_LVAR,				"\t; 局部变量", \
									"\t; local variable ") \
									\
	_NL(RS_EXCP_FUNC,				"\t; 函数返回值", \
									"\t; function return value ") \
									\
	_NL(RS_MMU_BASE,				"找不到内核页表\n", \
									"Fail to find kernel page table\n") \
									\
	_NL(RS_DRAMC,					"异常, 请检查硬件Vcore,Vm等重要power是否存在异常\n", \
									" error, check if power(e.g. Vcore,Vm) is abnormal or not\n") \
									\
	_NL(RS_LAST_BUS,				"PERISYS访问0x%08X地址超时", \
									"PERISYS access address 0x%08X timeout") \
									\
	_NL(RS_LAST_BUS2,				", 涉及的模块有:", \
									", related module:") \
									\
	_NL(RS_LAST_BUS3,				"INFRASYS访问超时\n", \
									"INFRASYS access timeout\n") \
									\
	_NL(RS_SPM_HW_REBOOT,			"在%s的%s流程里发生HW reboot, ", /* 需要接RS_SPM_FW或RS_SPM_FLOW */\
									"HW reboot in %s %s flow, ") \
									\
	_NL(RS_SPM_FW,					"需要分析SPM固件\n", \
									"need analyze SPM firmware\n") \
									\
	_NL(RS_SPM_FLOW,				"检查CPU是否卡死\n", \
									"check if CPU hang or not\n") \
									\
	_NL(RS_HW_REBOOT,				"发生HW reboot, 检查CPU是否卡死或硬件故障\n", \
									"HW reboot, check if CPU hang or HW issue\n") \
									\
	_NL(RS_ATF_CRASH,				"发生ATF crash, 请分析%s\n", \
									"ATF crash, please analyze %s\n") \
									\
	_NL(RS_THERMAL_RB,				"高温重启, 检查log看各个模块是否超过预定温度\n", \
									"Thermal reboot, check each module is over temperature or not from log\n") \
									\
	_NL(RS_SSPM_IPI2BUG,			"SSPM IPI超时, 从kernel log找出ipi号后请对应的RD分析\n", \
									"SSPM IPI timeout, find ipi number from kernel log and check with corresponding RD from table\n") \
									\
	_NL(RS_RELATED_CR,				"== 关联信息 ==\n", \
									"== Related Info ==\n") \
									\
	_NL(RS_OUT_RELATED_INF,			"获取关联信息 ...\n", \
									"Output related info ...\n") \
									\
	_NL(RS_EIGEN_INF0,				"CrId", \
									"CrId") \
									\
	_NL(RS_EIGEN_INF1,				"编译类型", \
									"BuildType") \
									\
	_NL(RS_EIGEN_INF2,				"PatchId", \
									"PatchId") \
									\
	_NL(RS_EIGEN_INF3,				"平台", \
									"Platfrom") \
									\
	_NL(RS_EIGEN_INF4,				"版本", \
									"Branch") \
									\
	_NL(RS_EIGEN_INF5,				"客户", \
									"Customer") \
									\
	_NL(RS_EIGEN_INF6,				"部门", \
									"AssignDept") \
									\
	_NL(RS_EIGEN_INF7,				"Assignee", \
									"Assignee") \
									\
	_NL(RS_UNKNOWN_HDTO,			"未知的hang detect值(%d)\n", \
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
*	资源数组
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern RS_UNIT *pRsAry;

/*************************************************************************
* DESCRIPTION
*	根据资源索引获取字符串
*
* PARAMETERS
*	Id: 资源索引
*
* RETURNS
*	字符串
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define GetCStr(Id) pRsAry[Id].pStr

/*************************************************************************
* DESCRIPTION
*	输出信息到文件
*
* PARAMETERS
*	fp: 文件句柄
*	Id: 资源索引
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define LOG(fp, Id, ...) fprintf(fp, GetCStr(Id), ##__VA_ARGS__)

/*************************************************************************
* DESCRIPTION
*	输出常量字符串到文件
*
* PARAMETERS
*	fp: 文件句柄
*	ConstStr: 常量字符串
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define LOGC(fp, ConstStr, ...) fprintf(fp, ConstStr, ##__VA_ARGS__)

/*************************************************************************
* DESCRIPTION
*	输出异常信息
*
* PARAMETERS
*	Id: 资源索引
*	arg: 额外参数
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern int SstErr[16], SstErrIdx;
#define __SSTERR(Id)	SstErr[SstErrIdx++&(sizeof(SstErr) / sizeof(SstErr[0]) - 1)] = Id /* 注意: 只能在DEBUG版本直接使用!!! */
#define SSTERR(Id, ...) do { __SSTERR(Id); LOG(stderr, RS_ERROR); LOG(stderr, Id, ##__VA_ARGS__); } while (0)

/*************************************************************************
* DESCRIPTION
*	debug版本输出异常信息
*
* PARAMETERS
*	ConstStr: 常量字符串
*
* RETURNS
*	无
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
*	输出警告信息
*
* PARAMETERS
*	Id: 资源索引
*	arg: 额外参数
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define SSTWARN(Id, ...) do { LOG(stderr, RS_WARNING); LOG(stderr, Id, ##__VA_ARGS__); } while (0)

/*************************************************************************
* DESCRIPTION
*	输出信息
*
* PARAMETERS
*	Id: 资源索引
*	arg: 额外参数
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
#define SSTINFO(Id, ...) LOG(stdout, Id, ##__VA_ARGS__)


typedef struct _SST_OPT
{
	const char *pInstallDir; /* 安装目录, 为NULL则关闭依赖该目录的功能(sc, ref, fb) */

#define DEF_SYMDIR  "symbols"
	const char *pSymDir;
	const char *pCrId; /* 为NULL则不添加到logeigen系统 */
	unsigned SymDirLen;

#define SST_HPAUSE_BIT		(1 << 0) /* 程序完成后无暂停 */
#define SST_HVIEW_BIT		(1 << 1) /* 程序完成后不启动记事本显示报告 */
#define SST_LAUNCH_MASK		(3 << 2) /* 启动方式 */
#define SST_DIRECT_LAUNCH   (0 << 2) /* 直接启动 */
#define SST_RIGHT_LAUNCH	(1 << 2) /* 右键启动 */
#define SST_QAAT_LAUNCH		(2 << 2) /* qaat启动 */
#define SST_GENCMM_BIT		(1 << 4) /* 是否生成cmm脚本 */
#define SST_GENGDB_BIT		(1 << 5) /* 是否生成gdb脚本 */
#define SST_SCRLOADALL_BIT	(1 << 6) /* 脚本加载所有可能的库 */
#define SST_ALLTHREADS_BIT	(1 << 7) /* 是否显示所有进程 */
#define SST_LANG_MASK		(3 << 8) /* 语言模式 */
#define SST_CHS_LANG		(0 << 8) /* 简体模式 */
#define SST_ENG_LANG		(1 << 8) /* 英语模式 */
#define SST_TW_LANG			(2 << 8) /* 繁体模式 */
#define SST_REF_BIT			(1 << 10) /* 参考共享信息 */
#define SST_FB_BIT			(1 << 11) /* 反馈信息 */
#define SST_SC_BIT			(1 << 12) /* symcache功能 */
#define SST_LOGEIGEN_BIT	(1 << 13) /* logeigen功能 */
	unsigned Flag;
} SST_OPT;

/*************************************************************************
* DESCRIPTION
*	功能开关
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
*	配置结构体
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern SST_OPT SstOpt;

#endif
