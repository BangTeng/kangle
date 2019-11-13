#ifndef MSOCKET_FORWIN32_H
#define MSOCKET_FORWIN32_H
#include "kfeature.h"
KBEGIN_DECLS
#ifdef _WIN32
#include <ws2tcpip.h>
#include <io.h>
#include <stdio.h>
#include <process.h>
#include <windows.h>
#define KTHREAD_FUNCTION  void
#define KTHREAD_RETURN    return
#define snprintf _snprintf
#define O_SYNC	_O_WRONLY
#define pthread_create(a,b,c,d)		_beginthread(c,0,d)
#define PTHREAD_CREATE_SUCCESSED(x) (x!=-1)
#define lstat _stati64
#ifndef S_ISREG
#define S_ISREG(s)  (s & _S_IFREG)
#endif
#ifndef S_ISDIR
#define S_ISDIR(s)	(s & _S_IFDIR)
#endif
#define fseeko _fseeki64
#define pthread_key_t				DWORD
#define pthread_mutex_t 			HANDLE 
#define pthread_mutex_lock(x)	    WaitForSingleObject(*x,INFINITE)
#define pthread_mutex_unlock(x)		ReleaseMutex(*x)
INLINE int pthread_key_create(pthread_key_t *key, void *t)
{
	*key = TlsAlloc();
	if (*key == TLS_OUT_OF_INDEXES) {
		return 1;
	}
	return 0;
}
INLINE int pthread_setspecific(pthread_key_t key, void *arg)
{
	if (TlsSetValue(key, arg)) {
		return 0;
	}
	return 1;
}
INLINE void *pthread_getspecific(pthread_key_t key)
{
	return TlsGetValue(key);
}
INLINE int pthread_mutex_init(pthread_mutex_t *mutex,void *t)
{
	*mutex=CreateMutex(NULL,FALSE,NULL);
	return 0;
};
INLINE int pthread_mutex_destroy(pthread_mutex_t *mutex)
{
	CloseHandle(*mutex);
	return 0;
}
#define getpid()					GetCurrentProcessId()
#define pthread_self()				GetCurrentThreadId()
#define sleep(a)					Sleep(1000*a)
#define poll(a,b,c)                 WSAPoll(a,b,c)
#define pthread_t					unsigned

#ifndef bzero
#define bzero(x,y)	memset(x,0,y)
#endif
#define syslog		klog
#define		strncasecmp	_strnicmp
#define		strcasecmp	_stricmp
#define		strdup		_strdup
#define		ERRNO			WSAGetLastError()
#define		CLOSE(so)		closesocket(so)
#define		strtok_r(a,b,c)		strtok(a,b)
#define ctime_r( _clock, _buf ) 	( strcpy( (_buf), ctime( (_clock) ) ), (_buf) )
#define gmtime_r( _clock, _result ) ( *(_result) = *gmtime( (_clock) ), (_result) )
#define localtime_r(a,b) localtime_s(b,a)
#define mkdir(a,b) _mkdir(a)
#define unlink(a)	_unlink(a)
#define PID_LIKE(x)  (x!=INVALID_HANDLE_VALUE)
#define filecmp		_stricmp
#define filencmp 	_strnicmp
#define pid_t       HANDLE
#define PATH_SPLIT_CHAR		'\\'
#define iovec          WSABUF
#define iov_base       buf
#define iov_len        len
#define FILE_HANDLE     HANDLE
typedef HANDLE Token_t;
#else
#define ntohll be64toh
#define htonll htobe64
#define FILE_HANDLE     int
#define PID_LIKE(x)  (x>0)
#define PTHREAD_CREATE_SUCCESSED(x) (x==0)
typedef void * KTHREAD_FUNCTION;
#define KTHREAD_RETURN do { return NULL; }while(0)
#define filecmp		strcmp
#define filencmp 	strncmp
#define LoadLibrary(x) dlopen(x,RTLD_NOW|RTLD_LOCAL)
#define GetProcAddress dlsym
#define FreeLibrary	dlclose
#define SetDllDirectory(x)
#define GetLastError()	errno
#define _stati64 stat
#define _stat64 stat
typedef struct iovec   WSABUF;
typedef struct iovec * LPWSABUF;
typedef int * Token_t;
#define PATH_SPLIT_CHAR		'/'
#endif
#ifndef WIN32
#include <sys/types.h>
#include <sys/uio.h>
#endif
KEND_DECLS
#endif
