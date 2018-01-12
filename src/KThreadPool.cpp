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
#include "global.h"

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>
#endif 
#include <time.h>
#include <assert.h>
//#include "KFilter.h"
#include "KThreadPool.h"
#include "do_config.h"
#include "lang.h"
#include "log.h"
#include "malloc_debug.h"

#include <sstream>

#include<string>
KThreadPool m_thread;
int total_thread = 0;
static const int max_sleep_time = 60;
static ThreadInfoList FreeThread;
static KMutex m_ThreadPoolLock;
static KMutex ipLock;
struct connect_per_ip_t {
	ip_addr ip;
	unsigned per_ip;
};
using namespace std;
#ifndef _WIN32
static sigset_t m_blockset;
static sigset_t m_all;

void recv_notice_thread_ignore(int sig) {
	//	//printf("recv signal\n");
	//	printf("error recv a signal %d in pthread_id=%d\n", sig,
	//			pthread_self());
	assert(sig==NOTICE_THREAD_SIG);
}
#endif
#if 0
void set_stack_size(std::string stack_size) {
#if 0
	unsigned size = (unsigned)get_size(stack_size.c_str());
	if (size < 16 * 1024 || size > 50 * 1048576) {
		//min size 16k,max size 50m
		//default size: 2m
		size = 2 * 1048576;
	}
	conf.stack_size = size;
	m_thread.setStackSize(conf.stack_size);
#endif
}
#endif
FUNC_TYPE FUNC_CALL run_thread(void *param) {
	ThreadInfo *m_thread = (ThreadInfo *) param;
	m_thread->pid = pthread_self();
	
	m_thread->runCount = 0;
	m_thread->work(m_thread->param);
	
	for (;;) {
		m_thread->runCount++;
		m_thread->end_time = time(NULL);
#ifndef _WIN32
		pthread_sigmask(SIG_BLOCK, &m_blockset, NULL);//阻塞信号
#endif
		m_ThreadPoolLock.Lock();
		m_thread->cmd = COMMAND_THREAD_NULL;
		total_thread--;
		FreeThread.push_front(m_thread);
		m_ThreadPoolLock.Unlock();
		
		int recv_sig = 0;
		sigwait(&m_blockset, &recv_sig);
		pthread_sigmask(SIG_UNBLOCK, &m_blockset, NULL);
		
		if (m_thread->cmd == COMMAND_THREAD_START) {
			m_thread->work(m_thread->param);
		} else if (m_thread->cmd == COMMAND_THREAD_END) {

			delete m_thread;
			KTHREAD_RETURN;
		} else {
			klog(
					KLOG_ERR,
					"closed or bug!!in %s:%d(recv awake signal),my pthread id=%d,pid=%d\n",
					__FILE__, __LINE__, pthread_self(),getpid());
			delete m_thread;
			KTHREAD_RETURN;
		}
	}
	KTHREAD_RETURN;
}
void KThreadPool::setStackSize(unsigned size) {
#ifndef _WIN32
	//pthread_attr_setstacksize(&m_thread.attr, conf.stack_size);
#endif
}
KThreadPool::KThreadPool() {
#ifndef _WIN32
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);//设置线程为分离
	//printf("signal=%d\n",NOTICE_THREAD_SIG);
	signal(NOTICE_THREAD_SIG, recv_notice_thread_ignore);
	sigemptyset(&m_blockset);
	sigaddset(&m_blockset, NOTICE_THREAD_SIG);
	sigemptyset(&m_all);
#endif
}
KThreadPool::~KThreadPool() {
}
void KThreadPool::closeAllFreeThread() {
	ThreadInfoList::iterator it;
	pthread_t pid;//
	m_ThreadPoolLock.Lock();
	for (it = FreeThread.end(); it != FreeThread.begin();) {
		it--;
		pid = (*it)->pid;
		(*it)->cmd = COMMAND_THREAD_END;
#ifndef _WIN32
		if (pthread_kill(pid, NOTICE_THREAD_SIG) != 0) {
			klog(KLOG_ERR,
					"cann't send signal to thread(command:close,id=%d)\n", pid);
		}
		
#endif
		it = FreeThread.erase(it);
	}
	m_ThreadPoolLock.Unlock();
}
void KThreadPool::Flush(unsigned min_free_thread) {

	ThreadInfoList::iterator it;
	time_t now_time = time(NULL);
	pthread_t pid;//
	m_ThreadPoolLock.Lock();
	for (it = FreeThread.end(); it != FreeThread.begin();) {
		if (FreeThread.size() <= min_free_thread)
			break;
		it--;
		if (now_time - (*it)->end_time > max_sleep_time) {
			pid = (*it)->pid;
			(*it)->cmd = COMMAND_THREAD_END;
#ifndef _WIN32
			if (pthread_kill(pid, NOTICE_THREAD_SIG) != 0) {
				klog(KLOG_ERR,
						"cann't send signal to thread(command:close,id=%d)\n",
						pid);
			}
			
#endif
			it = FreeThread.erase(it);
		} else {
			break;
		}
	}
	m_ThreadPoolLock.Unlock();
}
int KThreadPool::get_work_thread_count()
{
	m_ThreadPoolLock.Lock();
	int ret = total_thread;
	m_ThreadPoolLock.Unlock();
	return ret;
}
int KThreadPool::getFreeThread() {
	int FreeThreadCount = 0;
	m_ThreadPoolLock.Lock();
	FreeThreadCount = FreeThread.size();
	m_ThreadPoolLock.Unlock();
	return FreeThreadCount;
}
bool KThreadPool::start(void *param, ThreadFunc work, bool usePool) {
	if (!usePool) {
		int ret = pthread_create(&id, &attr, work, param);
		return PTHREAD_CREATE_SUCCESSED(ret);
	}
	ThreadInfoList::iterator it;
	//	ip_addr *ip=param->server.get_remote_addr();
	int ret = 0;
	ThreadInfo *m_thread = NULL;

	m_ThreadPoolLock.Lock();

	it = FreeThread.begin();
	if (it == FreeThread.end()) {//It is no free Thread now
		m_thread = new ThreadInfo;
		if (m_thread == NULL) {
			//	klog(ERR_LOG,"no mem to alloc\n");
			goto err;
		}
		m_thread->param = param;
		m_thread->work = work;
		ret = pthread_create(&id, &attr, run_thread, (void *) m_thread);
		if (!PTHREAD_CREATE_SUCCESSED(ret)) {
			klog(KLOG_ERR, "create thread error.result=%d,errno=%d\n", ret,
					errno);
			delete m_thread;
			goto err;
		}
	} else {
		(*it)->cmd = COMMAND_THREAD_START;
		(*it)->param = param;
		(*it)->work = work;
#ifndef _WIN32
		if (pthread_kill((*it)->pid, NOTICE_THREAD_SIG) != 0) {
			klog(KLOG_ERR,
					"cann't send signal to thread(command:start,id=%d)\n",
					(*it)->pid);
		}
		
#endif
		FreeThread.erase(it);
	}
	total_thread++;
	m_ThreadPoolLock.Unlock();
	return true;

	err: m_ThreadPoolLock.Unlock();
	return false;
}
