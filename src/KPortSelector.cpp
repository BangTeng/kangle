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
#include "KPortSelector.h"
#ifdef HAVE_PORT_H
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>

#include "utils.h"
#include "log.h"
#include "KThreadPool.h"
#include "malloc_debug.h"
#include "KHttpRequest.h"
#include "log.h"
#include "KSelectorManager.h"
#include "do_config.h"
#include "time_utils.h"
#include "malloc_debug.h"
#define MAXEVENT	16
KPortSelector::KPortSelector() {
	kdpfd = port_create() ;
}
KPortSelector::~KPortSelector() {
	close(kdpfd);
}
void KPortSelector::select() {
	port_event_t events[MAXEVENT]; 
	struct timespec tm;
	tm.tv_sec = tmo_msec/1000;
	tm.tv_nsec = (tmo_msec % 1000) * 1000000;
	uint_t ret=0;
	uint_t n;
	///*
	for (;;) {
		checkTimeOut();
		int ret = port_get(kdpfd,events,&tm);
		if(utm){
			updateTime();
		}
		if(ret==-1){
			continue;

		}
		KSelectable *st = (KSelectable *)events[0].portev_user;
#ifndef NDEBUG
		klog(KLOG_DEBUG,"select st=%p,st_flags=%d,events=%d at %p\n",st,st->st_flags,events[0].portev_events,pthread_self());
#endif
		if (TEST(events[0].portev_events, POLLOUT)) {
			st->eventWrite(st->e[OP_WRITE].arg, st->e[OP_WRITE].result, st->e[OP_WRITE].buffer);
		} else if (TEST(events[0].portev_events, POLLIN)) {
			st->eventRead(st->e[OP_READ].arg, st->e[OP_READ].result, st->e[OP_READ].buffer); \
		}			

	}
	/*//
	port_getn not stable
	for (;;) {
		checkTimeOut();
		ret = 1;
		int result = port_getn(kdpfd,events,(uint_t)MAXEVENT,&ret,&tm);
		if (utm) {
			updateTime();
		}
		if (result==-1) {
			continue;
		}
		printf("ret = %d\n",ret);
		for (n=0;n<ret;n++) {
			KSelectable *st = (KSelectable *)events[n].portev_user;
			klog(KLOG_DEBUG,"select st=%p,st_flags=%d,events=%d at %p\n",st,st->st_flags,events[n].portev_events,pthread_self());
			if (TEST(events[n].portev_events,POLLIN)) {
				st->eventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
			} else {
				assert(TEST(events[n].portev_events,POLLOUT));
				st->eventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
			}
		}
	}
	//*/
}
bool KPortSelector::write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
#ifndef NDEBUG
	klog(KLOG_DEBUG,"write st=%p\n",st);
#endif
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	//st->e[OP_READ].result = NULL;
	SOCKET sockfd = st->getSocket()->get_socket();
	int ev = POLLOUT;
	if (TEST(st->st_flags, STF_ALWAYS_READ)) {
		SET(ev, POLLIN);
	}
	if(0 == port_associate(kdpfd,PORT_SOURCE_FD, sockfd, ev,st)) {
		SET(st->st_flags,STF_READ|STF_EV|STF_ONE_SHOT);
		return true;
	}
	return false;
}
bool KPortSelector::read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
#ifndef NDEBUG
	klog(KLOG_DEBUG,"read st=%p\n",st);
#endif
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = buffer;
	SOCKET sockfd = st->getSocket()->get_socket();
	if(0 == port_associate(kdpfd,PORT_SOURCE_FD, sockfd,POLLIN,st)) {
		SET(st->st_flags,STF_READ|STF_EV|STF_ONE_SHOT);
		return true;
	}
	klog(KLOG_ERR,"port_associate failed errno = %d\n",errno);
	return false;
}
void KPortSelector::removeSocket(KSelectable *st) {
	if (!TEST(st->st_flags,STF_EV)) {
		return;
	}
	SOCKET sockfd = st->getSocket()->get_socket();
	port_dissociate(kdpfd,PORT_SOURCE_FD,sockfd);
	CLR(st->st_flags, STF_ALWAYS_READ|STF_READ|STF_WRITE|STF_ONE_SHOT|STF_EV);
}
bool KPortSelector::connect(KSelectable *st,resultEvent result,void *arg)
{
	return write(st,result,NULL,arg);
}
bool KPortSelector::next(KSelectable *st,resultEvent result,void *arg)
{
	return write(st,result,NULL,arg);
}
bool KPortSelector::listen(KServer *st,resultEvent result)
{
	return read(st,result,NULL,st);
}
#endif
