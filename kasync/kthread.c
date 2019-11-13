#include "kthread.h"
#include "kforwin32.h"
#include "ksync.h"
#include "klist.h"
#include "klog.h"
//#include "kselector_manager.h"
#include <time.h>
#include <stdio.h>
#ifndef _WIN32
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>

#define KGL_NOTICE_THREAD_SIG		SIGUSR2
static sigset_t m_blockset;
static sigset_t m_all;
static pthread_attr_t attr;
#endif

static kgl_list free_threads;
static kmutex thread_lock;
static int work_thread_count = 0;
static int free_thread_count = 0;
static const int max_sleep_time = 60;

typedef enum {
	kthread_command_null,
	kthread_command_start,
	kthread_command_end
} kthread_command;

typedef KTHREAD_FUNCTION (*kthread_func)(void *);
typedef struct kthread_info_s kthread_info;

struct kthread_info_s
{
	void *param;
	kthread_command cmd;
	kthread_func work;
	pthread_t pid;
	unsigned run_count;
	time_t end_time;
	kgl_list queue;
#ifdef _WIN32
	HANDLE notice;
#endif
};
#ifndef _WIN32
void recv_notice_thread_ignore(int sig) {
	kassert(sig == KGL_NOTICE_THREAD_SIG);
}
#endif
static void kthread_info_destroy(kthread_info *m_thread)
{
#ifdef _WIN32
	CloseHandle(m_thread->notice);
#endif
	xfree(m_thread);
}
static void kthread_wait(kthread_info *ti)
{

#ifdef _WIN32
	WaitForSingleObject(ti->notice, INFINITE);
#else
	int recv_sig = 0;
	sigwait(&m_blockset, &recv_sig);
	pthread_sigmask(SIG_UNBLOCK, &m_blockset, NULL);
#endif
}
static void kthread_notice(kthread_info *ti)
{
#ifndef _WIN32
	if (pthread_kill(ti->pid, KGL_NOTICE_THREAD_SIG) != 0) {
		klog(KLOG_ERR,	"BUG!! cann't send signal to thread(command:close,id=%d)\n", ti->pid);
	}
#else
	SetEvent(ti->notice);
#endif
}
static KTHREAD_FUNCTION run_thread(void *param) {
	kthread_info *m_thread = (kthread_info *)param;
	m_thread->pid = pthread_self();
	m_thread->run_count = 0;
	m_thread->work(m_thread->param);

	for (;;) {
		m_thread->run_count++;
		m_thread->end_time = time(NULL);
#ifndef _WIN32
		//阻塞信号
		pthread_sigmask(SIG_BLOCK, &m_blockset, NULL);
#endif
		kmutex_lock(&thread_lock);
		m_thread->cmd = kthread_command_null;
		work_thread_count--;
		free_thread_count++;
		klist_append(&free_threads, &m_thread->queue);
		kmutex_unlock(&thread_lock);
		kthread_wait(m_thread);
		if (likely(m_thread->cmd == kthread_command_start)) {
			m_thread->work(m_thread->param);
		} else if (m_thread->cmd == kthread_command_end) {
			kthread_info_destroy(m_thread);
			KTHREAD_RETURN;
		} else {
			//fprintf(stderr,	"closed or bug!!in %s:%d(recv awake signal),my pthread id=%d,pid=%d\n",	__FILE__, __LINE__, (int)pthread_self(), getpid());
			kthread_info_destroy(m_thread);
			KTHREAD_RETURN;
		}
	}
	KTHREAD_RETURN;
}

void kthread_flush(int min_free_thread) {
	time_t now_time = time(NULL);
	kmutex_lock(&thread_lock);
	kgl_list *l;
	for (;;) {
		if (free_thread_count <= min_free_thread) {
			break;
		}
		l = free_threads.next;
		if (l == &free_threads) {
			kassert(free_thread_count > 0);
			break;
		}
		kthread_info *ti = kgl_list_data(l, kthread_info, queue);
		if (now_time - ti->end_time <= max_sleep_time) {
			break;
		}
		klist_remove(l);
		free_thread_count--;
		ti->cmd = kthread_command_end;
		kthread_notice(ti);
	}
	kmutex_unlock(&thread_lock);
}
void kthread_get_count(int *work_count, int *free_count)
{
	kmutex_lock(&thread_lock);
	*work_count = work_thread_count;
	*free_count = free_thread_count;
	kmutex_unlock(&thread_lock);
}
void kthread_init()
{
	kmutex_init(&thread_lock, NULL);
	klist_init(&free_threads);
#ifndef _WIN32
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);//设置线程为分离	
	signal(KGL_NOTICE_THREAD_SIG, recv_notice_thread_ignore);
	sigemptyset(&m_blockset);
	sigaddset(&m_blockset, KGL_NOTICE_THREAD_SIG);
	sigemptyset(&m_all);
#endif
}
bool kthread_start(KTHREAD_FUNCTION (*work)(void *param), void *param)
{
#ifndef _WIN32	
	pthread_t id;
#endif
	return PTHREAD_CREATE_SUCCESSED(pthread_create(&id, &attr, work, param));
}
bool kthread_pool_start(KTHREAD_FUNCTION(*work)(void *param), void *param)
{
	kthread_info *m_thread;
	kmutex_lock(&thread_lock);
	kgl_list *l = free_threads.prev;
	if (l == &free_threads) {
		m_thread = xmemory_new(kthread_info);
		if (m_thread == NULL) {
			kmutex_unlock(&thread_lock);
			return false;
		}
		memset(m_thread, 0, sizeof(kthread_info));
#ifdef _WIN32
		m_thread->notice = CreateEvent(NULL, FALSE, FALSE, NULL);
#endif
		m_thread->param = param;
		m_thread->work = work;
		if (!kthread_start(run_thread, m_thread)) {
			kthread_info_destroy(m_thread);
			kmutex_unlock(&thread_lock);
			return false;
		}
	} else {
		m_thread = kgl_list_data(l, kthread_info, queue);
		m_thread->cmd = kthread_command_start;
		m_thread->param = param;
		m_thread->work = work;
		free_thread_count--;
		klist_remove(l);
		kthread_notice(m_thread);
	}
	work_thread_count++;
	kmutex_unlock(&thread_lock);
	return true;
}
void kthread_close_all_free()
{
	kthread_info *m_thread;
	kmutex_lock(&thread_lock);
	while (!klist_empty(&free_threads)) {
		kgl_list *l = klist_head(&free_threads);
		m_thread = kgl_list_data(l, kthread_info, queue);
		klist_remove(l);
		m_thread->cmd = kthread_command_end;
		m_thread->param = NULL;
		m_thread->work = NULL;
		kthread_notice(m_thread);
		free_thread_count--;
	}
	kassert(free_thread_count == 0);
	kmutex_unlock(&thread_lock);
}
