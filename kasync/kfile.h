#ifndef KFILE_H_99
#define KFILE_H_99
#include <sys/types.h>
#include <sys/stat.h> 
#include "kfeature.h"
#include "kforwin32.h"
KBEGIN_DECLS
#define KFILE_TEMP_MODEL       1
#define KFILE_ASYNC            2
#define KFILE_NOFOLLOW         4
#define KFILE_DSYNC            8
typedef enum
{
	fileRead,
	fileWrite,//truncate
	fileModify,
	fileReadWrite,
	fileWriteRead,//truncate
	fileAppend
} fileModel;

typedef enum
{
	seekBegin,
	seekEnd,
	seekCur
} seekPosion;
INLINE bool kfile_close_on_exec(FILE_HANDLE fd, bool close_on_exec)
{
#ifndef _WIN32
	//return fcntl(fd, F_SETFD, (closeExec ? FD_CLOEXEC : 0)) == 0;
#else
	return SetHandleInformation((HANDLE)fd, HANDLE_FLAG_INHERIT, (close_on_exec ? 0 : HANDLE_FLAG_INHERIT)) == 0;
#endif
	return true;
}
#ifdef _WIN32
INLINE int kfwrite(FILE_HANDLE h, const char *buf, int len)
{
	int ret = 0;
	if (WriteFile(h, (void *)buf, len, (LPDWORD)&ret, NULL)) {
		return ret;
	}
	return -1;
}
INLINE int kfread(FILE_HANDLE h, char *buf, int len)
{
	int ret = 0;
	if (ReadFile(h, (void *)buf, len, (LPDWORD)&ret, NULL)) {
		return ret;
	}
	return -1;
}
#define kfinit(h)              (h=INVALID_HANDLE_VALUE)
#define kflike(h)              (h!=INVALID_HANDLE_VALUE)
#define kfclose(h)             CloseHandle(h)
INLINE FILE_HANDLE kfopen_w(const wchar_t *path, fileModel model, int flag)
{
	int share_flag = FILE_SHARE_READ | FILE_SHARE_WRITE;
	int other_flag = 0;
	if (TEST(flag, KFILE_TEMP_MODEL)) {
		share_flag = 0;
		other_flag = (FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE);
		}
	if (TEST(flag, KFILE_ASYNC)) {
		SET(other_flag, FILE_FLAG_OVERLAPPED);
	}
	if (TEST(flag, KFILE_DSYNC)) {
		SET(other_flag, FILE_FLAG_WRITE_THROUGH);
	}
	SECURITY_ATTRIBUTES sa;
	memset(&sa, 0, sizeof(sa));
	sa.bInheritHandle = FALSE;
	int flag1 = 0;
	int flag2 = OPEN_EXISTING;
	switch (model) {
	case fileRead:
		flag1 = GENERIC_READ;
		break;
	case fileAppend:
		flag1 = FILE_APPEND_DATA;
		flag2 = OPEN_ALWAYS;
		break;
	case fileModify:
		flag1 = GENERIC_WRITE;
		flag2 = OPEN_ALWAYS;
		break;
	case fileWrite:
		flag1 = GENERIC_WRITE;
		flag2 = CREATE_ALWAYS;
		break;
	case fileReadWrite:
		flag1 = GENERIC_READ | GENERIC_WRITE;
		break;
	case fileWriteRead:
		flag1 = GENERIC_READ | GENERIC_WRITE;
		flag2 = CREATE_ALWAYS;
		break;
	}
	return CreateFileW(path,
		flag1,
		share_flag,
		&sa,
		flag2,
		other_flag,
		NULL);	
}
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <string.h>
#include <unistd.h>
#define kfclose(h)             close(h)
#define kfwrite(h,buf,len)     write(h,buf,len)
#define kfread(h,buf,len)      read(h,buf,len)
#define kflike(h)              (h>=0)
#define kfinit(h)              (h=-1)
#endif
FILE_HANDLE kfopen(const char *path, fileModel model, int flag);
INLINE int64_t kfsize(FILE_HANDLE fp)
{
#ifdef _WIN32
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileInformationByHandle(fp, &info)) {
		ULARGE_INTEGER    lv_Large;
		lv_Large.LowPart = info.nFileSizeLow;
		lv_Large.HighPart = info.nFileSizeHigh;
		return lv_Large.QuadPart;
	}
	return 0;
#else
	struct stat buf;
	if (fstat(fp, &buf) == 0) {
		return buf.st_size;
	}
	return 0;
#endif
}
//update file modified time
INLINE bool kfutime(FILE_HANDLE h, time_t time)
{
#ifdef _WIN32
	LARGE_INTEGER t;
	t.QuadPart = time * 10000000 + 116444736000000000;
	return SetFileTime(h, NULL, NULL, (FILETIME *)&t);
#else
	struct timeval t[2];
	memset(&t, 0, sizeof(t));
	t[0].tv_sec = time;
	t[1].tv_sec = time;
	return futimes(h, t) == 0;
#endif
}
INLINE time_t kfile_last_modified(const char *file)
{
	struct _stati64 sbuf;
	int ret = lstat(file, &sbuf);
	if (ret != 0 || !S_ISREG(sbuf.st_mode)) {
		return 0;
	}
	return sbuf.st_mtime;
}
//get file modified time
INLINE time_t kftime(FILE_HANDLE fp)
{
#ifdef _WIN32
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileInformationByHandle(fp, &info)) {
		ULARGE_INTEGER    lv_Large;
		lv_Large.LowPart = info.ftCreationTime.dwLowDateTime;
		lv_Large.HighPart = info.ftCreationTime.dwHighDateTime;
		return (time_t)((lv_Large.QuadPart - 116444736000000000) / 10000000);
	}
	return 0;
#else
	struct stat buf;
	if (fstat(fp, &buf) == 0) {
		return buf.st_mtime;
	}
	return 0;
#endif
}
bool kfseek(FILE_HANDLE fp,int64_t len, seekPosion position);
KEND_DECLS
#endif
