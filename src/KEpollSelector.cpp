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
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <errno.h>
#include "global.h"
#include "KEpollSelector.h"
#include <stdio.h>
#include "log.h"
#include "KSelectorManager.h"
#include "do_config.h"
#include "malloc_debug.h"
#include "time_utils.h"
#include "KConnectionSelectable.h"
#ifdef HAVE_SYS_EPOLL_H
//#ifdef WORK_MODEL_TCP
//#define MAXEVENT        1
//#else
#define MAXEVENT	256
//#endif
KEpollSelector::KEpollSelector() {
	kdpfd = epoll_create(MAXEVENT);
	aio_st = new KAioSelectable(kdpfd);
#ifdef EPOLLRDHUP
	//printf("epoll support read_hup event\n");
#endif
}

KEpollSelector::~KEpollSelector() {
	close(kdpfd);
}
void KEpollSelector::select() {
	epoll_event events[MAXEVENT];
	for (;;) {
#ifdef MALLOCDEBUG
		if (closeFlag) {
			delete this;
			return;
		}
#endif
		checkTimeOut();
		int ret = epoll_wait(kdpfd, events, MAXEVENT,tmo_msec);
		if (utm) {
			updateTime();
		}
		//if (ret>0) {
		//	printf("epoll_wait ret=[%d]\n",ret);
		//}
		for (int n = 0; n < ret; ++n) {
			KSelectable *st = ((KSelectable *) events[n].data.ptr);
			assert(TEST(st->st_flags,STF_LOCK)==0);
			if (TEST(events[n].events,EPOLLRDHUP|EPOLLOUT)) {
				assert(TEST(st->st_flags,STF_WRITE|STF_RDHUP));
				SET(st->st_flags,STF_WLOCK);
			} else if (TEST(events[n].events,EPOLLIN|EPOLLPRI)) {
				assert(TEST(st->st_flags,STF_READ));
				SET(st->st_flags,STF_RLOCK);
			} else if (TEST(st->st_flags,STF_RDHUP|STF_WRITE)) {
				SET(st->st_flags,STF_WLOCK);
			} else {
				assert(TEST(st->st_flags,STF_READ));
				SET(st->st_flags,STF_RLOCK);
			}
			if (TEST(st->st_flags,STF_ONE_SHOT)) {
				CLR(st->st_flags,STF_READ|STF_WRITE|STF_RDHUP);
			}
		}
		for (int n = 0; n < ret; ++n) {
			KSelectable *st = ((KSelectable *) events[n].data.ptr);
			if (TEST(st->st_flags,STF_WLOCK)) {
				CLR(st->st_flags,STF_WLOCK);
				assert(TEST(st->st_flags,STF_RLOCK)==0);
				assert(st->e[OP_WRITE].result);
				if (TEST(events[n].events,EPOLLERR)) {
					st->e[OP_WRITE].result(st->e[OP_WRITE].arg,-1);
					continue;
				}
#ifdef ENABLE_KSSL_BIO
				st->lowEventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#else
				st->eventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#endif
				continue;
			}
			assert(TEST(st->st_flags,STF_RLOCK|STF_WLOCK)==STF_RLOCK);
			CLR(st->st_flags,STF_RLOCK);
			assert(st->e[OP_READ].result);
			if (TEST(events[n].events,EPOLLERR)) {
					st->e[OP_READ].result(st->e[OP_READ].arg,-1);
					continue;
			}
#ifdef ENABLE_KSSL_BIO
			st->lowEventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#else
			st->eventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#endif
		}
	}

}
void KEpollSelector::removeSocket(KSelectable *st) {
	if (!TEST(st->st_flags,STF_EV)) {
		//socket not set event
		return;
	}
	SOCKET sockfd = st->getSocket()->get_socket();
#ifndef NDEBUG
	klog(KLOG_DEBUG,"removeSocket st=%p,sockfd=%d\n",st,sockfd);
#endif
	struct epoll_event ev;
	CLR(st->st_flags,STF_EV|STF_READ|STF_WRITE|STF_RDHUP|STF_ONE_SHOT);
	if (epoll_ctl(kdpfd, EPOLL_CTL_DEL,sockfd, &ev) != 0) {
		klog(KLOG_ERR, "epoll del sockfd error: fd=%d,errno=%d\n", sockfd,errno);
		return;
	}
}
bool KEpollSelector::listen(KServerSelectable *st,resultEvent result)
{
	struct epoll_event ev;
	st->e[OP_READ].arg = st;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = NULL;
	st->e[OP_WRITE].result = NULL;
	SOCKET sockfd = st->getSocket()->get_socket();
	int poll_op;
	if (TEST(st->st_flags,STF_EV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_WRITE|STF_RDHUP);
	SET(st->st_flags,STF_READ|STF_EV);
	ev.events = EPOLLIN;
	ev.data.ptr = static_cast<KSelectable *>(st);
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
		klog(KLOG_ERR, "epoll add listen event error: fd=%d,errno=%d %s\n", sockfd,errno,strerror(errno));
		return false;
	}
	return true;

}
bool KEpollSelector::read_hup(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
#ifdef EPOLLRDHUP
	SOCKET sockfd = st->getSocket()->get_socket();
	if (sockfd==INVALID_SOCKET) {
		return false;
	}
	if (TEST(st->st_flags,STF_LOCK)) {
		return false;
	}
	struct epoll_event ev;
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	int poll_op;
	if (TEST(st->st_flags,STF_EV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_WRITE|STF_READ);
	SET(st->st_flags, STF_EV|STF_RDHUP|STF_ONE_SHOT);
	ev.events = EPOLLRDHUP
#ifdef ENABLE_ONESHOT_MODEL
	|EPOLLONESHOT
#endif
	;
	ev.data.ptr = st;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"read_hup sockfd=%d,st=%p\n",sockfd,st);
#endif
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
		CLR(st->st_flags,STF_RDHUP);
		klog(KLOG_ERR, "epoll set insertion error: fd=%d,errno=%d %s\n", sockfd,errno,strerror(errno));
		return false;
	}
	return true;
#else
	return false;
#endif
}

bool KEpollSelector::read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
	struct epoll_event ev;
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = buffer;
	if (TEST(st->st_flags,STF_RLOCK)) {
		return true;
	}
	int poll_op;
	if (TEST(st->st_flags,STF_EV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_WRITE|STF_RDHUP);
	SET(st->st_flags,STF_READ|STF_EV|STF_ONE_SHOT);
	SOCKET sockfd = st->getSocket()->get_socket();
	ev.events = EPOLLIN
#ifdef ENABLE_ONESHOT_MODEL
	|EPOLLONESHOT
#endif
	;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"read sockfd=%d,st=%p\n",sockfd,st);
#endif
	ev.data.ptr = st;
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
		CLR(st->st_flags,STF_READ);
		klog(KLOG_ERR, "epoll set insertion error: fd=%d,errno=%d %s\n", sockfd,errno,strerror(errno));
		return false;
	}
	return true;
}
bool KEpollSelector::write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
	struct epoll_event ev;
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	int poll_op;
	if (TEST(st->st_flags,STF_EV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	if (TEST(st->st_flags,STF_WLOCK)) {
		return true;
	}
	CLR(st->st_flags,STF_READ|STF_RDHUP);
	SET(st->st_flags,STF_WRITE|STF_EV|STF_ONE_SHOT);
	SOCKET sockfd = st->getSocket()->get_socket();
	ev.events = EPOLLOUT
#ifdef EPOLLRDHUP
	|EPOLLRDHUP
#endif
#ifdef ENABLE_ONESHOT_MODEL
	|EPOLLONESHOT
#endif
	;
	if (TEST(st->st_flags, STF_ALWAYS_READ)) {
		SET(ev.events, EPOLLIN);
		SET(st->st_flags,STF_READ);
	}
	ev.data.ptr = st;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"write sockfd=%d,st=%p,ev=[%d]\n",sockfd,st,ev.events);
#endif
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
		CLR(st->st_flags,STF_WRITE);
		klog(KLOG_ERR, "epoll set insertion error: fd=%d,errno=%d %s\n", sockfd,errno,strerror(errno));
		return false;
	}
	return true;
}
bool KEpollSelector::connect(KSelectable *st,resultEvent result,void *arg)
{
	return write(st,result,NULL,arg);
}
bool KEpollSelector::next(KSelectable *st,resultEvent result,void *arg)
{
	return write(st,result,NULL,arg);
}
#endif
