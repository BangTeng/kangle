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
#include <unistd.h>
//#define MALLOC_CHECK_END 1
#include<syslog.h>

#include<sys/mman.h>
#include<signal.h>
//#include<malloc.h>
#include<time.h>
#include<pthread.h>
#include "trace.h"
using namespace std;
#define MEMORY_BAD_END		1
#define MEMORY_BAD_FRONT	2

static char start_magic[] = { 0x12, 0x56, 0xdc, 0x78, 0xef, 0x34, 0x45, 0xc3 };
static char end_magic[] = { 0xc3, 0x45, 0x34, 0xef, 0x78, 0xdc, 0x56, 0x12 };

#ifndef DEBUG_NEW_HASHTABLESIZE
#define DEBUG_NEW_HASHTABLESIZE 16384
#endif
#ifndef DEBUG_NEW_HASH
#define DEBUG_NEW_HASH(p) (((unsigned long)(p) >> 8) % DEBUG_NEW_HASHTABLESIZE)
#endif

class new_ptr_list_t {
public:
	new_ptr_list_t() {
		next = NULL;
	}
	;
	TRACEBACK where_alloced;
	size_t size;
	char *user_addr;
	new_ptr_list_t *next;
};
static new_ptr_list_t *new_ptr_list[DEBUG_NEW_HASHTABLESIZE];
static new_ptr_list_t *delay_freed1[DEBUG_NEW_HASHTABLESIZE];
static new_ptr_list_t *delay_freed2[DEBUG_NEW_HASHTABLESIZE];
//static KMutex malloc_mutex(&attr,PTHREAD_MUTEX_RECURSIVE)[DEBUG_NEW_HASHTABLESIZE+1];
static KMutex malloc_mutex[DEBUG_NEW_HASHTABLESIZE];
static bool malloc_started = false;

extern "C" {
        extern void *__libc_malloc(size_t size);
        extern void *__libc_realloc(void *p,size_t size);
        extern void __libc_free(void *p);
}
inline void free_ptr(new_ptr_list_t *ptr)
{
	for (size_t i=0;i<ptr->size;i++) {
		if((unsigned char)ptr->user_addr[i]!=0xee){
			abort();
		}
	}
	 __libc_free(ptr->user_addr - sizeof(start_magic));
	__libc_free(ptr);

}
void check_ptr(new_ptr_list_t *ptr) {
	if(memcmp(ptr->user_addr - sizeof(start_magic), start_magic,sizeof(start_magic)) != 0){
		abort();
	}
	if(memcmp(ptr->user_addr + ptr->size, end_magic, sizeof(end_magic)) != 0){
		abort();
	}
}
void *memory_debug_thread(void* arg) {
	for (;;) {
		//printf("check delay freed list\n");
		sleep(5);
		for (int i=0;i<DEBUG_NEW_HASHTABLESIZE;i++) {
			malloc_mutex[i].Lock();
			new_ptr_list_t *ptr = delay_freed1[i];
			while (ptr) {
				check_ptr(ptr);
				new_ptr_list_t *next = ptr->next;
				free_ptr(ptr);
				ptr = next;
			}
			delay_freed1[i] = delay_freed2[i];
			delay_freed2[i] = NULL;
			malloc_mutex[i].Unlock();
		}	
	}
	return NULL;
}
void start_hook_alloc()
{
	memset(new_ptr_list,0,sizeof(new_ptr_list_t *)*DEBUG_NEW_HASHTABLESIZE);
	memset(delay_freed1,0,sizeof(new_ptr_list_t *)*DEBUG_NEW_HASHTABLESIZE);
	memset(delay_freed2,0,sizeof(new_ptr_list_t *)*DEBUG_NEW_HASHTABLESIZE);
	TRACEBACK where_alloced;
	generate_traceback(where_alloced);
	pthread_t id;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&id, &attr,memory_debug_thread, NULL);
	malloc_started = true;	
}

void dump_memory(int min_time,int max_time) {
	unsigned leakSize = 0;
	unsigned leakCount = 0;
	char buf[512];
	int buf_len;
	for (int i = 0; i < DEBUG_NEW_HASHTABLESIZE; ++i) {
		malloc_mutex[i].Lock();
		new_ptr_list_t* ptr = new_ptr_list[i];
		while (ptr) {
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
			syslog(
					LOG_NOTICE,
					"dump memory at %p (size %u,stack: %s)\n",
					ptr->user_addr + sizeof(start_magic), unsigned(ptr->size),
					buf);
			leakSize += ptr->size;
			leakCount++;
			ptr = ptr->next;
		}
		malloc_mutex[i].Unlock();
	}
	syslog(LOG_NOTICE, "Total leaked size=%d,count=%d\n", leakSize, leakCount);

}

void list_all_malloc() {
	dump_memory(0,-1);
}
void *kgl_memalign(size_t alignment, size_t size)
{
	void  *p;
        int    err;
	err = posix_memalign(&p, alignment, size);
	if (err) {
		return NULL;
	}
	return p;
}
void *realloc(void *old_ptr,size_t size) {
	if (!malloc_started) {
		if (old_ptr==NULL) {
			return __libc_malloc(size);
		}
		return __libc_realloc(old_ptr,size);
	}
	if (size==0) {
		free(old_ptr);
		return NULL;
	}
	new_ptr_list_t* ptr = (new_ptr_list_t*) __libc_malloc(sizeof(new_ptr_list_t));
	if (ptr == NULL) {
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
	ptr->size = size;
	generate_traceback(ptr->where_alloced);
	if (old_ptr) {
		size_t old_hash = DEBUG_NEW_HASH(old_ptr);
		bool old_finded = false;
		malloc_mutex[old_hash].Lock();
		new_ptr_list_t* ptr_head = new_ptr_list[old_hash];
		new_ptr_list_t* ptr_last = NULL;
		while (ptr_head) {
			if (ptr_head->user_addr != old_ptr) {
				ptr_last = ptr_head;
				ptr_head = ptr_head->next;
				continue;
			}
			if (ptr_last == NULL) {
				new_ptr_list[old_hash] = ptr_head->next;
			} else {
				ptr_last->next = ptr_head->next;
			}
			check_ptr(ptr_head);
			ptr_head->next = delay_freed2[old_hash];
			delay_freed2[old_hash] = ptr_head;
			size_t copy_size = ptr_head->size<size?ptr_head->size:size;
			memcpy(ptr->user_addr,old_ptr,copy_size);
			memset(ptr_head->user_addr, 0xee, ptr_head->size);
			old_finded = true;
			break;
		}
		malloc_mutex[old_hash].Unlock();
		if (!old_finded) {
			__libc_free(real_addr);
			__libc_free(ptr);
			return __libc_realloc(old_ptr,size);
		}
	}
	size_t hash_index = DEBUG_NEW_HASH((ptr->user_addr));
	assert(hash_index>=0 && hash_index<DEBUG_NEW_HASHTABLESIZE);
	malloc_mutex[hash_index].Lock();
	ptr->next = new_ptr_list[hash_index];
	new_ptr_list[hash_index] = ptr;
	malloc_mutex[hash_index].Unlock();
	return ptr->user_addr;
}
void free(void *pointer) {
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
			check_ptr(ptr);
			ptr->next = delay_freed2[hash_index];
			delay_freed2[hash_index] = ptr;
			memset(ptr->user_addr, 0xee, ptr->size);
			malloc_mutex[hash_index].Unlock();
			return;
		}
		ptr_last = ptr;
		ptr = ptr->next;
	}
	malloc_mutex[hash_index].Unlock();
	__libc_free(pointer);
}
void *calloc(size_t n, size_t size)
{
	size *= n;
	void *ptr = malloc(size);
	if (ptr) {
		memset(ptr,0,size);
	}
	return ptr;
}
void *malloc(size_t size)
{
	return realloc(NULL,size);
}
#endif
#endif
