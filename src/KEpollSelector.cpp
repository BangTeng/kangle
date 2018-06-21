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
#ifdef EPOLL_CLOEXEC
	kdpfd = epoll_create1(EPOLL_CLOEXEC);
#else
	kdpfd = epoll_create(MAXEVENT);
#endif
	aio_st = new KAioSelectable(this);
	notice_st = new KEpollNoticeSelectable(this);
#ifdef EPOLLRDHUP
	//printf("epoll support read_hup event\n");
#endif
}
void KEpollSelector::handle_read_event(KSelectable *st)
{
	//printf("handle_read_event st=[%p]\n",st);
	if (TEST(st->st_flags,STF_ET)) {
		CLR(st->st_flags,STF_READ);
	}
#ifdef ENABLE_KSSL_BIO
	st->lowEventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#else
	st->eventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#endif
}
void KEpollSelector::handle_write_event(KSelectable *st)
{
	CLR(st->st_flags,STF_WRITE|STF_RDHUP);
#ifdef ENABLE_KSSL_BIO
	st->lowEventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#else
	st->eventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#endif
}
KEpollSelector::~KEpollSelector() {
	::close(kdpfd);
}
void KEpollSelector::select() {
	epoll_event events[MAXEVENT];
	uint32_t ev;
	for (;;) {
#ifdef MALLOCDEBUG
		if (closed_flag) {
			delete this;
			return;
		}
#endif
		for (;;) {
			kgl_list *l = klist_head(&list[KGL_LIST_READY]);
			if (l == &list[KGL_LIST_READY]) {
				break;
			}
			KSelectable *st = kgl_list_data(l,KSelectable,queue);
			klist_remove(l);
			memset(l, 0, sizeof(kgl_list));
			count--;
			uint16_t st_flags = st->st_flags;
			if (TEST(st_flags,STF_WREADY) && TEST(st_flags,STF_WRITE|STF_RDHUP)) {
				handle_write_event(st);
				CLR(st_flags,STF_WRITE|STF_RDHUP);
			}
			if (TEST(st_flags,STF_RREADY) && TEST(st_flags,STF_READ)) {
				handle_read_event(st);
				CLR(st_flags,STF_READ);
			}
			if (TEST(st_flags,STF_READ|STF_WRITE) &&
				TEST(st_flags,STF_ET) &&
				st->queue.next==NULL) {
				add_list(st,KGL_LIST_RW);
			}
		}
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
			remove_list(st);
			ev = events[n].events;
			//klog(KLOG_DEBUG,"event happened st=[%p] ev=[%d]\n",st,ev);
			if (TEST(ev, EPOLLHUP | EPOLLERR)) {
				SET(st->st_flags, STF_ERR);
			}
			if (TEST(ev,EPOLLRDHUP)) {
				SET(st->st_flags,STF_WREADY);
				if (TEST(st->st_flags,STF_WRITE|STF_RDHUP)) {
					add_list(st,KGL_LIST_READY);
				}
			} else if (TEST(ev,EPOLLOUT)) {
				SET(st->st_flags,STF_WREADY);
				if (TEST(st->st_flags,STF_WRITE)) {
					add_list(st,KGL_LIST_READY);
				}
			}
			if (TEST(ev,EPOLLIN|EPOLLPRI)) {
				SET(st->st_flags,STF_RREADY);
				if (TEST(st->st_flags,STF_READ) && st->queue.next==NULL) {
					add_list(st,KGL_LIST_READY);
				}
			}
		}
	}

}
void KEpollSelector::removeSocket(KSelectable *st) {
	if (!TEST(st->st_flags,STF_REV|STF_WEV)) {
		//socket not set event
		return;
	}
	remove_list(st);
	SOCKET sockfd = st->getSocket()->get_socket();
#ifndef NDEBUG
	if (TEST(st->st_flags,STF_ET)) {
		assert(TEST(st->st_flags,STF_READ|STF_WRITE|STF_RDHUP)==0);
	}
	klog(KLOG_DEBUG,"removeSocket st=%p,sockfd=%d\n",st,sockfd);
#endif
	struct epoll_event ev;
	CLR(st->st_flags,STF_REV|STF_WEV|STF_ET|STF_RREADY|STF_WREADY);
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
	if (TEST(st->st_flags,STF_REV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_WRITE|STF_RDHUP);
	SET(st->st_flags,STF_READ|STF_REV);
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
	//printf("st=[%p] read_hup\n",st);
#ifdef EPOLLRDHUP
	if (TEST(st->st_flags,STF_READ|STF_WRITE)) {
		return false;
	}
	SET(st->st_flags,STF_RDHUP);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	if (!TEST(st->st_flags,STF_WEV|STF_REV)) {
		if (!add_event(st,STF_WEV|STF_REV)) {
			CLR(st->st_flags,STF_RDHUP);
			return false;
		}
	}
	return true;
#else
	return false;
#endif
}
bool KEpollSelector::add_event(KSelectable *st,uint16_t ev)
{
	struct epoll_event event;
	int op = EPOLL_CTL_ADD;
	uint32_t events = 0;
	uint16_t prev_ev = st->st_flags;
	if (TEST(ev,STF_REV)) {
		events |= EPOLLIN|EPOLLRDHUP|EPOLLET;
		if (TEST(prev_ev,STF_WEV)) {
			op = EPOLL_CTL_MOD;
			events|=EPOLLOUT;
		}
		SET(st->st_flags,STF_REV|STF_ET|STF_WREADY);
	}
	if (TEST(ev,STF_WEV)) {
		events |= EPOLLOUT|EPOLLRDHUP|EPOLLET;
		if (TEST(prev_ev,STF_REV)) {
			op = EPOLL_CTL_MOD;
			events|=EPOLLIN;
		}
		SET(st->st_flags,STF_WEV|STF_ET);
	}
	SOCKET sockfd = st->getSocket()->get_socket();
	event.events = events;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"%s event [%d] epoll event=[%lld] sockfd=[%d],st=[%p]\n",op==EPOLL_CTL_ADD?"add":"modify",ev,int64_t(events),sockfd,st);
#endif
	event.data.ptr = st;
	int ret = epoll_ctl(kdpfd, op, sockfd, &event);
	if (ret !=0) {
		klog(KLOG_ERR, "epoll ctl error fd=%d,errno=%d %s\n", sockfd,errno,strerror(errno));
		return false;
	}
	return true;
}
bool KEpollSelector::read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
	//printf("st=[%p] read\n",st);
	assert(TEST(st->st_flags,STF_READ)==0);
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = buffer;
	SET(st->st_flags,STF_READ);
	CLR(st->st_flags,STF_RDHUP);
	if (TEST(st->st_flags,STF_RREADY)) {
		add_list(st,KGL_LIST_READY);
		return true;
	}
	if (!TEST(st->st_flags,STF_REV)) {
		if (!add_event(st,STF_REV)) {
			CLR(st->st_flags,STF_READ);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		add_list(st,KGL_LIST_RW);
	}
	return true;
}
bool KEpollSelector::write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
	//printf("st=[%p] write\n",st);
	assert(TEST(st->st_flags,STF_WRITE)==0);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	SET(st->st_flags,STF_WRITE);
	CLR(st->st_flags,STF_RDHUP);
	if (TEST(st->st_flags,STF_WREADY)) {
		add_list(st,KGL_LIST_READY);
		return true;
	}
	if (!TEST(st->st_flags,STF_WEV)) {
		if (!add_event(st,STF_REV|STF_WEV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		add_list(st,KGL_LIST_RW);
	}
	return true;
}
bool KEpollSelector::connect(KSelectable *st,resultEvent result,void *arg)
{
	//printf("st=[%p] connect\n",st);
	assert(TEST(st->st_flags,STF_READ|STF_WRITE|STF_RDHUP|STF_REV|STF_WEV)==0);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = NULL;
	SET(st->st_flags,STF_WRITE);
	if (!TEST(st->st_flags,STF_WEV|STF_REV)) {
		if (!add_event(st,STF_WEV|STF_REV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	add_list(st,KGL_LIST_CONNECT);
	return true;
}
void KEpollSelector::remove_read_hup(KSelectable *st)
{
	assert(is_same_thread());
	if (!TEST(st->st_flags,STF_RDHUP)) {
		return;
	}
	assert(TEST(st->st_flags,STF_READ|STF_WRITE)==0);
	CLR(st->st_flags,STF_RDHUP);
	//if st in ready list,remove it
	remove_list(st);
}
bool KEpollSelector::next(resultEvent result,void *arg,int got)
{
	KSelectable *next_st = new KSelectable;
	memset(next_st,0,sizeof(KSelectable));
	next_st->selector = this;
	next_st->e[OP_READ].arg = arg;
	next_st->e[OP_READ].result = result;
	next_st->e[OP_READ].buffer = NULL;
	return notice_st->notice(next_st,got);
}
#endif
