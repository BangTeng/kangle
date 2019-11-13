#ifndef KSYNC_H_99
#define KSYNC_H_99
#ifndef _WIN32
#include <pthread.h>
#endif
#include <stdlib.h>
#include <string.h>
#include "kforwin32.h"
#include "katom.h"
#include "kfeature.h"
#include "kmalloc.h"
KBEGIN_DECLS
typedef pthread_mutex_t kmutex;
#define kmutex_init pthread_mutex_init
#define kmutex_lock pthread_mutex_lock
#define kmutex_unlock pthread_mutex_unlock
#define kmutex_destroy pthread_mutex_destroy

#ifdef _WIN32
typedef void kcond;
#else
typedef struct {
	bool ev;
	bool auto_reset;
	kmutex  mutex;
	pthread_cond_t cond;
} kcond;
#endif
INLINE kcond *kcond_init(bool auto_reset) {
#ifdef _WIN32
	return  CreateEvent(NULL, (auto_reset ? FALSE : TRUE), FALSE, NULL);
#else
	kcond *h = (kcond *)xmalloc(sizeof(kcond));
	memset(h, 0, sizeof(kcond));
	kmutex_init(&h->mutex, NULL);
	pthread_cond_init(&h->cond, NULL);
	h->auto_reset = auto_reset;
	h->ev = false;
	return h;
#endif
}
INLINE void kcond_wait(kcond *cond)
{
#ifdef _WIN32
	WaitForSingleObject(cond, INFINITE);
#else
	kmutex_lock(&cond->mutex);
	if (cond->ev) {
		cond->ev = false;
		kmutex_unlock(&cond->mutex);
		return;
	}
	pthread_cond_wait(&cond->cond, &cond->mutex);
	if (cond->auto_reset) {
		cond->ev = false;
	}
	kmutex_unlock(&cond->mutex);
#endif
}
INLINE void kcond_notice(kcond *cond)
{
#ifdef _WIN32
	SetEvent(cond);
#else
	kmutex_lock(&cond->mutex);
	cond->ev = true;
	if (cond->auto_reset) {
		pthread_cond_signal(&cond->cond);
	} else {
		pthread_cond_broadcast(&cond->cond);
	}
	kmutex_unlock(&cond->mutex);
#endif
}
INLINE void kcond_destroy(kcond *cond)
{
#ifdef _WIN32
	CloseHandle(cond);
#else
	pthread_mutex_destroy(&cond->mutex);
	pthread_cond_destroy(&cond->cond);
	xfree(cond);
#endif
}
INLINE void kgl_pause()
{
#ifdef _WIN32
	YieldProcessor();
#else
	__asm__("pause");
#endif
}

//读优先
typedef volatile int32_t krw_mutex;
INLINE void krw_mutex_init(krw_mutex *mutex)
{
	*mutex = 0;
}
INLINE void krw_mutex_rlock(krw_mutex *mutex)
{
	int32_t x;
	for (;;) {
		x = *mutex;
		/* write lock is held */
		if (x < 0) {
			kgl_pause();
			continue;
		}
		if (katom_cas((void *)mutex, x, x + 1)) {
			//lock success
			break;
		}
	}
	return;
}
INLINE void krw_mutex_wlock(krw_mutex *mutex)
{
	for (;;) {
		/* write lock is held */
		if (*mutex != 0) {
			kgl_pause();
			continue;
		}
		if (katom_cas((void *)mutex, 0, -1)) {
			//lock success
			break;
		}
	}
}
INLINE void krw_mutex_wunlock(krw_mutex *mutex)
{
	katom_inc((void *)mutex);
}
INLINE void krw_mutex_runlock(krw_mutex *mutex)
{
	katom_dec((void *)mutex);
}

//写优先
typedef struct {
	krw_mutex rw;
	krw_mutex try_write;
} kwr_mutex;

INLINE void kwr_mutex_init(kwr_mutex *mutex)
{
	krw_mutex_init(&mutex->rw);
	krw_mutex_init(&mutex->try_write);
}
INLINE void kwr_mutex_rlock(kwr_mutex *mutex, bool high_priority)
{
	if (high_priority) {
		krw_mutex_rlock(&mutex->rw);
		return;
	}
	while (mutex->try_write > 0) {
		kgl_pause();
	}
	krw_mutex_rlock(&mutex->rw);
}
INLINE void kwr_mutex_wlock(kwr_mutex *mutex)
{
	for (;;) {
		if (mutex->try_write != 0) {
			kgl_pause();
			continue;
		}
		if (katom_cas((void *)&mutex->try_write, 0, 1)) {
			//try_write  success
			break;
		}
	}
	krw_mutex_wlock(&mutex->rw);
}
INLINE void kwr_mutex_wunlock(kwr_mutex *mutex)
{
	krw_mutex_wunlock(&mutex->rw);
	katom_set((void *)&mutex->try_write, 0);
}
INLINE void kwr_mutex_runlock(kwr_mutex *mutex)
{
	krw_mutex_runlock(&mutex->rw);
}
KEND_DECLS
#endif
