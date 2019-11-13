/*
 * KPoolableSocketContainer.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#include "KPoolableSocketContainer.h"
#include "log.h"
#include "time_utils.h"
#include "kselector_manager.h"
#include "KHttpRequest.h"
#include "KSink.h"
#include "KTsUpstream.h"

using namespace std;
struct KUpstreamSelectableList {
	kgl_list queue;
	KPoolableUpstream *st;
};
kev_result next_destroy(void *arg, int got)
{
	KPoolableUpstream *st = (KPoolableUpstream *)arg;
	st->Destroy();
	return kev_destroy;
}
void SafeDestroyUpstream(KUpstream *st)
{
	kselector *selector = st->GetConnection()->st.selector;
	if (selector != NULL && selector != kgl_get_tls_selector()) {
		kgl_selector_module.next(selector, next_destroy, st, 0);
	} else {
		st->Destroy();
	}
}
void KPoolableSocketContainerImp::refreshList(kgl_list *l,bool clean)
{
	for (;;) {
		kgl_list *n = l->prev;
		if (n == l) {
			break;
		}
		KUpstreamSelectableList *socket_list = kgl_list_data(n, KUpstreamSelectableList, queue);
		if (!clean && socket_list->st->expire_time > kgl_current_sec) {
			break;
		}
		size--;
		klist_remove(n);
		assert(socket_list->st->container == NULL);
		KPoolableUpstream *st = socket_list->st;
		delete socket_list;	
		SafeDestroyUpstream(st);
	}
}
KPoolableSocketContainerImp::KPoolableSocketContainerImp()
{
	assert(is_selector_manager_init());
	size = 0;
	klist_init(&head);
}
KPoolableSocketContainerImp::~KPoolableSocketContainerImp()
{
	kassert(head.next == &head);
	kassert(head.prev == &head);
}
void KPoolableSocketContainerImp::refresh(bool clean)
{
	refreshList(&head,clean);
}
KPoolableSocketContainer::KPoolableSocketContainer() {
	lifeTime = 0;
	imp = NULL;
}
KPoolableSocketContainer::~KPoolableSocketContainer() {
	if (imp) {
		imp->refresh(true);
		delete imp;
	}
}

void KPoolableSocketContainer::unbind(KPoolableUpstream *st) {
	release();
}
void KPoolableSocketContainer::gcSocket(KPoolableUpstream *st,int lifeTime,time_t base_time) {
	if (this->lifeTime <= 0) {
		//debug("sorry the lifeTime is zero.we must close it\n");
		//noticeEvent(0, st);
		st->Destroy();
		return;
	}
	if (lifeTime<0) {
		//debug("the poolableSocket have error,we close it\n");
		//noticeEvent(0, st);
		st->Destroy();
		return;
	}	
	if (lifeTime == 0 || lifeTime>this->lifeTime) {
		lifeTime = this->lifeTime;
	}
	st->expire_time = base_time + lifeTime;
	time_t now_time = kgl_current_sec;
	if (st->expire_time < now_time || base_time > now_time) {
		st->Destroy();
		return;
	}
	putPoolSocket(st);
}
void KPoolableSocketContainer::putPoolSocket(KPoolableUpstream *st)
{
	st->OnPushContainer();
	lock.Lock();
	if (imp == NULL) {
		imp = new KPoolableSocketContainerImp;
	}
	imp->size++;
	assert(st->container);
	st->container = NULL;
	kgl_list *l = imp->GetList();
	KUpstreamSelectableList *st_list = new KUpstreamSelectableList;
	st_list->st = st;
	l = l->next;
	klist_insert(l, &st_list->queue);
	lock.Unlock();
	unbind(st);
}
KUpstream *KPoolableSocketContainer::internalGetPoolSocket(KHttpRequest *rq) {
	if (imp == NULL) {
		imp = new KPoolableSocketContainerImp;
	}
	kgl_list *list_head = imp->GetList();
	imp->refreshList(list_head, false);
	KUpstream *socket = NULL;
	kgl_list *n = klist_head(list_head);
	while (n!=list_head) {
		KUpstreamSelectableList *st_list = kgl_list_data(n, KUpstreamSelectableList, queue);
		socket = st_list->st;
		if (socket->IsMultiStream()) {
			KUpstream *us = st_list->st->NewStream(rq);
			if (us==NULL) {
				imp->size--;
				kgl_list *next = n->next;
				klist_remove(n);
				delete st_list;
				SafeDestroyUpstream(socket);
				n = next;
				continue;
			}
			bind(us);
			return us;
		}
		imp->size--;
		klist_remove(n);
		delete st_list;	
		bind(socket);
		return socket;
	}
	return NULL;
}
void KPoolableSocketContainer::bind(KPoolableUpstream *st) {
	kassert(st->container==NULL);
	st->container = this;
	refsLock.Lock();
	refs++;
	refsLock.Unlock();
}
void KPoolableSocketContainer::setLifeTime(int lifeTime) {
	this->lifeTime = lifeTime;
	if (lifeTime <= 0) {
		clean();
	}
}
void KPoolableSocketContainer::refresh(time_t nowTime) {
	lock.Lock();
	if (imp) {
		imp->refresh(false);
	}
	lock.Unlock();
}
KUpstream *KPoolableSocketContainer::getPoolSocket(KHttpRequest *rq) {
	lock.Lock();
	KUpstream *socket = internalGetPoolSocket(rq);
	lock.Unlock();
	if (socket==NULL) {
		return NULL;
	}
	kselector *selector = socket->GetConnection()->st.selector;
	if (selector!=NULL  && selector!=rq->sink->GetSelector()) {
		return new KTsUpstream(rq->sink->GetSelector(),socket);
	}
	return socket;
}
void KPoolableSocketContainer::clean()
{
	lock.Lock();
	if (imp) {
		imp->refresh(true);
		assert(imp->size == 0);
	}
	lock.Unlock();
}
