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
#ifndef malloc_debug_h_lskdjflskdjfskdj92394u2394234
#define malloc_debug_h_lskdjflskdjfskdj92394u2394234
#include "global.h"
#include <time.h>
#ifdef MALLOCDEBUG
//void list_all_malloc();
void dump_memory(int min_time,int max_time);

//void check_addr(void *ptr);
#ifndef _WIN32
/*
#include <string.h>
#include <string>
#include <vector>
#include <list>
#include <sstream>
#include <map>
#include<new>
#define xmalloc(x)		xmalloc2(x,__FILE__,__LINE__)
#define xfree(x)		xfree2(x,__FILE__,__LINE__)
#define xstrdup(x)		xstrdup2(x,__FILE__,__LINE__)
#define malloc(x)		xmalloc2(x,__FILE__,__LINE__)
#define free(x)			xfree2(x,__FILE__,__LINE__)
#define strdup(x)		xstrdup2(x,__FILE__,__LINE__)
#define kassert         assert
void xfree2(void *ptr, const char *file, int line);
void *xmalloc2(size_t size, const char *file, int line);
char *xstrdup2(const char * s, const char *file, int line);
*/
#define xmalloc(x)		malloc(x)
#define xfree(x)		free(x)
#define xstrdup(x)		strdup(x)
#define kassert         assert     
/*
void * operator new(size_t m_size, const char *file, int line);
void * operator new[](size_t m_size, const char *file, int line);
void operator delete(void *ptr);
#define new DEBUG_NEW2
#define DEBUG_NEW2 new(__FILE__, __LINE__)
*/
void * check_memory_thread(void* arg);
#else
#include <crtdbg.h>
#include <stdlib.h>
//#define   new   new( _NORMAL_BLOCK, __FILE__, __LINE__ )
//#define   malloc(x)     _malloc_dbg(x, _NORMAL_BLOCK, __FILE__, __LINE__)

#define xmalloc(x)		malloc(x)
#define xfree(x)		free(x)
#define xstrdup(x)		strdup(x)
void kassert(int result);
#endif
#else
#include <stdlib.h>
#define xmalloc(x)		malloc(x)
#define xfree(x)		free(x)
#define xstrdup(x)		strdup(x)
#define kassert         assert     
//#define dump_memory(a)		
#endif//END OF MALLOCDEBUG
#endif

