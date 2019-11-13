#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include <assert.h>
#include "kfeature.h"
#include "ksync.h"
//#include "log.h"
//#include "malloc_debug.h"
#ifdef MALLOCDEBUG
#ifndef _WIN32
#ifdef ENABLE_JEMALLOC
#include <jemalloc/jemalloc.h>
void start_hook_alloc() {
	bool active = true;
        int ret = mallctl("prof.active", NULL,NULL,&active,sizeof(bool));
	printf("active result=[%d]\n",ret);

}
void dump_memory_leak(int min,int max) {
	int ret = mallctl("prof.dump",NULL,NULL,NULL,0);
	printf("dump result=[%d]\n",ret);
}
#else
#include <unistd.h>
//#define MALLOC_CHECK_END 1
#include<syslog.h>

#include<sys/mman.h>
#include<signal.h>
//#include<malloc.h>
#include<time.h>
#include<pthread.h>
#include<execinfo.h>
typedef void * addr;
#define MAX_TRACEBACK_LEVELS 32
typedef addr TRACEBACK[MAX_TRACEBACK_LEVELS];
INLINE int generate_traceback(TRACEBACK tb) {
	return backtrace(tb,MAX_TRACEBACK_LEVELS);
}
#define MEMORY_BAD_END		1
#define MEMORY_BAD_FRONT	2

static unsigned char start_magic[] = { 0x12, 0x56, 0xdc, 0x78, 0xef, 0x34, 0x45, 0xc3 };
static unsigned char end_magic[] = { 0xc3, 0x45, 0x34, 0xef, 0x78, 0xdc, 0x56, 0x12 };

#ifndef DEBUG_NEW_HASHTABLESIZE
#define DEBUG_NEW_HASHTABLESIZE 16384
#endif
#ifndef DEBUG_NEW_HASH
#define DEBUG_NEW_HASH(p) (((unsigned long)(p) >> 8) % DEBUG_NEW_HASHTABLESIZE)
#endif
typedef struct new_ptr_list_s new_ptr_list_t;
struct new_ptr_list_s {
	TRACEBACK where_alloced;
	size_t size;
	char *user_addr;
	new_ptr_list_t *next;
};
static new_ptr_list_t *new_ptr_list[DEBUG_NEW_HASHTABLESIZE];
#ifdef ENABLE_FREE_MEMORY_CHECK
static new_ptr_list_t *delay_freed1[DEBUG_NEW_HASHTABLESIZE];
static new_ptr_list_t *delay_freed2[DEBUG_NEW_HASHTABLESIZE];
#endif
//static KMutex malloc_mutex(&attr,PTHREAD_MUTEX_RECURSIVE)[DEBUG_NEW_HASHTABLESIZE+1];
static kmutex malloc_mutex[DEBUG_NEW_HASHTABLESIZE];
static bool malloc_started = false;
KBEGIN_DECLS
extern void *__libc_malloc(size_t size);
extern void *__libc_realloc(void *p,size_t size);
extern void __libc_free(void *p);
KEND_DECLS
static INLINE void free_ptr(new_ptr_list_t *ptr)
{
#ifdef ENABLE_FREE_MEMORY_CHECK
	for (size_t i=0;i<ptr->size;i++) {
		if((unsigned char)ptr->user_addr[i]!=0xee){
			abort();
		}
	}
#endif
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
#ifdef ENABLE_FREE_MEMORY_CHECK
void *memory_debug_thread(void* arg) {
	for (;;) {
		sleep(5);
		for (int i=0;i<DEBUG_NEW_HASHTABLESIZE;i++) {
			kmutex_lock(&malloc_mutex[i]);
			new_ptr_list_t *ptr = delay_freed1[i];
			while (ptr) {
				check_ptr(ptr);
				new_ptr_list_t *next = ptr->next;
				free_ptr(ptr);
				ptr = next;
			}
			delay_freed1[i] = delay_freed2[i];
			delay_freed2[i] = NULL;
			kmutex_unlock(&malloc_mutex[i]);
		}	
	}
	return NULL;
}
#endif
void start_hook_alloc()
{
	memset(new_ptr_list,0,sizeof(new_ptr_list_t *)*DEBUG_NEW_HASHTABLESIZE);
#ifdef ENABLE_FREE_MEMORY_CHECK
	memset(delay_freed1,0,sizeof(new_ptr_list_t *)*DEBUG_NEW_HASHTABLESIZE);
	memset(delay_freed2,0,sizeof(new_ptr_list_t *)*DEBUG_NEW_HASHTABLESIZE);
#endif
	//generate_traceback
	TRACEBACK where_alloced;
        generate_traceback(where_alloced);
	for (int i=0;i<DEBUG_NEW_HASHTABLESIZE;i++) {
		kmutex_init(&malloc_mutex[i],NULL);
	}
#ifdef ENABLE_FREE_MEMORY_CHECK
	pthread_t id;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&id, &attr,memory_debug_thread, NULL);
#endif
	malloc_started = true;	
}
int dump_memory_leak(int min_time,int max_time) {
	unsigned leakSize = 0;
	unsigned leakCount = 0;
	char buf[512];
	char **strings = NULL;
	memset(buf,0,sizeof(buf));
	snprintf(buf, sizeof(buf) - 1, "/tmp/leak.%d", getpid());
	bool saved_malloc_started = malloc_started;
	malloc_started = false;
	FILE *fp = fopen(buf, "wt");
	if (fp == NULL) {
		syslog(LOG_NOTICE, "cann't open file for write.");
		malloc_started = saved_malloc_started;
		return -1;
	}
	for (int i = 0; i < DEBUG_NEW_HASHTABLESIZE; ++i) {
		kmutex_lock(&malloc_mutex[i]);
		new_ptr_list_t* ptr = new_ptr_list[i];
		kmutex_unlock(&malloc_mutex[i]);
		while (ptr) {
			fprintf(fp,
					"--ptr=%p size=%u stack=\n",
					ptr->user_addr,
					(unsigned)ptr->size);
			strings = backtrace_symbols(ptr->where_alloced, MAX_TRACEBACK_LEVELS);
			for (int j = 0; j < MAX_TRACEBACK_LEVELS; j++) {
				if (ptr->where_alloced[j]==0 || strings[j]==NULL) {
					break;
				}
				fprintf(fp, "\t%s\n", strings[j]);
			}
			free(strings);
			leakSize += ptr->size;
			leakCount++;
			kmutex_lock(&malloc_mutex[i]);
			ptr = ptr->next;
			kmutex_unlock(&malloc_mutex[i]);
		}
	}
	fprintf(fp,"Total leaked size=%d,count=%d\n", leakSize, leakCount);
	fclose(fp);
	if (leakCount==0) {
		unlink(buf);
	} else {
		printf("dump file [%s]\n",buf);
	}
	malloc_started = saved_malloc_started;
	return leakCount;
}

void list_all_malloc() {
	dump_memory_leak(0,-1);
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
		kmutex_lock(&malloc_mutex[old_hash]);
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
			size_t copy_size = ptr_head->size<size?ptr_head->size:size;
			memcpy(ptr->user_addr,old_ptr,copy_size);
#ifdef ENABLE_FREE_MEMORY_CHECK
			ptr_head->next = delay_freed2[old_hash];
			delay_freed2[old_hash] = ptr_head;
			memset(ptr_head->user_addr, 0xee, ptr_head->size);
#else
			free_ptr(ptr_head);
#endif
			old_finded = true;
			break;
		}
		kmutex_unlock(&malloc_mutex[old_hash]);
		if (!old_finded) {
			__libc_free(real_addr);
			__libc_free(ptr);
			return __libc_realloc(old_ptr,size);
		}
	}
	size_t hash_index = DEBUG_NEW_HASH((ptr->user_addr));
	assert(hash_index>=0 && hash_index<DEBUG_NEW_HASHTABLESIZE);
	kmutex_lock(&malloc_mutex[hash_index]);
	ptr->next = new_ptr_list[hash_index];
	new_ptr_list[hash_index] = ptr;
	kmutex_unlock(&malloc_mutex[hash_index]);
	return ptr->user_addr;
}
void free(void *pointer) {
	size_t hash_index = DEBUG_NEW_HASH(pointer);
	assert(hash_index>=0 && hash_index<DEBUG_NEW_HASHTABLESIZE);
	kmutex_lock(&malloc_mutex[hash_index]);
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
#ifdef ENABLE_FREE_MEMORY_CHECK
			ptr->next = delay_freed2[hash_index];
			delay_freed2[hash_index] = ptr;
			memset(ptr->user_addr, 0xee, ptr->size);
#else
			free_ptr(ptr);
#endif
			kmutex_unlock(&malloc_mutex[hash_index]);
			return;
		}
		ptr_last = ptr;
		ptr = ptr->next;
	}
	kmutex_unlock(&malloc_mutex[hash_index]);
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
#endif

