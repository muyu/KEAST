/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	symfile.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/5/22
* Description: 符号文件解析
**********************************************************************************/
#ifndef _SYMFILE_H
#define _SYMFILE_H

#include <ttypes.h>
#include <stdio.h>


#ifdef SST64
#define START_KERNEL	(~(TADDR)0 << ADDR_BITS)
#define KVM_OFF			0x80000
#define MOD_NAME_LEN	55 /* 不包含'\0', 不能使用sizeof!!! */
#else
#define START_KERNEL	0xBF000000
#define KVM_OFF			0x8000
#define MOD_NAME_LEN	59
#endif

typedef const struct _SYM_HDL
{
	TADDR LoadBias; /* LoadBias + vmlinux_vaddr = coredump addr */

#define SYM_LOAD	(1 << 0) /* 已加载 */
#define SYM_MATCH	(1 << 1) /* 文件匹配 */
#define SYM_LCLSYM	(1 << 4) /* 存在局部符号信息(加载sym文件后才有, 有可能是diff文件的) */
#define SYM_RBITS	14 /* 预留bit位数(内部使用) */
#define SYM_NOTICE	(1 << SYM_RBITS) /* 已提示过 */
/* 这里添加其他bit信息 */
	unsigned Flag;
	const char *pName; /* elf名字，不包含目录 */
} SYM_HDL;

extern SYM_HDL *pVmSHdl; /* vmlinux符号文件句柄 */

typedef const struct _DBG_HDL
{
	void *u[5];
} DBG_HDL;


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
/* 类型, 和elf属性同步!!! */
#define OPT_SHIFT	 0
#define OPT_NOTYPE	(1 << (OPT_SHIFT + 0)) /* 无类型 */
#define OPT_OBJECT	(1 << (OPT_SHIFT + 1)) /* 变量 */
#define OPT_FUNC	((1 << (OPT_SHIFT + 2))|(1 << (OPT_SHIFT + 13))) /* 函数 */
#define OPT_MASK(Op) ((Op)&0xFFFF)
/* 范围 */
#define OPB_SHIFT	16
#define OPB_LOCAL	(1 << (OPB_SHIFT + 0)) /* 局部符号 */
#define OPB_GLOBAL	(1 << (OPB_SHIFT + 1)) /* 全局符号 */
#define OPB_MASK(Op) ((Op) >> 16)

#define TGLBVAR		(OPB_GLOBAL|OPT_OBJECT) /* 全局变量 */
#define TLCLVAR		(OPB_LOCAL|OPT_OBJECT) /* 局部变量 */
#define TALLVAR		(OPB_LOCAL|OPB_GLOBAL|OPT_OBJECT) /* 所有变量 */
#define TGLBFUN		(OPB_GLOBAL|OPT_FUNC) /* 全局函数 */
#define TLCLFUN		(OPB_LOCAL|OPT_FUNC) /* 局部函数 */
#define TALLFUN		(OPB_LOCAL|OPB_GLOBAL|OPT_FUNC) /* 所有函数 */

/*************************************************************************
* DESCRIPTION
*	根据符号名称获取地址
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pSize: 不为NULL时返回符号大小(字节), 0表示不确定
*	pName: 符号名
*	Op: 查询属性: OP*
*
* RETURNS
*	0: 失败/无该符号
*	其他: 地址, 如果是函数则包含THUMB bit
*
* GLOBAL AFFECTED
*
*************************************************************************/
TADDR SymName2Addr(SYM_HDL *pSymHdl, TSIZE *pSize, const char *pName, unsigned Op);

/*************************************************************************
* DESCRIPTION
*	根据文件名获取dbg句柄
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pDbgHdl: 返回dbg句柄
*	pFileName: 文件名
*
* RETURNS
*	0: 成功
*	1: 失败/无该dbg信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymFile2DbgHdl(SYM_HDL *pSymHdl, struct _DBG_HDL *pDbgHdl, const char *pFileName);

/*************************************************************************
* DESCRIPTION
*	根据地址获取所在符号名
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pAddr: 不为NULL时返回符号地址, 如果是函数则包含THUMB bit
*	Addr: 地址, 如是thumb函数首地址不包含THUMB bit找不到!!!
*	Op: 查询属性: OP*
*
* RETURNS
*	NULL: 失败/无该地址对应的符号
*	其他: 符号名或pUndFuncName
*
* GLOBAL AFFECTED
*
*************************************************************************/
extern const char * const pUndFuncName; /* 未知函数名称 */
const char *SymAddr2Name(SYM_HDL *pSymHdl, TADDR *pAddr, TADDR Addr, unsigned Op);

/*************************************************************************
* DESCRIPTION
*	通过地址获取sym句柄对应路径和行号
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pLineNo: 返回行号
*	Addr: 地址
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *SymAddr2Line(SYM_HDL *pSymHdl, TUINT *pLineNo, TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	根据地址获取嵌套函数信息
*
* PARAMETERS
*	pSymHdl: sym句柄
*	Addr: 函数地址
*	pFunc: 回调函数, 按嵌套顺序回调
*		pSymHdl: sym句柄
*		FAddr: 函数首地址
*		pFName: 函数名
*		pDbgHdl: 函数dbg句柄
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymAddr2Func(SYM_HDL *pSymHdl, TADDR Addr
  , int (*pFunc)(SYM_HDL *pSymHdl, TADDR FAddr, const char *pFName, DBG_HDL *pDbgHdl, void *pArg), void *pArg);

/*************************************************************************
* DESCRIPTION
*	获取函数参数信息
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pDbgHdl: 函数dbg句柄
*	pFunc: 回调函数, 按参数顺序回调
*		pParaName: 函数参数名, 可能为NULL
*		Idx: 参数索引, 从0开始
*		Type: 函数参数类型
*		Val: 函数参数值
*		pArg: 参数
*	pRegAry/RegMask: 寄存器组/寄存器有效位图
*	pCallerRegAry/CParaMask/CParaMask2: 调用函数时寄存器组/参数有效位图(指令推导)/参数有效位图(栈推导), 为NULL则不使用ABI规则推导
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	1: 失败
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
typedef enum _BASE_TYPE
{
	CS_NOVAL, /* 值未知, 必须为0!!! */
	CS_BOOL,
	CS_INT,
	CS_UINT,
	CS_I64,
	CS_FLOAT,
	CS_DOUBLE,
	CS_STRING, /* 常量字符串 */
	CS_ADDR,
} BASE_TYPE;
typedef union _BASE_VAL
{
	TUINT n;
	TINT i;
	long long j;
	float f;
	double d;
	const char *pStr;
	TADDR p;
} BASE_VAL;
int SymGetFuncPara(SYM_HDL *pSymHdl, DBG_HDL *pDbgHdl, int (*pFunc)(const char *pParaName, int Idx, BASE_TYPE Type, BASE_VAL Val, void *pArg)
  , const TSIZE *pRegAry, TSIZE RegMask, const TSIZE *pCallerRegAry, TSIZE CParaMask, TSIZE CParaMask2, void *pArg);

/*************************************************************************
* DESCRIPTION
*	设置sym句柄的标志
*
* PARAMETERS
*	pSymHdl: sym句柄
*	Flag: 标志
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SymSetFlag(SYM_HDL *pSymHdl, unsigned Flag);

/*************************************************************************
* DESCRIPTION
*	遍历符号表
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pFunc: 回调函数
*		pName: 符号名
*		Val: 值
*		Size: 大小
*		Info: 其他信息(st_info/st_other/st_shndx)
*		pArg: 参数
*	pArg: 参数
*	Flag: 要扫描的符号表
*		bit0: 动态符号表
*		bit1: plt符号表
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymForEachSym(SYM_HDL *pSymHdl, int (*pFunc)(const char *pName, TADDR Val, TSIZE Size, TUINT Info, void *pArg), void *pArg, unsigned Flag);

/*************************************************************************
* DESCRIPTION
*	强制触发sym加载
*
* PARAMETERS
*	pSymHdl: sym句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void SymForceLoad(SYM_HDL *pSymHdl);

/*************************************************************************
* DESCRIPTION
*	还原1个栈帧
*
* PARAMETERS
*	pSymHdl: sym句柄
*	pRegAry/pRegMask: 寄存器组/寄存器有效位图(REG_SP/REG_PC必须有效!!!), 会被更新!!!
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int SymUnwind(SYM_HDL *pSymHdl, TSIZE *pRegAry, TSIZE *pRegMask);

/*************************************************************************
* DESCRIPTION
*	获取已加载文件的路径
*
* PARAMETERS
*	pSymHdl: sym句柄
*
* RETURNS
*	NULL: 失败
*	其他: 路径
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *SymGetFilePath(SYM_HDL *pSymHdl);


/*************************************************************************
*============================= debug section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取dbg句柄名称
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*
* RETURNS
*	NULL: 失败/无该属性
*	其他: 名称
*
* GLOBAL AFFECTED
*
*************************************************************************/
const char *DbgGetName(DBG_HDL *pDbgHdl);

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄占用字节
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*
* RETURNS
*	~(TSIZE)0: 失败/无该属性
*	其他: 占用字节
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DbgGetByteSz(DBG_HDL *pDbgHdl);

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄偏移量(比如结构体成员偏移量)
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*
* RETURNS
*	~(TSIZE)0: 失败/无该属性
*	其他: 偏移量
*
* GLOBAL AFFECTED
*
*************************************************************************/
TSIZE DbgGetOffset(DBG_HDL *pDbgHdl);

/*************************************************************************
* DESCRIPTION
*	批量获取dbg句柄的子句柄的偏移量(比如结构体成员偏移量)
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	pOffAry: 返回偏移量数组, 长度为n, 没有获取的填充-1
*	pNames: 成员名称数组, 长度为n
*	n: 个数
*
* RETURNS
*	0: 成功
*	其他: 获取失败个数
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetChildOffs(DBG_HDL *pDbgHdl, int *pOffAry, const char *pNames[], size_t n);

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄对应路径和行号
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	pLineNo: 返回行号
*
* RETURNS
*	NULL: 失败
*	其他: 路径，不用时需调用Free()释放!!!
*
* GLOBAL AFFECTED
*
*************************************************************************/
char *DbgGetLineInf(DBG_HDL *pDbgHdl, TUINT *pLineNo);

/*************************************************************************
* DESCRIPTION
*	获取dbg句柄的type dbg句柄
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	ptDbgHdl: 返回type dbg句柄
*
* RETURNS
*	0: 成功
*	1: 失败/无该dbg信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetTDbgHdl(DBG_HDL *pDbgHdl, struct _DBG_HDL *ptDbgHdl);

/*************************************************************************
* DESCRIPTION
*	根据名称获取dbg句柄的子dbg句柄
*
* PARAMETERS
*	pDbgHdl: dbg句柄
*	pcDbgHdl: 返回子dbg句柄
*	pName: 名称
*
* RETURNS
*	0: 成功
*	1: 失败/无该dbg信息
*
* GLOBAL AFFECTED
*
*************************************************************************/
int DbgGetCDbgHdl(DBG_HDL *pDbgHdl, struct _DBG_HDL *pcDbgHdl, const char *pName);


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据名称获取sym句柄, 会触发sym加载!!!
*
* PARAMETERS
*	pName: sym名称
*
* RETURNS
*	NULL: 没有
*	其他: sym句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
SYM_HDL *GetSymHdlByName(const char *pName);

/*************************************************************************
* DESCRIPTION
*	根据地址获取sym句柄, 会触发sym加载!!!
*
* PARAMETERS
*	Addr: 地址
*
* RETURNS
*	NULL: 地址不在任何sym句柄内
*	其他: sym句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
SYM_HDL *GetSymHdlByAddr(TADDR Addr);

/*************************************************************************
* DESCRIPTION
*	遍历sym句柄, 不会触发sym加载!!!
*
* PARAMETERS
*	pFunc: 回调函数
*		pSymHdl: sym句柄
*		pArg: 参数
*	pArg: 参数
*
* RETURNS
*	0: 成功
*	其他: 回调函数返回值
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ForEachSymHdl(int (*pFunc)(SYM_HDL *pSymHdl, void *pArg), void *pArg);


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化symfiles模块
*	使用方法: PrepareSymFiles() -> ModuleSymInit() -> DestorySymFiles()
*
* PARAMETERS
*	CoreCnt: CPU核数
*	CodeStart: kernel代码起始地址, 0表示未知
*	fp: 异常输出句柄
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int PrepareSymFiles(TUINT CoreCnt, TADDR CodeStart, FILE *fp);

/*************************************************************************
* DESCRIPTION
*    初始化module模块
*
* PARAMETERS
*    ModAddr: 模块结构体地址，如果不为0，则LoadBias和pName可以为空
*    LoadBias/Size: 加载偏移量和模块大小
*    pName: 模块名称
*
* RETURNS
*    0: 成功
*    1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int ModuleSymInit(TADDR ModAddr, TADDR LoadBias, TSIZE Size, const char *pName);

/*************************************************************************
* DESCRIPTION
*	注销symfiles模块
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
void DestorySymFiles(void);

#endif
