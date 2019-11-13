#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "kfeature.h"
#ifdef _WIN32
#include "kforwin32.h"
#include "krbtree.h"
#include <dbghelp.h>
#include <crtdbg.h>
#include "ksync.h"
#include "kselector.h"
#ifdef MALLOCDEBUG
#pragma comment(lib,"Dbghelp.lib")
#define SKIP_STACK_TRACE_COUNT  7
#define MAX_TRACEBACK_LEVELS	16
#define MALLOC_CHECK_END		1
#define MEMORY_BAD_END			1
#define MEMORY_BAD_FRONT		2
#define nNoMansLandSize			4

typedef void * addr;
typedef addr TRACEBACK[MAX_TRACEBACK_LEVELS];

static HANDLE hProcess = NULL;
static int bad_memory_count = 0;
#ifndef DEBUG_NEW_HASHTABLESIZE
#define DEBUG_NEW_HASHTABLESIZE 16384
#define DEBUG_NEW_HASHTABLE     16383
#endif
#ifndef DEBUG_NEW_HASH
#define DEBUG_NEW_HASH(p) ((unsigned long)(p & DEBUG_NEW_HASHTABLE))
#endif
static kmutex stackLock;

static void *inter_malloc(int size)
{
	return _malloc_dbg(size, _CRT_BLOCK, NULL, 0);
}
static void inter_free(void *ptr)
{
	_free_dbg(ptr, _CRT_BLOCK);
}
#ifdef _WIN64

typedef struct
{
	long lRequest;
	unsigned char gap[nNoMansLandSize];
} _CrtMemBlockHeader;
#ifndef ADDRESS
#define ADDRESS    DWORD64
#endif

#elif _WIN32
#define ADDRESS    DWORD
typedef struct _CrtMemBlockHeader
{
	// Pointer to the block allocated just before this one:
	struct _CrtMemBlockHeader *pBlockHeaderNext;
	// Pointer to the block allocated just after this one:
	struct _CrtMemBlockHeader *pBlockHeaderPrev;
	char *szFileName;    // File name
	int nLine;           // Line number
	size_t nDataSize;    // Size of user block
	int nBlockUse;       // Type of block
	long lRequest;       // Allocation number
 // Buffer just before (lower than) the user's memory:
	unsigned char gap[nNoMansLandSize];
} _CrtMemBlockHeader;
#endif

typedef struct {
	long request;
	TRACEBACK where_alloced;
	size_t size;
	time_t malloc_time;
} new_ptr_list_t;
static struct krb_root memory_pool[DEBUG_NEW_HASHTABLESIZE];

static kmutex malloc_mutex[DEBUG_NEW_HASHTABLESIZE];
static long ptr_list_cmp(void *k1, void *k2) {
	new_ptr_list_t *key1 = (new_ptr_list_t *)k1;
	new_ptr_list_t *key2 = (new_ptr_list_t *)k2;
	return (key1->request - key2->request);
}
static void strapp(char **dest, char *source)
{
	int  length;
	char   *temp;
	temp = *dest;
	length = (int)(strlen(*dest) + strlen(source));
	*dest = (char *)inter_malloc(length + 1);
	strncpy(*dest, temp, length);
	strncat(*dest, source, length);
	inter_free(temp);
}

static char * buildsymbolsearchpath()
{
	char    *env;
	size_t   index;
	size_t   length;
	HMODULE  module;
	char    *path = (char *)inter_malloc(MAX_PATH);
	size_t   pos = 0;


	module = GetModuleHandle(NULL);
	GetModuleFileNameA(module, path, MAX_PATH);
	char *p = strrchr(path, '\\');
	if (p) {
		*(p + 1) = '\0';
	}
	strapp(&path, ";.\\");

	// Append %SYSTEMROOT% and %SYSTEMROOT%\system32.
	env = getenv("SYSTEMROOT");
	if (env) {
		strapp(&path, ";");
		strapp(&path, env);
		strapp(&path, ";");
		strapp(&path, env);
		strapp(&path, "\\system32");
	}

	// Append %_NT_SYMBOL_PATH% and %_NT_ALT_SYMBOL_PATH%.
	env = getenv("_NT_SYMBOL_PATH");
	if (env) {
		strapp(&path, ";");
		strapp(&path, env);
	}
	env = getenv("_NT_ALT_SYMBOL_PATH");
	if (env) {
		strapp(&path, ";");
		strapp(&path, env);
	}

	// Remove any quotes from the path. The symbol handler doesn't like them.
	pos = 0;
	length = strlen(path);
	while (pos < length) {
		if (path[pos] == '\"') {
			for (index = pos; index < length; index++) {
				path[index] = path[index + 1];
			}
		}
		pos++;
	}

	return path;
}
int dump_memory_leak(int min_time, int max_time) 
{

	unsigned leakSize = 0;
	unsigned leakCount = 0;
	time_t now_time = time(NULL);
	char buf[512];
	int buf_len;
	_CrtSetAllocHook(NULL);
	GetModuleFileName(NULL, buf, sizeof(buf) - 1);
	char buf2[512];
	memset(buf2, 0, sizeof(buf2));
	snprintf(buf2, sizeof(buf2) - 1, "%s.%d.leak", buf, GetCurrentProcessId());
	FILE *fp = fopen(buf2, "wt");
	if (fp == NULL) {
		//LogEvent("cann't open file [%s] to write\n",buf2);
		return -1;
	}
	//LogEvent("dump_memory to [%s] min_time=[%d],max_time=[%d]\n", s->str().c_str(),min_time, max_time);
	DWORD				displacement;
	IMAGEHLP_LINE     sourceinfo;
	char *symbolpath = buildsymbolsearchpath();
	SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
	SymInitialize(hProcess, symbolpath, TRUE);
	inter_free(symbolpath);

	for (int i = 0; i < DEBUG_NEW_HASHTABLESIZE; ++i) {
		kmutex_lock(&malloc_mutex[i]);
		struct krb_node *node = rb_first(&memory_pool[i]);
		while (node) {
			new_ptr_list_t* ptr = (new_ptr_list_t *)(node->data);
			kassert(ptr);
			int ptr_past_time = (int)(now_time - ptr->malloc_time);
			if (ptr_past_time >= min_time && (max_time < 0 || ptr_past_time <= max_time)) {
				fprintf(fp,
					"-- id=%d ptr=%u size=%u alloc_time=%I64x past_time=%d stack=\n",
					i,
					ptr->request,
					(int)ptr->size,
					ptr->malloc_time,
					(int)(now_time - ptr->malloc_time)
				);
				buf_len = 0;
				for (int j = 0; j < MAX_TRACEBACK_LEVELS; j++) {
					if (ptr->where_alloced[j] == 0) {
						break;
					}
					fprintf(fp, "\t{%p}", ptr->where_alloced[j]);
					if (SymGetLineFromAddr(hProcess, (DWORD64)ptr->where_alloced[j], &displacement, &sourceinfo)) {
						fprintf(fp, " %s:%d", sourceinfo.FileName, sourceinfo.LineNumber);
					}
					fprintf(fp, "\n");
				}
				//fprintf(fp,"\r\n");
				leakSize += (int)ptr->size;
				leakCount++;
			}
			node = rb_next(node);
			//ptr = ptr->next;
		}
		kmutex_unlock(&malloc_mutex[i]);
	}
	fprintf(fp, "** total_size=%d count=%d\r\n", leakSize, leakCount);
	fclose(fp);
	if (leakCount == 0) {
		unlink(buf2);
	}
	return leakCount;
}

void list_all_malloc() {
	dump_memory_leak(0, -1);
}

static void getstacktrace(new_ptr_list_t* ptr)
{
	CaptureStackBackTrace(SKIP_STACK_TRACE_COUNT, MAX_TRACEBACK_LEVELS, ptr->where_alloced, NULL);
	return;
}
static bool ptr_list_remove(struct krb_root *root,long request)
{
	struct krb_node *n = root->rb_node;
	new_ptr_list_t *ptr = NULL;
	while (n) {
		ptr = (new_ptr_list_t *)(n->data);
		kassert(ptr);
		long result = request - ptr->request;
		if (result < 0) {
			n = (n->rb_left);
		} else if (result > 0) {
			n = (n->rb_right);
		} else {
			rb_erase(n, root);
			inter_free(ptr);
			inter_free(n);
			return true;
		}
	}
	return false;	
}
static void ptr_list_insert(struct krb_root *root, void *key)
{
	struct krb_node **n = &(root->rb_node), *parent = NULL;
	while (*n) {
		long result = ptr_list_cmp(key, (*n)->data);
		parent = *n;
		if (result < 0) {
			n = &((*n)->rb_left);
		} else if (result > 0) {
			n = &((*n)->rb_right);
		} else {
			kassert(false);
		}
	}
	struct krb_node *node = (struct krb_node *)inter_malloc(sizeof(struct krb_node));
	node->data = key;
	rb_link_node(node, parent, n);
	rb_insert_color(node, root);
	return;
}
void win32_free_hook(void *pvData)
{
	_CrtMemBlockHeader *bh = (_CrtMemBlockHeader *)((char *)pvData - sizeof(_CrtMemBlockHeader));
	size_t hash_index = DEBUG_NEW_HASH(bh->lRequest);
	kassert(hash_index >= 0 && hash_index < DEBUG_NEW_HASHTABLESIZE);
	kmutex_lock(&malloc_mutex[hash_index]);
	if (!ptr_list_remove(&memory_pool[hash_index], bh->lRequest)) {
		//printf("ptr remove error request=[%u] hash_index=[%d].\n", bh->lRequest,hash_index);
	}
	kmutex_unlock(&malloc_mutex[hash_index]);
}
void win32_malloc_hook(long request, size_t size, const char *file, int line)
{
	new_ptr_list_t *ptr = (new_ptr_list_t*)inter_malloc(sizeof(new_ptr_list_t));
	if (ptr == NULL) {
		abort();
		return;
	}
	memset(ptr, 0, sizeof(new_ptr_list_t));
	ptr->request = request;
	ptr->size = size;
	ptr->malloc_time = kgl_current_sec;
	getstacktrace(ptr);
	int new_flag = 0;
	size_t hash_index = DEBUG_NEW_HASH(request);
	kassert(hash_index >= 0 && hash_index < DEBUG_NEW_HASHTABLESIZE);
	kmutex_lock(&malloc_mutex[hash_index]);
	ptr_list_insert(&memory_pool[hash_index], ptr);
	kmutex_unlock(&malloc_mutex[hash_index]);
}
static void win32_realloc_hook(void *ptr, long request, size_t size, const char *file, int line)
{
	win32_free_hook(ptr);
	win32_malloc_hook(request, size, file, line);
}
static int xallochook(int nAllocType, void *pvData,size_t nSize, int nBlockUse, long lRequest,const unsigned char * szFileName, int nLine)
{

	if (nBlockUse == _CRT_BLOCK) {
		return TRUE;
	}
	switch (nAllocType) {
	case _HOOK_ALLOC:
		win32_malloc_hook(lRequest, nSize, (const char *)szFileName, nLine);
		break;
	case _HOOK_FREE:
		win32_free_hook(pvData);
		break;

	case _HOOK_REALLOC:
		win32_realloc_hook(pvData, lRequest, nSize, (const char *)szFileName, nLine);
		break;
	}
	return TRUE;
}
void start_hook_alloc() {
	hProcess = GetCurrentProcess();
	for (int i = 0; i < DEBUG_NEW_HASHTABLESIZE; i++) {
		memory_pool[i].rb_node = NULL;
		kmutex_init(&malloc_mutex[i],NULL);
	}
	_CrtSetAllocHook(xallochook);
}
#endif
#endif
