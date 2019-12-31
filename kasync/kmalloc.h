#ifndef MSOCKET_MALLOCDEBUG_H
#define MSOCKET_MALLOCDEBUG_H
#include "kfeature.h"
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
KBEGIN_DECLS
#define xmemory_new(T) (T *)xmalloc(sizeof(T))
#define kgl_memcpy memmove
#define xfree	 free
#define xmalloc	 malloc
#define xstrdup  strdup
void start_hook_alloc();
int dump_memory_leak(int min_time, int max_time);
#if defined(_WIN32) && !defined(NDEBUG)
#define kassert(result) do {if (!(result)) abort();}while(0)
#else
#define kassert assert
#endif


#define kgl_align_floor(d, a)     ((d) & ~(a - 1))
#define kgl_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define kgl_align_ptr(p, a) \
     (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

#ifdef _WIN32
#define kgl_memalign(a,b)	_aligned_malloc(b,a)
#define kgl_align_free		_aligned_free
#else
#define kgl_align_free free
#if MALLOCDEBUG && !ENABLE_JEMALLOC
extern void *kgl_memalign(size_t alignment, size_t size);
#else
INLINE void *kgl_memalign(size_t alignment, size_t size) {
	void  *p;
	int    err;
	err = posix_memalign(&p, alignment, size);
	if (err) {
		return NULL;
	}
	return p;
}
#endif
#endif

typedef struct kgl_pool_s kgl_pool_t;
typedef struct kgl_pool_cleanup_s kgl_pool_cleanup_t;
typedef struct kgl_pool_large_s kgl_pool_large_t;

typedef void (*kgl_pool_cleanup_pt) (void *data);
struct kgl_pool_cleanup_s {
	kgl_pool_cleanup_pt handler;
	void *data;
	kgl_pool_cleanup_t *next;
};
typedef struct {
	char               *last;
	char               *end;
	kgl_pool_t          *next;
	unsigned          failed;
} kgl_pool_data_t;

struct kgl_pool_large_s {
	kgl_pool_large_t     *next;
	void                 *alloc;
};
struct kgl_pool_s {
	kgl_pool_data_t       d;
	size_t                max;
	kgl_pool_t           *current;
	kgl_pool_large_t     *large;
	kgl_pool_cleanup_t   *cleanup;
};

extern unsigned kgl_pagesize;
#define KGL_MAX_ALLOC_FROM_POOL  (kgl_pagesize - 1)
#define KGL_DEFAULT_POOL_SIZE    (16 * 1024)

#define KGL_POOL_ALIGNMENT       16
#define KGL_MIN_POOL_SIZE                                                     \
    kgl_align((sizeof(kgl_pool_t) + 2 * sizeof(kgl_pool_large_t)),            \
              KGL_POOL_ALIGNMENT)


kgl_pool_t *kgl_create_pool(size_t size);
void kgl_destroy_pool(kgl_pool_t *pool);
//指针对齐分配内存
void *kgl_palloc(kgl_pool_t *pool, size_t size);
//不对齐分配内存
void *kgl_pnalloc(kgl_pool_t *pool, size_t size);
bool kgl_pfree(kgl_pool_t *pool, void *p);
kgl_pool_cleanup_t *kgl_pool_cleanup_add(kgl_pool_t *pool, size_t size);

typedef struct {
	void        *elts;
	size_t      nelts;
	size_t      size;
	size_t		nalloc;
	kgl_pool_t  *pool;
} kgl_array_t;

kgl_array_t *kgl_array_create(kgl_pool_t *p, size_t n, size_t size);
void kgl_array_destroy(kgl_array_t *a);
void *kgl_array_push(kgl_array_t *a);
void *kgl_array_push_n(kgl_array_t *a, size_t n);


INLINE bool kgl_array_init(kgl_array_t *array, kgl_pool_t *pool, size_t n, size_t size)
{

	array->nelts = 0;
	array->size = size;
	array->nalloc = n;
	array->pool = pool;

	array->elts = kgl_palloc(pool, n * size);
	return array->elts != NULL;
}
KEND_DECLS
#endif
