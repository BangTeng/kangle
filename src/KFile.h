#ifndef KFILE_H_asdf1231
#define KFILE_H_asdf1231
#include "global.h"
#include <stdarg.h>
#ifndef _WIN32
#include <fcntl.h>
#endif
enum fileModel
{
	fileRead,
	fileWrite,//truncate
	fileModify,
	fileReadWrite,
	fileWriteRead,//truncate
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
#define KFILE_DSYNC            8

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
#include "forwin32.h"
#include <sys/types.h>  
#include <sys/stat.h>
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
		if (TEST(flag, KFILE_DSYNC)) {
			SET(other_flag, FILE_FLAG_WRITE_THROUGH);
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
		if (TEST(flag, KFILE_DSYNC)) {
			SET(other_flag, FILE_FLAG_WRITE_THROUGH);
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
		fp =  CreateFile(path,
			flag1 ,
			share_flag,
			&sa,
			flag2,
			other_flag,
			NULL);
#else
		int f = O_CLOEXEC;
#ifdef O_NOATIME
		SET(f,O_NOATIME);
#endif
		if (TEST(flag,KFILE_NOFOLLOW)) {
			SET(f,O_NOFOLLOW);
		}
		if (TEST(flag, KFILE_ASYNC)) {
			SET(f, O_DIRECT);
		}
		if (TEST(flag, KFILE_DSYNC)) {
#ifdef O_DSYNC
			SET(f, O_DSYNC);
#else
			SET(f,O_SYNC);
#endif
		}
		switch(model){
		case fileRead:
				f |= O_RDONLY;
				break;
		case fileAppend:
				f |= O_WRONLY|O_APPEND|O_CREAT;
				break;
		case fileModify:
				f |= (O_WRONLY | O_CREAT);
				break;
		case fileWrite:
				f |= (O_WRONLY | O_CREAT | O_TRUNC);
				break;
		case fileReadWrite:
				f |= O_RDWR;
				break;
		case fileWriteRead:
				f |= (O_RDWR|O_CREAT|O_TRUNC);
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
	void sync_data()
	{
#ifdef _WIN32
		FlushFileBuffers(fp);
#elif O_DSYNC
		fdatasync(fp);
#else
		fsync(fp);
#endif
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
#define KGL_MAX_BUFFER_FILE_SIZE 4194304
//#define KGL_MAX_BUFFER_FILE_SIZE 4
class KBufferFile : public KFile
{
public:
	~KBufferFile()
	{
		this->flush();
		free(buffer);
	}
	KBufferFile(int buffer_size)
	{
		assert(buffer_size > 0);
		total_write = 0;
		buffer_size = MIN(buffer_size, KGL_MAX_BUFFER_FILE_SIZE);
		buffer = (char *)malloc(buffer_size);
		hot = buffer;
		buffer_left = buffer_size;
		this->buffer_size = buffer_size;
	}
	void close()
	{
		this->flush();
		KFile::close();
	}
	int write(const char *buf, int len)
	{
		int orig_len = len;
		while (len>0) {
			int write_len = MIN(len, buffer_left);
			if (write_len <= 0) {
				if (!flush()) {
					return -1;
				}
				continue;
			}			
			if (buf != NULL) {
				memcpy(hot, buf, write_len);
				buf += write_len;
			}
			hot += write_len;
			len -= write_len;
			buffer_left -= write_len;
			total_write += write_len;
		};
		return orig_len;
	}
	INT64 get_total_write()
	{
		return total_write;
	}
private:
	bool flush()
	{
		int len = (int)(hot - buffer);
		bool result = KFile::write(buffer, len) == len;
		hot = buffer;
		buffer_left = buffer_size;
		return result;
	}
	INT64 total_write;
	int buffer_left;
	int buffer_size;
	char *buffer;
	char *hot;
};
#endif
