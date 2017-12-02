/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include<new>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "KMutex.h"
//#include "log.h"
#include "global.h"
//#include "log.h"
//#include "malloc_debug.h"
#ifdef MALLOCDEBUG
#ifndef _WIN32
//#define MALLOC_CHECK_END 1
#include<syslog.h>

#include<sys/mman.h>
#include<signal.h>
//#include<malloc.h>
#include<time.h>
#include <sstream>
#include "trace.h"
using namespace std;
#define MEMORY_BAD_END		1
#define MEMORY_BAD_FRONT	2

static char start_magic[] = { 0x12, 0x56, 0xdc, 0x78, 0xef, 0x34, 0x45, 0xc3 };
static char end_magic[] = { 0xc3, 0x45, 0x34, 0xef, 0x78, 0xdc, 0x56, 0x12 };

static int bad_memory_count = 0;
#ifdef MALLOC_CHECK_END
#define PGSZ 4096UL
#define PAGEMASK (PGSZ - 1)
#define POINTER_FORMAT "%#08lx"
#endif
#ifndef DEBUG_NEW_HASHTABLESIZE
#define DEBUG_NEW_HASHTABLESIZE 16384
#endif
#ifndef DEBUG_NEW_HASH
#define DEBUG_NEW_HASH(p) (((unsigned long)(p) >> 8) % DEBUG_NEW_HASHTABLESIZE)
#endif

#ifdef MALLOC_CHECK_END
static inline unsigned long round_down(unsigned long x, unsigned long mul) {
	return x - (x % mul);
}
static inline unsigned long round_up(unsigned long x, unsigned long mul) {
	return round_down(x + (mul - 1), mul);
}
#endif
class new_ptr_list_t {
public:
	new_ptr_list_t() {
		next = NULL;
		flag = 0;
	}
	;
	TRACEBACK where_alloced;
	int size;
#ifdef MALLOC_CHECK_END
	char *real_addr;
	int real_size;
#endif
	char flag;
	char *user_addr;
	time_t malloc_time;
	new_ptr_list_t* next;
};
static new_ptr_list_t* new_ptr_list[DEBUG_NEW_HASHTABLESIZE];
static new_ptr_list_t *bad_ptr = NULL;
//static KMutex malloc_mutex(&attr,PTHREAD_MUTEX_RECURSIVE)[DEBUG_NEW_HASHTABLESIZE+1];
static KMutex malloc_mutex[DEBUG_NEW_HASHTABLESIZE];
static bool malloc_started = false;
void start_hook_alloc()
{
	malloc_started = true;	
}
#ifdef MALLOC_CHECK_END
static int zap(void * p, size_t nbytes) {
	int v;
	/*  fprintf(stderr, "Doing mprotect(%p, %u, PROT_NONE)\n", p, nb); */
	v = mprotect(p, nbytes, PROT_NONE);
	if (v != 0) {
		//        perror("unmap: mprotect");
	}
	return v;
}
static int unzap(void * p, size_t nbytes) {
	int v;
	v = mprotect((void *)p, nbytes, PROT_READ | PROT_WRITE);
	if (v != 0) {
		//perror("unmap: mprotect");
	}
	return v;
}
void *do_valloc(int nbytes)
{
	/* void *p;
	 p = mmap(NULL, nbytes, PROT_READ | PROT_WRITE,
	 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	 if (p == MAP_FAILED){
	 perror("mmap:");
	 return NULL;
	 }
	 else
	 return p;
	 */
	/*void *p=NULL;
	 posix_memalign(&p,PGSZ,nbytes);
	 return p;
	 */
	return memalign(PGSZ,nbytes);
	//	return malloc(nbytes);
}
void free_valloc(void *addr,int len)
{
	/*
	 int ret=munmap(addr, len);
	 if(ret!=0){
	 perror("munmap:");
	 }
	 */
	//	printf("free_valloc=%x\n",addr);
	free(addr);
}
#endif
extern "C" {
        extern void *__libc_malloc(size_t size);
        extern void __libc_free(void *p);
}
void add_bad_ptr(new_ptr_list_t *ptr) {
	if (bad_memory_count == 0) {
		bad_ptr = NULL;
	}
	bad_memory_count++;
	if (bad_ptr == NULL) {
		bad_ptr = ptr;
		ptr->next = NULL;
		return;
	}
	new_ptr_list_t *last = bad_ptr;
	new_ptr_list_t *prev = NULL;
	while (last) {
		if (ptr->user_addr < last->user_addr) {
			if (prev) {
				prev->next = ptr;
			}
			ptr->next = last;
			if (prev == NULL) {
				bad_ptr = ptr;
			}
			return;
		}
		if (last->next == NULL) {
			last->next = ptr;
			ptr->next = NULL;
			return;
		}
		prev = last;
		last = last->next;
	}

}
bool check_ptr(new_ptr_list_t *ptr) {
	ptr->flag = 0;
	if (memcmp(ptr->user_addr - sizeof(start_magic), start_magic,
			sizeof(start_magic)) != 0) {
		ptr->flag |= MEMORY_BAD_FRONT;
	}
	if (memcmp(ptr->user_addr + ptr->size, end_magic, sizeof(end_magic)) != 0) {
		ptr->flag |= MEMORY_BAD_END;
	}
	if (ptr->flag != 0) {
		add_bad_ptr(ptr);
		return true;
	}
	return false;
}
void check_all_ptr() {
	for (int i = 0; i < DEBUG_NEW_HASHTABLESIZE; ++i) {
		malloc_mutex[i].Lock();
		new_ptr_list_t* ptr = new_ptr_list[i];
		if (ptr == NULL) {
			malloc_mutex[i].Unlock();
			continue;
		}
		while (ptr) {
			new_ptr_list_t *next = ptr->next;
			check_ptr(ptr);
			ptr = next;
		}
		malloc_mutex[i].Unlock();
	}
	if (bad_memory_count > 0) {
		abort();
	}
}
void dump_memory(int min_time,int max_time) {
	unsigned leakSize = 0;
	unsigned leakCount = 0;
	time_t now_time = time(NULL);
	char buf[512];
	int buf_len;
	for (int i = 0; i < DEBUG_NEW_HASHTABLESIZE; ++i) {
		malloc_mutex[i].Lock();
		new_ptr_list_t* ptr = new_ptr_list[i];
		while (ptr) {
			int ptr_past_time = now_time - ptr->malloc_time;
			if (ptr_past_time >=min_time && (max_time<0 || ptr_past_time<=max_time)) {
				buf_len = 0;
				for (int j = 0; j < MAX_TRACEBACK_LEVELS; j++) {
					int max_len = sizeof(buf) - buf_len - 1;
					if (max_len <= 0 || ptr->where_alloced[j] == 0) {
						buf[buf_len] = 0;
						break;
					}
					buf_len += snprintf(buf + buf_len, max_len, "%p ",
							ptr->where_alloced[j]);

				}
				/*		printf(
				 "dump memory at %p (size %u, %s:%d,past time:%d,stack: %s)\n",
				 ptr->user_addr + sizeof(magic), ptr->size, ptr->file,
				 ptr->line, ptr_past_time, buf);
				 */
				syslog(
						LOG_NOTICE,
						"dump memory at %p (size %u,past time:%d,stack: %s)\n",
						ptr->user_addr + sizeof(start_magic), ptr->size,
						ptr_past_time, buf);
				leakSize += ptr->size;
				leakCount++;
			}
			ptr = ptr->next;
		}
		malloc_mutex[i].Unlock();
	}
	syslog(LOG_NOTICE, "Total leaked size=%d,count=%d\n", leakSize, leakCount);

}

void list_all_malloc() {
	dump_memory(0,-1);
}
void * my_malloc(size_t size) {
	/*
	if(size>10240000){
		abort();
	}
	*/
	if (!malloc_started) {
		return __libc_malloc(size);
	}
	new_ptr_list_t* ptr = (new_ptr_list_t*) __libc_malloc(sizeof(new_ptr_list_t));
	if (ptr == NULL) {
		//	fprintf(stderr, "new:  out of memory when allocating %u bytes\n", size);
		abort();
		return NULL;
	}

	char *real_addr = (char *) __libc_malloc(size + 2 * sizeof(start_magic));
	if (real_addr == NULL) {
		perror("malloc:");
		abort();
		return NULL;
	}
	memcpy(real_addr, start_magic, sizeof(start_magic));
	ptr->user_addr = real_addr + sizeof(start_magic);
	memset(ptr->user_addr, 0xcc, size);
	memcpy(ptr->user_addr + size, end_magic, sizeof(end_magic));
	ptr->next = NULL;
	ptr->flag = 0;
	ptr->malloc_time = time(NULL);
	ptr->size = size;
	generate_traceback(ptr->where_alloced);
	//	void* pointer = (char*)ptr + sizeof(new_ptr_list_t);
	size_t hash_index = DEBUG_NEW_HASH((ptr->user_addr));
	assert(hash_index>=0 && hash_index<DEBUG_NEW_HASHTABLESIZE);
	malloc_mutex[hash_index].Lock();
	ptr->next = new_ptr_list[hash_index];
	new_ptr_list[hash_index] = ptr;
	malloc_mutex[hash_index].Unlock();
	return ptr->user_addr;
}
void check_addr(void *pointer) {
	if (pointer == NULL) {
		abort();
		return;
	}
	size_t hash_index = DEBUG_NEW_HASH(pointer);
	assert(hash_index>=0 && hash_index<DEBUG_NEW_HASHTABLESIZE);
	malloc_mutex[hash_index].Lock();
	new_ptr_list_t* ptr = new_ptr_list[hash_index];
	while (ptr) {
		if (ptr->user_addr == pointer) {
			if (check_ptr(ptr)) {
				malloc_mutex[hash_index].Unlock();
				check_all_ptr();
				return;
			}
			break;
		}
		ptr = ptr->next;
	}
	malloc_mutex[hash_index].Unlock();

}
void my_free(void *pointer) {
	size_t hash_index = DEBUG_NEW_HASH(pointer);
	assert(hash_index>=0 && hash_index<DEBUG_NEW_HASHTABLESIZE);
	malloc_mutex[hash_index].Lock();
	new_ptr_list_t* ptr = new_ptr_list[hash_index];
	new_ptr_list_t* ptr_last = NULL;
	while (ptr) {
		if (ptr->user_addr == pointer) {
			if (ptr_last == NULL) {
				new_ptr_list[hash_index] = ptr->next;
			} else {
				ptr_last->next = ptr->next;
			}
			if (check_ptr(ptr)) {
				malloc_mutex[hash_index].Unlock();
				check_all_ptr();
			} else {
				malloc_mutex[hash_index].Unlock();
			}
			memset(ptr->user_addr, 0xee, ptr->size + sizeof(start_magic));
			__libc_free(ptr->user_addr - sizeof(start_magic));
			__libc_free(ptr);
			return;
		}
		ptr_last = ptr;
		ptr = ptr->next;
	}
	malloc_mutex[hash_index].Unlock();
	__libc_free(pointer);
	//	fprintf(stderr, "free: invalid pointer %p\n", pointer);
	//abort();
}
/*
char * xstrdup2(const char *s, const char * file, int line) {
	int len = strlen(s);
	char *tmp = (char *) malloc(len + 1);
	strcpy(tmp, s);
	return tmp;
}
void *operator new(size_t m_size, const char *file, int line) {
	return xmalloc2(m_size, file, line);
}
void operator delete(void *p) {
	xfree2(p, "delete", 0);
}
void * operator new[](size_t m_size, const char *file, int line) {
	return xmalloc2(m_size, file, line);
}
void operator delete[](void *p) {
	xfree2(p, "delete[]", 0);
}

void* operator new(size_t size, const std::nothrow_t&) throw () {
	return operator new(size);
}
 void* operator new[](size_t size, const std::nothrow_t&) throw()
 {
 return operator new[](size);
 }
 */
#ifdef MALLOC_CHECK_END
static void disclaimer(void) {
	static char message[] =
	"This appears to be a non-malloc bug, dumping core\n";
	write(2, message, sizeof(message));
	signal(SIGSEGV, SIG_DFL);
	return;
}
new_ptr_list_t * find_block_by_any_addr(char *addr)
{
	for (int i = 0; i < DEBUG_NEW_HASHTABLESIZE; ++i)
	{
		new_ptr_list_t* ptr = new_ptr_list[i];
		if (ptr == NULL)
		continue;
		while (ptr)
		{
			if ((addr >= ptr->real_addr) && (addr < (ptr->real_addr + ptr->real_size)))
			return ptr;
			ptr=ptr->next;
		}
	}
	return NULL;
}
static void handle_page_fault(char *address, int write) {
	new_ptr_list_t *ptr=find_block_by_any_addr(address);
	fprintf(stderr, "Tried to %s address " POINTER_FORMAT "\n",(write) ? "write" : "read", address);
	if(ptr) {
		fprintf(stderr,"The address infomation %s:%d,size=%d\n",ptr->file,ptr->line,ptr->size);
	}
}
#endif
static void sigabort_handler(int signum) {
	signal(SIGABRT, SIG_DFL);
	check_all_ptr();
}
#ifdef MALLOC_CHECK_END
static void sigsegv_handler(int signum ,struct sigcontext ctx) {
#else
static void sigsegv_handler(int signum) {
#endif
#ifdef MALLOC_CHECK_END
	(void)signum; /* shut the compiler up */
	/* Find out if this is a fault we're trying to catch */
	if (ctx.trapno != 14 /* Not a page fault */
			|| !(ctx.err & 4)) /* Not a user access */
	{
		disclaimer();
		return;
	}
	/* Note that using cr2 here assumes that the base of DS is 0.
	 Thank God for systems with real memory mapping... */
	handle_page_fault((char *)ctx.cr2, ctx.err & 2);
	/*been fixed up */
#endif
	signal(SIGABRT, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
	check_all_ptr();
}
void * check_memory_thread(void* arg) {
	for (;;) {
		sleep(5);
		check_all_ptr();
	}
	return NULL;
}
static void startup() {
	signal(SIGSEGV, (void(*)(int)) sigsegv_handler);
	signal(SIGABRT, (void(*)(int)) sigabort_handler);
}
class new_check_t {
public:
	new_check_t() {
		::startup();
	}
	~new_check_t() {
		//dump_memory(0,-1);
	}
};
static new_check_t new_check_object;
#endif
#endif
