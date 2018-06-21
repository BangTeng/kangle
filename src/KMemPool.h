#ifndef KMEMPOOL_H
#define KMEMPOOL_H
#include "global.h"
#include "forwin32.h"
#include "ksapi.h"

#include <stdlib.h>
#define kgl_align_floor(d, a)     ((d) & ~(a - 1))

#define kgl_align(d, a)     (((d) + (a - 1)) & ~(a - 1))
#define kgl_align_ptr(p, a)                                                   \
     (((uintptr_t) (p) + ((uintptr_t) a - 1)) & ~((uintptr_t) a - 1))

#ifdef _WIN32
#define kgl_memalign(a,b)	_aligned_malloc(b,a)
#define kgl_align_free		_aligned_free
#else
#define kgl_align_free free
#ifdef MALLOCDEBUG
extern void *kgl_memalign(size_t alignment, size_t size);
#else
inline void *kgl_memalign(size_t alignment, size_t size) {
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
struct kgl_pool_t;
typedef void (WINAPI * kgl_pool_cleanup_pt) (void *data);
struct kgl_pool_cleanup_t {
	kgl_pool_cleanup_pt handler;
	void *data;
	kgl_pool_cleanup_t *next;
};
struct kgl_pool_data_t {
	char               *last;
	char               *end;
	kgl_pool_t          *next;
	unsigned          failed;
};

struct kgl_pool_large_t {
	kgl_pool_large_t     *next;
	void                 *alloc;
};
struct kgl_pool_t {
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


inline bool kgl_array_init(kgl_array_t *array, kgl_pool_t *pool, size_t n, size_t size)
{

	array->nelts = 0;
	array->size = size;
	array->nalloc = n;
	array->pool = pool;

	array->elts = kgl_palloc(pool, n * size);
	if (array->elts == NULL) {
		return false;
	}

	return true;
}


#endif
