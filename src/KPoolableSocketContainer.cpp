/*
 * KPoolableSocketContainer.cpp
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#include "KPoolableSocketContainer.h"
#include "log.h"
#include "time_utils.h"
#include "KSelectorManager.h"
using namespace std;
struct KUpstreamSelectableList {
	kgl_list queue;
	KUpstreamSelectable *st;
};
void KPoolableSocketContainerImp::refreshList(kgl_list *l,bool clean)
{
	for (;;) {
		kgl_list *n = l->prev;
		if (n == l) {
			break;
		}
		KUpstreamSelectableList *socket_list = kgl_list_data(n, KUpstreamSelectableList, queue);
		if (!clean && socket_list->st->expireTime > kgl_current_sec) {
			break;
		}
		size--;
		klist_remove(n);
		assert(socket_list->st->container == NULL);
		socket_list->st->destroy();
		delete socket_list;
	}
}
KPoolableSocketContainerImp::KPoolableSocketContainerImp()
{
	assert(selectorManager.isInit());
	size = 0;
	int count = selectorManager.getSelectorCount()+1;
	l = (kgl_list **)malloc(sizeof(kgl_list *)*count);
	for (int i = 0; i < count; i++) {
		l[i] = new kgl_list;
		klist_init(l[i]);
	}
}
KPoolableSocketContainerImp::~KPoolableSocketContainerImp()
{
	int count = selectorManager.getSelectorCount()+1;
	for (int i = 0; i < count; i++) {
		assert(l[i]->next == l[i] && l[i] == l[i]->prev);
		delete l[i];
	}
	xfree(l);
}
void KPoolableSocketContainerImp::refresh(bool clean)
{
	int count = selectorManager.getSelectorCount()+1;
	for (int i = 0; i < count; i++) {
		refreshList(l[i],clean);
	}
}
kgl_list *KPoolableSocketContainerImp::getPushList(KUpstreamSelectable *st)
{
	int sid = 0;
	assert(l);
	if (st->selector != NULL) {
#ifdef _WIN32
		sid = st->selector->sid + 1;
#else
		
#endif			
	}
	return l[sid];
}
kgl_list *KPoolableSocketContainerImp::getPopList(KConnectionSelectable *st)
{
	if (!klist_empty(l[0])) {
		//Í¨ÓÃ
		return l[0];
	}
	assert(st->selector != NULL);
	int sid = st->selector->sid + 1;
	return l[sid];
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

void KPoolableSocketContainer::unbind(KUpstreamSelectable *st) {	
	release();
}
void KPoolableSocketContainer::gcSocket(KUpstreamSelectable *st,int lifeTime) {
	
	if (this->lifeTime <= 0) {
		//debug("sorry the lifeTime is zero.we must close it\n");
		//noticeEvent(0, st);
		st->destroy();
		return;
	}
	if (lifeTime<0) {
		//debug("the poolableSocket have error,we close it\n");
		//noticeEvent(0, st);
		st->destroy();
		return;
	}	
	if (lifeTime == 0 || lifeTime>this->lifeTime) {
		lifeTime = this->lifeTime;
	}
	st->expireTime = kgl_current_sec + lifeTime;
	putPoolSocket(st);
}
void KPoolableSocketContainer::putPoolSocket(KUpstreamSelectable *st)
{
	lock.Lock();
	if (imp == NULL) {
		imp = new KPoolableSocketContainerImp;
	}
	imp->size++;
	assert(st->container);
	st->container = NULL;
	kgl_list *l = imp->getPushList(st);
	KUpstreamSelectableList *st_list = new KUpstreamSelectableList;
	st_list->st = st;
	l = l->next;
	klist_insert(l, &st_list->queue);
	lock.Unlock();
	unbind(st);
}
KUpstreamSelectable *KPoolableSocketContainer::internalGetPoolSocket(KHttpRequest *rq) {
	if (imp == NULL) {
		imp = new KPoolableSocketContainerImp;
	}
	kgl_list *list_head = imp->getPopList(rq->c);
	imp->refreshList(list_head, false);
	KUpstreamSelectable *socket = NULL;
	kgl_list *n = klist_head(list_head);
	while (n!=list_head) {
		KUpstreamSelectableList *st_list = kgl_list_data(n, KUpstreamSelectableList, queue);
		socket = st_list->st;
		
		imp->size--;
		klist_remove(n);
		delete st_list;	
		bind(socket);
		return socket;
	}
	return NULL;
}
void KPoolableSocketContainer::bind(KUpstreamSelectable *st) {
	assert(st->container==NULL);
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
KUpstreamSelectable *KPoolableSocketContainer::getPoolSocket(KHttpRequest *rq) {
	lock.Lock();
	KUpstreamSelectable *socket = internalGetPoolSocket(rq);
	lock.Unlock();
	//printf("get pool socket=[%p] url=[%s]\n", socket,rq->url->path);
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
