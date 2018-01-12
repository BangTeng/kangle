#include <string.h>
#include <stdlib.h>
#include <vector>
#include <errno.h>
#include "global.h"
#ifdef BSD_OS
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include "log.h"
#include "KSelectorManager.h"
#include "do_config.h"
#include "time_utils.h"
#include "KKqueueSelector.h"
#include "malloc_debug.h"
#define MAXEVENT	16
#ifndef NETBSD
typedef void * kqueue_udata_t;
#else
typedef intptr_t kqueue_udata_t;
#endif
KKqueueSelector::KKqueueSelector() {
	kdpfd = kqueue() ;
}

KKqueueSelector::~KKqueueSelector() {
	close(kdpfd);
}
void KKqueueSelector::select() {
	struct kevent events[MAXEVENT]; 
	struct timespec tm;
	tm.tv_sec = tmo_msec/1000;
	tm.tv_nsec = tmo_msec * 1000 - tm.tv_sec * 1000000;
	for (;;) {
		checkTimeOut();
		int ret = kevent(kdpfd, NULL, 0, events, MAXEVENT, &tm);
		if(utm){
			updateTime();
		}
		for (int n = 0; n < ret; ++n) {
			KSelectable *st = (KSelectable *) events[n].udata;
#ifndef NDEBUG
			klog(KLOG_DEBUG,"select st=%p,st_flags=%d,events=%d at %p\n",st,st->st_flags,events[n].filter,pthread_self());
			assert(TEST(st->st_flags,STF_READ|STF_WRITE)>0);
#endif
			if (events[n].filter==EVFILT_WRITE) {
				st->eventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
			} else if(events[n].filter == EVFILT_READ) {
				st->eventRead(st->e[OP_READ].arg, st->e[OP_READ].result, st->e[OP_READ].buffer);
			} else {
				assert(false);
			}		
		}
	}

}
void KKqueueSelector::removeSocket(KSelectable *st) {
	if (TEST(st->st_flags,STF_EV)==0) {
		return;
	}
#ifndef NDEBUG
	klog(KLOG_DEBUG,"remove socket st=%p\n",st);
#endif
	SOCKET sockfd = st->getSocket()->get_socket();
	int ev = 0;
	if (TEST(st->st_flags,STF_READ)) {
		SET(ev ,EVFILT_READ);
	}
	if (TEST(st->st_flags,STF_WRITE)) {
		SET(ev,EVFILT_WRITE);
	}
	struct kevent changes[1]; 
	EV_SET(&changes[0], sockfd, ev, EV_DELETE, 0, 0, NULL); 
    kevent(kdpfd, changes, 1, NULL, 0, NULL);
	CLR(st->st_flags,STF_EV|STF_READ|STF_WRITE|STF_ALWAYS_READ);
}
bool KKqueueSelector::read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
#ifndef NDEBUG
	klog(KLOG_DEBUG,"read st=%p\n",st);
#endif
	struct kevent changes[2];
	int ev_count = 0;
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = buffer;
	//st->e[OP_WRITE].result = NULL;
	SOCKET sockfd = st->getSocket()->get_socket();
	if (TEST(st->st_flags,STF_READ)==0) {
		EV_SET(&changes[ev_count++], sockfd, EVFILT_READ, EV_ADD, 0, 0, (kqueue_udata_t)st); 
		SET(st->st_flags,STF_READ|STF_EV);
	}
	if (TEST(st->st_flags,STF_WRITE)) {
		EV_SET(&changes[ev_count++], sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
		CLR(st->st_flags,STF_WRITE);
	}
	if (ev_count==0) {
		return true;
	}
      	if(kevent(kdpfd, changes, ev_count, NULL, 0, NULL)==-1){
		klog(KLOG_ERR,"cann't addSocket sockfd=%d for read\n",sockfd);
		return false;
	}
	return true;
}
bool KKqueueSelector::write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
#ifndef NDEBUG
        klog(KLOG_DEBUG,"write st=%p\n",st);
#endif
        struct kevent changes[2];
	int ev_count = 0;
        st->e[OP_WRITE].arg = arg;
        st->e[OP_WRITE].result = result;
        st->e[OP_WRITE].buffer = buffer;
        //st->e[OP_READ].result = NULL;
	SOCKET sockfd = st->getSocket()->get_socket();
	if (TEST(st->st_flags,STF_WRITE)==0) {
		EV_SET(&changes[ev_count++], sockfd, EVFILT_WRITE, EV_ADD, 0, 0, (kqueue_udata_t)st);
		SET(st->st_flags,STF_WRITE|STF_EV);
	}
	if (TEST(st->st_flags,STF_READ|STF_ALWAYS_READ)==STF_READ) {
            EV_SET(&changes[ev_count++], sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
            CLR(st->st_flags,STF_READ);
    }
    if (ev_count==0) {
            return true;
    }
    if(kevent(kdpfd, changes, ev_count, NULL, 0, NULL)==-1){
            klog(KLOG_ERR,"cann't addSocket sockfd=%d for write\n",sockfd);
            return false;
    }
    return true;

}
bool KKqueueSelector::listen(KServer *st,resultEvent result)
{
	return read(st,result,NULL,st);
}
bool KKqueueSelector::connect(KSelectable *st,resultEvent result,void *arg)
{
	return write(st,result,NULL,arg);
}
bool KKqueueSelector::next(KSelectable *st,resultEvent result,void *arg)
{
	return write(st,result,NULL,arg);
}
#endif
