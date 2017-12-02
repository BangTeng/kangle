/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef for_win32_include_skdjfskdfkjsdfj
#define for_win32_include_skdjfskdfkjsdfj

#define PID_LIKE(x)  (x>0)
#define PTHREAD_CREATE_SUCCESSED(x) (x==0)
#define FUNC_CALL
typedef void * FUNC_TYPE;
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
#define PATH_SPLIT_CHAR		'/'

#ifndef WIN32
#include <sys/types.h>
#endif

#ifdef _WIN32
typedef HANDLE Token_t;
#else
typedef int * Token_t;
#endif
// 禁止使用拷贝构造函数和 operator= 赋值操作的宏
// 应该类的 private: 中使用

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
            TypeName(const TypeName&); \
            void operator=(const TypeName&)
#endif
