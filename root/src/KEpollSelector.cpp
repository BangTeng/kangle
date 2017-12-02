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
#define MAXEVENT        1
//#else
//#define MAXEVENT	256
//#endif
KEpollSelector::KEpollSelector() {
	kdpfd = epoll_create(128);
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
		for (int n = 0; n < ret; ++n) {
			KSelectable *st = ((KSelectable *) events[n].data.ptr);
#ifndef NDEBUG
			klog(KLOG_DEBUG,"select st=%p,st_flags=%d,events=%d at %p\n",st,st->st_flags,events[n].events,pthread_self());
#endif
			assert(TEST(st->st_flags,STF_EV));
#ifdef EPOLLRDHUP
			if (TEST(events[n].events,EPOLLRDHUP)) {
				SET(st->st_flags,STF_RDHUP);
				st->e[OP_WRITE].result(st->e[OP_WRITE].arg,-1);
				continue;
			}
#endif
			if (TEST(events[n].events,EPOLLOUT)) {
				//assert(TEST(st->st_flags,STF_WRITE));
				assert(st->e[OP_WRITE].result);
				if (TEST(events[n].events,EPOLLERR)) {
					st->e[OP_WRITE].result(st->e[OP_WRITE].arg,-1);
				} else {
#ifdef ENABLE_KSSL_BIO
					st->lowEventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#else
					st->eventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#endif
				}
				continue;
			}
			if (TEST(events[n].events,EPOLLIN)) {
				//assert(TEST(st->st_flags,STF_READ));
				assert(st->e[OP_READ].result);
#ifdef ENABLE_KSSL_BIO
				st->lowEventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#else
				st->eventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#endif
				continue;
			} 
			if (TEST(st->st_flags,STF_READ)) {
				st->e[OP_READ].result(st->e[OP_READ].arg,-1);
			} else {
				assert(st->e[OP_WRITE].result!=NULL);
				st->e[OP_WRITE].result(st->e[OP_WRITE].arg,-1);
			}
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
	CLR(st->st_flags,STF_EV|STF_READ|STF_WRITE);
	if (epoll_ctl(kdpfd, EPOLL_CTL_DEL,sockfd, &ev) != 0) {
		klog(KLOG_ERR, "epoll del sockfd error: fd=%d,errno=%d\n", sockfd,errno);
		return;
	}
}
bool KEpollSelector::listen(KServer *st,resultEvent result)
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
	CLR(st->st_flags,STF_WRITE|STF_ET);
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
	struct epoll_event ev;
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	//st->e[OP_READ].result = NULL;
	//st->e[OP_READ].buffer = NULL;
	int poll_op;
	if (TEST(st->st_flags,STF_EV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_WRITE|STF_READ);
	SET(st->st_flags, STF_EV);
	ev.events = EPOLLRDHUP;
	ev.data.ptr = st;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"read_hup sockfd=%d,st=%p\n",sockfd,st);
#endif
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
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
	//st->e[OP_WRITE].result = NULL;
	//st->e[OP_WRITE].buffer = NULL;
	int poll_op;
	if (TEST(st->st_flags,STF_EV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_WRITE);
	SET(st->st_flags,STF_READ|STF_EV);
	SOCKET sockfd = st->getSocket()->get_socket();
	ev.events = EPOLLIN;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"read sockfd=%d,st=%p\n",sockfd,st);
#endif
	ev.data.ptr = st;
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
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
	//st->e[OP_READ].result = NULL;
	//st->e[OP_READ].buffer = NULL;
	int poll_op;
	if (TEST(st->st_flags,STF_EV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_READ);
	SET(st->st_flags,STF_WRITE|STF_EV);
	SOCKET sockfd = st->getSocket()->get_socket();
	ev.events = EPOLLOUT
#ifdef EPOLLRDHUP
	|EPOLLRDHUP
#endif
	;
	ev.data.ptr = st;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"write sockfd=%d,st=%p\n",sockfd,st);
#endif
	int ret = epoll_ctl(kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
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
