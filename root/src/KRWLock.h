#ifndef krwlock_h_lsdkjfs0d9fsdfsdafasdfasdf8sdf9
#define krwlock_h_lsdkjfs0d9fsdfsdafasdfasdf8sdf9
#include "KMutex.h"
#ifdef ENABLE_ATOM
#include "katom.h"
inline void kgl_pause()
{
#ifdef _WIN32
	YieldProcessor();
#else
#if defined(__i386__) || defined(__x86_64__)
#if defined(__SSE__)
         //_mm_pause();
         __asm__ ("pause" ) ;
#else
        __asm__ __volatile__ ("rep; nop");
#endif
#endif
#endif
}
class KRWLock
{
public:
	KRWLock()
	{
		cnt = 0;
	}
	int RLock()
	{
		int32_t x;
		for (;;) {
			x = cnt;
			/* write lock is held */
			if (x < 0) {
				kgl_pause();
				continue;
			}
			if (katom_cas((void *)&cnt,x, x+1)) {
				//lock success
				break;
			}
		}
		return 0;
	}
	int WLock()
	{
		for (;;) {
			/* write lock is held */
			if (cnt!=0) {
				kgl_pause();
				continue;
			}
			if (katom_cas((void *)&cnt,0,-1)) {
				//lock success
				break;
			}
		}
		return 0;
	}
	int WUnlock()
	{
		katom_inc((void *)&cnt);
		return 0;
	}
	int RUnlock()
	{
		katom_dec((void *)&cnt);
		return 0;
	}
private:
	volatile int32_t cnt;
};
#else
#ifdef DEAD_LOCK
#define RLock          Lock
#define WLock          Lock
#define KRWLock       KMutex
#else
#ifndef _WIN32
class KRWLock
{
public:
KRWLock()
{
        pthread_rwlock_init(&m_rw_lock,NULL);
}
~KRWLock()
{
        pthread_rwlock_destroy(&m_rw_lock);

}
int RLock()
{
        return pthread_rwlock_rdlock(&m_rw_lock);
}
int WLock()
{
         return pthread_rwlock_wrlock(&m_rw_lock);
}
int WUnlock()
{
        return pthread_rwlock_unlock(&m_rw_lock);
}
int RUnlock()
{
	 return pthread_rwlock_unlock(&m_rw_lock);
}
private:
	 pthread_rwlock_t m_rw_lock;

};
#else
#define RLock		Lock
#define WLock		Lock
#define RUnlock         Unlock
#define WUnlock         Unlock
#define KRWLock        	KMutex
#endif//_WIN32�������
#endif
#endif
class KWLocker
{
public:
	KWLocker(KRWLock *lock)
	{
		lock->WLock();
		this->lock = lock;
	}
	~KWLocker()
	{
		lock->WUnlock();
	}
private:
	KRWLock *lock;
};
class KRLocker
{
public:
	KRLocker(KRWLock *lock)
	{
		lock->RLock();
		this->lock = lock;
	}
	~KRLocker()
	{
		lock->RUnlock();
	}
private:
	KRWLock *lock;
};
#endif
