#ifndef KFILE_H_asdf1231
#define KFILE_H_asdf1231
#include "forwin32.h"
#include "global.h"
#include <stdarg.h>
enum fileModel
{
	fileRead,
	fileWrite,
	fileReadWrite,
	fileWriteRead,
	fileAppend
};
enum seekPosion
{
	seekBegin,
	seekEnd,
	seekCur
};
#define KFILE_TEMP_MODEL       1
#define KFILE_ASYNC            2
#define KFILE_NOFOLLOW         4

#ifdef _WIN32
#define FILE_HANDLE     HANDLE
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#define FILE_HANDLE     int
#endif
#ifdef ENABLE_SENDFILE
#include <sys/sendfile.h>
#endif

#include "KSocket.h"
inline time_t kfile_last_modified(const char *file)
{
	struct _stati64 sbuf;
	int ret = lstat(file, &sbuf);
	if (ret != 0 || !S_ISREG(sbuf.st_mode)) {
		return 0;
	}
	return sbuf.st_mtime;
}
#ifdef _WIN32
inline int kfwrite(FILE_HANDLE h,const char *buf,int len)
{
	int ret = 0;
	if(WriteFile(h,(void *)buf,len,(LPDWORD)&ret,NULL)){
		return ret;
	}
	return -1;
}
inline int kfread(FILE_HANDLE h,char *buf,int len)
{
	int ret = 0;
	if (ReadFile(h,(void *)buf,len,(LPDWORD)&ret,NULL)) {
		return ret;
	}
	return -1;
}
#define kfinit(h)              (h=INVALID_HANDLE_VALUE)
#define kflike(h)              (h!=INVALID_HANDLE_VALUE)
#define kfclose(h)             CloseHandle(h)         
#else
#define kfclose(h)             ::close(h)
#define kfwrite(h,buf,len)     ::write(h,buf,len)
#define kfread(h,buf,len)      ::read(h,buf,len)
#define kflike(h)              (h>=0)
#define kfinit(h)              (h=-1)
#endif
class KFile
{
public:
	KFile()
	{
		kfinit(fp);
	}
	~KFile()
	{
		this->close();
	}
	void flush()
	{
#ifdef _WIN32
		FlushFileBuffers(fp);
#else
		//fflush(fp);
#endif
	}
#ifdef ENABLE_SENDFILE
	int sendfile(SOCKET sockfd,off_t offset,int size)
	{
		return ::sendfile(sockfd,fp,&offset,(size_t)size);
	}
#endif
#ifdef _WIN32
	bool readEx(char *buf,int len,LPOVERLAPPED lp,LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
	{
		return ReadFileEx(fp,buf,len,lp,lpCompletionRoutine)==TRUE;
	}
	bool connectIOCP(HANDLE iocp,void *data)
	{
		CreateIoCompletionPort(fp, iocp,(ULONG_PTR)data, 0);
		return true;
	}
	bool openW(const wchar_t *path,fileModel model,int flag = 0)
	{
		int share_flag = FILE_SHARE_READ|FILE_SHARE_WRITE;
		int other_flag = 0;
		if (TEST(flag,KFILE_TEMP_MODEL)) {
			share_flag = 0;
			other_flag = (FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE);
		}
		if(TEST(flag,KFILE_ASYNC)){
			SET(other_flag,FILE_FLAG_OVERLAPPED);
		}
		SECURITY_ATTRIBUTES sa;
		memset(&sa,0,sizeof(sa));
		sa.bInheritHandle = FALSE;
		int flag1 = 0;
		int flag2 = OPEN_EXISTING;
		switch(model){
		case fileRead:
				flag1 = GENERIC_READ;
				break;
		case fileAppend:
				flag1 = FILE_APPEND_DATA;
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
				flag2 = CREATE_ALWAYS;
				flag1 = GENERIC_READ | GENERIC_WRITE;
				break;
		}
		fp =  CreateFileW(path,
			flag1 ,
			share_flag,
			&sa,
			flag2,
			other_flag,
			NULL);
		return kflike(fp);
	}
#endif
	INT64 getFileSize()
	{
#ifdef _WIN32
		BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(fp,&info)) {
			ULARGE_INTEGER    lv_Large ;
			lv_Large.LowPart  = info.nFileSizeLow   ;
			lv_Large.HighPart = info.nFileSizeHigh  ;
			return lv_Large.QuadPart;
		}
		return 0;
#else
		struct stat buf;
		if (fstat(fp,&buf)==0) {
			return buf.st_size;
		}
		return 0;	
#endif
	}
	INT64 getCreateTime()
	{
#ifdef _WIN32
		BY_HANDLE_FILE_INFORMATION info;
		if (GetFileInformationByHandle(fp,&info)) {
			ULARGE_INTEGER    lv_Large ;
			lv_Large.LowPart  = info.ftCreationTime.dwLowDateTime   ;
			lv_Large.HighPart = info.ftCreationTime.dwHighDateTime  ;
			return (lv_Large.QuadPart - 116444736000000000) / 10000000;
		}
		return 0;
#else
		struct stat buf;
		if (fstat(fp,&buf)==0) {
			return buf.st_mtime;
		}
		return 0;	
#endif
	}
	bool open(const char *path,fileModel model,int flag=0)
	{
#ifdef _WIN32
		int share_flag = FILE_SHARE_READ|FILE_SHARE_WRITE;
		int other_flag = 0;
		if (TEST(flag,KFILE_TEMP_MODEL)) {
			share_flag = 0;
			other_flag = (FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE);
		}
		if(TEST(flag,KFILE_ASYNC)){
			SET(other_flag,FILE_FLAG_OVERLAPPED);
		}
		SECURITY_ATTRIBUTES sa;
		memset(&sa,0,sizeof(sa));
		sa.bInheritHandle = FALSE;
		int flag1 = 0;
		int flag2 = OPEN_EXISTING;
		switch(model){
		case fileRead:
				flag1 = GENERIC_READ;
				break;
		case fileAppend:
				flag1 = FILE_APPEND_DATA;
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
				flag2 = CREATE_ALWAYS;
				flag1 = GENERIC_READ | GENERIC_WRITE;
				break;
		}

		fp =  CreateFile(path,
			flag1 ,
			share_flag,
			&sa,
			flag2,
			other_flag,
			NULL);
#else
		int f = 0;
		if (TEST(flag,KFILE_NOFOLLOW)) {
			SET(f,O_NOFOLLOW);
		}
		switch(model){
		case fileRead:
				f |= O_RDONLY;
				break;
		case fileAppend:
				f |= O_WRONLY|O_APPEND|O_CREAT;
				break;
		case fileWrite:
				f |= (O_WRONLY|O_CREAT|O_TRUNC);
				break;
		case fileReadWrite:
				f |= O_RDWR;
				break;
		case fileWriteRead:
				f |= (O_RDWR|O_CREAT);
				break;
		}
#ifdef LINUX
		fp = ::open(path,f|O_LARGEFILE,0666);
#else
		fp = ::open(path,f);
#endif
#endif
		return kflike(fp);
	}
	int vfprintf(const char *fmt,va_list ap)
	{
		int len;
		char buf[512];
		len = vsnprintf(buf,sizeof(buf),fmt,ap);
		return write(buf,len);
	}
	int fprintf(const char *fmt,...)
	{
		va_list ap;
		va_start(ap, fmt);
		int len = vfprintf(fmt,ap);
		va_end(ap);
		return len;
	}
	int read(char *buf,int len)
	{
		return kfread(fp,buf,len);
	}
	int write(const char *buf,int len)
	{
		return kfwrite(fp,buf,len);
	}
	void close()
	{
		if (kflike(fp)) {
			kfclose(fp);
			kfinit(fp);
		}
	}
	bool opened()
	{
		return kflike(fp);
	}
	bool seek(INT64 len,seekPosion position)
	{
#ifndef _WIN32
		int flag = 0;
		switch(position){
		case seekBegin:
			flag = SEEK_SET;
			break;
		case seekEnd:
			flag = SEEK_END;
			break;
		case seekCur:
			flag = SEEK_CUR;
			break;
		}
#ifdef LINUX
		return lseek64(fp,len,flag)!=-1;
#else
		return lseek(fp,len,flag)!=-1;
#endif
#else
		int flag = 0;
		switch (position) {
		case seekBegin:
			flag = FILE_BEGIN;
			break;
		case seekEnd:
			flag = FILE_END;
			break;
		case seekCur:
			flag = FILE_CURRENT;
			break;
		}
		LARGE_INTEGER li;
	    li.QuadPart = len;
		li.LowPart = SetFilePointer(fp,li.LowPart,&li.HighPart,flag);
		if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError()  != NO_ERROR) {
			return false;
	    }
		return true;

#endif
	}
	FILE_HANDLE stealHandle()
	{
		FILE_HANDLE fp2 = fp;
		kfinit(fp);
		return fp2;
	}
	FILE_HANDLE getHandle()
	{
		return fp;
	}
	void swap(KFile *file)
	{
		FILE_HANDLE t = file->fp;
		file->fp = fp;
		fp = t;
	}
	void setHandle(FILE_HANDLE fp)
	{
		this->fp = fp;
	}
private:
	FILE_HANDLE fp;
};
#endif
