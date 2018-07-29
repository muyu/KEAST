/*********************************************************************************
* Copyright(C),2014-2017,MediaTek Inc.
* FileName:	os.h
* Author:		Guangye.Yang
* Version:	V1.0
* Date:		2014/4/1
* Description: os相关函数定义
**********************************************************************************/
#include <string.h>
#include <stdlib.h>

#if defined(__MINGW32__)
#include <stdint.h>   /*For define uintptr_t*/
#endif

#ifdef _WIN32
//#include <crtdbg.h> /* 必须在stdlib.h之后，用于内存调试!!! */
#include <direct.h> /* chdir(), mkdir() */
#define FP_HANDLE					HANDLE
#define CLOSE_HANDLE(fd)			CloseHandle(fd)
#ifdef SST64
#define MMAPRO(fd, Ptr, Off, Sz)	(((Ptr) = MapViewOfFile(fd, FILE_MAP_READ, Off >> 32, (DWORD)(Off), (size_t)(Sz))) == NULL)
#else
#define MMAPRO(fd, Ptr, Off, Sz)	(((Ptr) = MapViewOfFile(fd, FILE_MAP_READ, 0, Off, (size_t)(Sz))) == NULL)
#endif
#define MUNMAP(Ptr, Sz)				((void)(Sz), UnmapViewOfFile(Ptr) == 0)
#define GETZERO(Ptr, IsRw, Sz)		(((Ptr) = VirtualAlloc(NULL, (size_t)(Sz), MEM_RESERVE|MEM_COMMIT, (IsRw) ? PAGE_READWRITE : PAGE_READONLY)) == NULL)
#define PROTZERO(Ptr, Sz, rt)		(!VirtualProtect((void *)(Ptr), (size_t)(Sz), PAGE_READONLY, (PDWORD)&(rt)))
#define PUTZERO(Ptr, Sz)			((void)(Sz), !VirtualFree((void *)(Ptr), 0, MEM_RELEASE))
#define MKDIR(pDir)					mkdir(pDir)
#else
#include <sys/mman.h>
#include <dirent.h>
#include <fcntl.h>
#define FP_HANDLE					int
#define CLOSE_HANDLE(fd)			close(fd)
#define MMAPRO(fd, Ptr, Off, Sz)	(((Ptr) = mmap(NULL, (size_t)(Sz), PROT_READ, MAP_PRIVATE, fd, (size_t)(Off))) == (void *)-1)
#define MUNMAP(Ptr, Sz)				(munmap((void *)(Ptr), (size_t)(Sz)) == -1)
#define GETZERO(Ptr, IsRw, Sz)		(((Ptr) = mmap(NULL, (size_t)(Sz), PROT_READ|((IsRw) ? PROT_WRITE : 0), MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)) == (void *)-1)
#define PROTZERO(Ptr, Sz, rt)		((void)(rt), mprotect((void *)Ptr, (size_t)(Sz), PROT_READ))
#define PUTZERO(Ptr, Sz)			MUNMAP(Ptr, (size_t)(Sz))
#define MKDIR(pDir)					mkdir(pDir, 0755)
#endif
#include "string_res.h"
#include <module.h>
#include "os.h"


typedef struct _FP_INT
{
	struct _FP_HDL Ex; /* export info */
	FP_HANDLE fd;
	unsigned Ref; /* 句柄引用计数 */
#ifdef _DEBUG
	unsigned MapCnt;
#endif
	char Path[0];
} FP_INT;

static unsigned AGranMask; /* 分配粒度 */
#ifdef _DEBUG
static TSIZE MappedSize, MaxMappedSz;
#endif


/*************************************************************************
*============================= local section =============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	根据指定的目录查找指定的文件并返回文件的路径
*
* PARAMETERS
*	pPath: 大小必须>=MAX_PATH
*		输入: 指定的目录
*		输出: 找到的指定文件的路径
*	PLen: 指定目录的长度
*	pFile: 指定的文件名
*	NLen: 文件名长度
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int FindPathByName(char *pPath, unsigned PLen, const char *pFile, unsigned NLen)
{
#ifdef _WIN32
	struct _finddata_t FileInfo;
	unsigned Len;
	long Handle;

	if (PLen + NLen <= MAX_PATH - 1) {
		memcpy(pPath + PLen, pFile, NLen + 1);
		if (!access(pPath, 0) && (Handle = _findfirst(pPath, &FileInfo)) != -1) { /* 需要区分大小写 */
			_findclose(Handle);
			if (!strcmp(pFile, FileInfo.name))
				return 0;
		}
		memcpy(pPath + PLen, "*.*", 4);
		if ((Handle = _findfirst(pPath, &FileInfo)) != -1) {
			do {
				if ((FileInfo.attrib&_A_SUBDIR) && strcmp(FileInfo.name, ".") && strcmp(FileInfo.name, "..")) {
					Len = strlen(FileInfo.name);
					if (PLen + Len <= MAX_PATH - 2) {
						memcpy(pPath + PLen, FileInfo.name, Len);
						pPath[PLen + Len] = '/';
						if (!FindPathByName(pPath, PLen + Len + 1, pFile, NLen)) {
							_findclose(Handle);
							return 0;
						}
					}
				}
			} while (!_findnext(Handle, &FileInfo));
			_findclose(Handle);
		}
	}
	return 1;
#else
	struct dirent *ep;
	unsigned Len;
	DIR *dp;

	if (PLen + NLen <= MAX_PATH - 1) {
		memcpy(pPath + PLen, pFile, NLen + 1);
		if (!access(pPath, 0))
			return 0;
		pPath[PLen] = '\0';
		if ((dp = opendir(pPath)) != NULL) {
			while ((ep = readdir(dp)) != NULL) {
				if ((ep->d_type&DT_DIR) && strcmp(ep->d_name, ".") && strcmp(ep->d_name, "..")) {
					Len = strlen(ep->d_name);
					if (PLen + Len <= MAX_PATH - 2) {
						memcpy(pPath + PLen, ep->d_name, Len);
						pPath[PLen + Len] = '/';
						if (!FindPathByName(pPath, PLen + Len + 1, pFile, NLen)) {
							closedir(dp);
							return 0;
						}
					}
				}
			}
			closedir(dp);
		}
	}
	return 1;
#endif
}

/*************************************************************************
* DESCRIPTION
*	获取符号文件所在路径
*
* PARAMETERS
*	pPath: 输入buffer，返回找到的指定文件的路径
*	PathSz: pPath大小,必须 >= MAX_PATH!!!
*	pFileName: 文件名
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int GetSymFilePath(char *pPath, unsigned PathSz, const char *pFileName)
{
	unsigned PLen, NLen;
	static int Flag;

	PLen = SstOpt.SymDirLen;
	NLen = strlen(pFileName);
	if (PLen + NLen + 2 > PathSz)
		return 1;
	if (!Flag) {
		Flag = 1;
		if (!strcmp(SstOpt.pSymDir, DEF_SYMDIR))
			MKDIR(SstOpt.pSymDir);
	}
	memcpy(pPath, SstOpt.pSymDir, PLen);
	if (PLen && (pPath[PLen - 1] != '/' || pPath[PLen - 1] != '\\'))
		pPath[PLen++] = '/';
	return FindPathByName(pPath, PLen, pFileName, NLen);
}

/*************************************************************************
* DESCRIPTION
*	尝试释放已映射进来的内存
*
* PARAMETERS
*	Size: 预期释放的大小(字节)
*
* RETURNS
*	0: 成功
*	1: 失败
*
* LOCAL AFFECTED
*
*************************************************************************/
static int DefTryUnmap(TSIZE Size)
{
	(void)Size;
	return 1;
}

static int (*pTryUnmapFunc)(TSIZE Size) = DefTryUnmap;

#ifdef _DEBUG
/*************************************************************************
* DESCRIPTION
*	统计增加映射进来的大小
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void MappedSzInc(TSIZE Size)
{
	MappedSize += Size;
	if (MaxMappedSz < MappedSize)
		MaxMappedSz = MappedSize;
}

/*************************************************************************
* DESCRIPTION
*	统计减少映射进来的大小
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* LOCAL AFFECTED
*
*************************************************************************/
static void MappedSzDec(TSIZE Size)
{
	MappedSize -= Size;
}
#endif


/*************************************************************************
*=========================== external section ===========================
*************************************************************************/
#ifndef _DEBUG
/*************************************************************************
* DESCRIPTION
*	申请堆内存
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: 内存指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Malloc(size_t Size)
{
	void *p;
	
	p = malloc(Size);
	if (!(p || pTryUnmapFunc((TSIZE)Size)))
		p = malloc(Size);
	return p;
}

/*************************************************************************
* DESCRIPTION
*	申请堆内存并格式化为0
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: 内存指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Calloc(size_t Size)
{
	void *p;
	
	p = calloc(1, Size);
	if (!(p || pTryUnmapFunc((TSIZE)Size)))
		p = calloc(1, Size);
	return p;
}

/*************************************************************************
* DESCRIPTION
*	调整堆内存
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: 新的内存指针
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *Realloc(void *p, size_t NewSz)
{
	void *q;

	q = realloc(p, NewSz);
	if (!(q || pTryUnmapFunc((TSIZE)NewSz)))
		q = realloc(p, NewSz);
	return q;
}

/*************************************************************************
* DESCRIPTION
*	释放堆内存
*
* PARAMETERS
*	pMem: 内存指针
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void Free(void *pMem)
{
	free(pMem);
}
#endif

/*************************************************************************
* DESCRIPTION
*	创建目录
*
* PARAMETERS
*	pDir: 目录
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void MkFileDir(const char *pDir)
{
	MKDIR(pDir);
}

/*************************************************************************
* DESCRIPTION
*	获取raw kernel log文件名
*
* PARAMETERS
*	pFileName: 返回raw kernel log文件名, 大小必须>=MAX_PATH
*
* RETURNS
*	0: 成功
*	1: 失败
*
* GLOBAL AFFECTED
*
*************************************************************************/
int FindRawKernLog(char *pFileName)
{
	int rt = 1;
#ifdef _WIN32
	struct _finddata_t FileInfo;
	const long Handle = _findfirst("SYS_KERNEL_LOG_RAW*", &FileInfo);

	if (Handle == -1)
		return rt;
	if (!(FileInfo.attrib&_A_SUBDIR)) {
		strcpy(pFileName, FileInfo.name);
		rt = 0;
	}
	_findclose(Handle);
#else
	DIR * const dp = opendir(".");
	struct dirent *ep;

	if (!dp)
		return rt;
	while ((ep = readdir(dp)) != NULL) {
		if (!(ep->d_type&DT_DIR) && !strncmp(ep->d_name, "SYS_KERNEL_LOG_RAW", sizeof("SYS_KERNEL_LOG_RAW") - 1)) {
			strcpy(pFileName, ep->d_name);
			rt = 0;
			break;
		}
	}
	closedir(dp);
#endif
	return rt;
}

/*************************************************************************
* DESCRIPTION
*	注册映射失败时尝试释放的函数(空间回收)
*
* PARAMETERS
*	pFunc: 函数指针
*	Size: 预期释放的大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void RegTryUnmapFunc(int (*pFunc)(TSIZE Size))
{
	pTryUnmapFunc = pFunc;
}

/*************************************************************************
* DESCRIPTION
*	获取0内存, 可以被修改
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
void *GetZeroBuf(TSIZE Size)
{
	void *pBuf;
	
	if (sizeof(Size) > sizeof(size_t) && Size >= (TSIZE)~(size_t)0) {
		DBGERR("长度溢出: "TSIZEFMT"\n", Size);
		return NULL;
	}
	if (GETZERO(pBuf, 1, Size) && (pTryUnmapFunc(Size) || GETZERO(pBuf, 1, Size)))
		return NULL;
#ifdef _DEBUG
	MappedSzInc(Size);
#endif
	return pBuf;
}

/*************************************************************************
* DESCRIPTION
*	获取0只读内存
*
* PARAMETERS
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *GetZeroRoBuf(TSIZE Size)
{
	const void *pBuf;

	if (sizeof(Size) > sizeof(size_t) && Size >= (TSIZE)~(size_t)0) {
		DBGERR("长度溢出: "TSIZEFMT"\n", Size);
		return NULL;
	}
	if (GETZERO(pBuf, 0, Size) && (pTryUnmapFunc(Size) || GETZERO(pBuf, 0, Size)))
		return NULL;
#ifdef _DEBUG
	MappedSzInc(Size);
#endif
	return pBuf;
}

/*************************************************************************
* DESCRIPTION
*	设置0内存为只读
*
* PARAMETERS
*	pBuf: buffer
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void ProtectZeroBuf(const void *pBuf, TSIZE Size)
{
	size_t rt;
	
	if (PROTZERO(pBuf, Size, rt))
		DBGERR("设置内存只读属性失败\n");
}

/*************************************************************************
* DESCRIPTION
*	释放0内存
*
* PARAMETERS
*	pBuf: buffer
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutZeroBuf(const void *pBuf, TSIZE Size)
{
#ifdef _DEBUG
	if (pBuf)
		MappedSzDec(Size);
#endif
	if (PUTZERO(pBuf, Size))
		SSTERR(RS_FILE_UNMAP, "0");
}

/*************************************************************************
* DESCRIPTION
*	映射文件到内存
*
* PARAMETERS
*	pFpHdl: fp句柄
*	Off: 偏移量(字节)
*	Size: 大小(字节)
*
* RETURNS
*	NULL: 失败
*	其他: buffer
*
* GLOBAL AFFECTED
*
*************************************************************************/
const void *FpMapRo(FP_HDL *pFpHdl, TSIZE Off, TSIZE Size)
{
	FP_INT * const pFp = MOD_INT(FP, pFpHdl);
	const void *pBuf;
	unsigned n;

	n = Off&AGranMask;
	Off  -= n;
	Size += n;
	if (sizeof(Size) > sizeof(size_t) && Size >= (TSIZE)~(size_t)0) {
		DBGERR("长度溢出: "TADDRFMT"\n", Size);
		return NULL;
	}
	if (MMAPRO(pFp->fd, pBuf, Off, Size) && (pTryUnmapFunc(Size) || MMAPRO(pFp->fd, pBuf, Off, Size)))
		return NULL;
#ifdef _DEBUG
	pFp->MapCnt++;
	MappedSzInc(Size);
#endif
	return (char *)pBuf + n;
}

/*************************************************************************
* DESCRIPTION
*	释放文件映射内存
*
* PARAMETERS
*	pFpHdl: fp句柄
*	pBuf: buffer
*	Size: 大小(字节)
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void FpUnmap(FP_HDL *pFpHdl, const void *pBuf, TSIZE Size)
{
	FP_INT * const pFp = MOD_INT(FP, pFpHdl);
	unsigned n;

	n = (unsigned)((uintptr_t)pBuf&AGranMask);
	if (MUNMAP((char *)pBuf - n/* 对齐 */, Size + n))
		SSTERR(RS_FILE_UNMAP, pFp->Path);
#ifdef _DEBUG
	MappedSzDec(Size + n);
	pFp->MapCnt--;
#endif
}


/*************************************************************************
*============================ handle section ============================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	获取指定文件的句柄
*
* PARAMETERS
*	pFile: 文件路径或文件名
*	FromSymDir: 0: pFile是路径, 1: 到SstOpt.pSymDir目录搜索pFile
*
* RETURNS
*	NULL: 失败
*	其他: fp句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
FP_HDL *GetFpHdl(const char *pFile, int FromSymDir)
{
	const char *pPath = pFile;
	char Path[MAX_PATH];
	unsigned Len;
	TSIZE FileSz;
	FP_HANDLE fd;
	FP_INT *pFp;
	
	if (FromSymDir) {
		if (GetSymFilePath(Path, sizeof(Path), pFile))
			return NULL;
		pPath = Path;
	}
{
#ifdef _WIN32
	DWORD SzHigh = 0;
	HANDLE Hdl;

	Hdl = CreateFileA(pPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	if (Hdl == INVALID_HANDLE_VALUE)
		return NULL;
	FileSz = (TSIZE)GetFileSize(Hdl, &SzHigh);
#ifdef SST64
	FileSz |= ((TSIZE)SzHigh << 32);
#else
	if (SzHigh)
		FileSz = ~(TSIZE)0;
#endif
	fd = CreateFileMapping(Hdl, NULL, PAGE_READONLY, 0, 0, NULL);
	CloseHandle(Hdl);
	if (!fd)
		return NULL;
#else
	struct stat st;

	fd = open(pPath, O_RDONLY);
	if (fd < 0)
		return NULL;
	if (fstat(fd, &st) || !S_ISREG(st.st_mode)) {
		close(fd);
		return NULL;
	}
	FileSz = st.st_size;
#endif
}
	Len = strlen(pPath) + 1;
	pFp = malloc(sizeof(*pFp) + Len);
	if (!pFp) {
		CLOSE_HANDLE(fd);
		return NULL;
	}
	pFp->Ref = 1;
#ifdef _DEBUG
	pFp->MapCnt = 0;
#endif
	pFp->fd = fd;
	pFp->Ex.FileSz = FileSz;
	pFp->Ex.pPath  = pFp->Path;
	memcpy(pFp->Path, pPath, Len);
	if (FromSymDir)
		SSTINFO(RS_LOADING, pFile);
	return MOD_HDL(pFp);
}

/*************************************************************************
* DESCRIPTION
*	复制fp句柄
*
* PARAMETERS
*	pFpHdl: fp句柄
*
* RETURNS
*	新的fp句柄
*
* GLOBAL AFFECTED
*
*************************************************************************/
FP_HDL *DupFpHdl(FP_HDL *pFpHdl)
{
	FP_INT * const pFp = MOD_INT(FP, pFpHdl);

	pFp->Ref++;
	return pFpHdl;
}

/*************************************************************************
* DESCRIPTION
*	释放文件句柄
*
* PARAMETERS
*	pFpHdl: fp句柄
*
* RETURNS
*	无
*
* GLOBAL AFFECTED
*
*************************************************************************/
void PutFpHdl(FP_HDL *pFpHdl)
{
	FP_INT * const pFp = MOD_INT(FP, pFpHdl);

	if (--pFp->Ref)
		return;
#ifdef _DEBUG
	if (pFp->MapCnt)
		DBGERR("未释放映射(%d): %s\n", pFp->MapCnt, pFp->Path);
#endif
	CLOSE_HANDLE(pFp->fd);
	free(pFp);
}


/*************************************************************************
*========================== init/deinit section ==========================
*************************************************************************/
/*************************************************************************
* DESCRIPTION
*	初始化os模块
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
void OsInit(void)
{
#ifdef _WIN32
	SYSTEM_INFO SymInf;

	GetSystemInfo(&SymInf);
	AGranMask = SymInf.dwAllocationGranularity - 1;
#else
	AGranMask = sysconf(_SC_PAGE_SIZE) - 1;
#endif
}

/*************************************************************************
* DESCRIPTION
*	注销os模块
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
void OsDeInit(void)
{
#ifdef _DEBUG
	if (MaxMappedSz > 1024 * 1024 * 500)
		LOGC(stdout, "映射峰值: %dMB\n", (unsigned)(MaxMappedSz >> 20));
	if (MappedSize)
		DBGERR("存在映射泄露: %dMB\n", (unsigned)(MappedSize >> 20));
#endif
}
