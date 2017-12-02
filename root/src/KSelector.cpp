/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include "KSelector.h"
#include "utils.h"
#include "log.h"
#include "KThreadPool.h"
#include "malloc_debug.h"
#include "KHttpRequest.h"

#include "http.h"
#include "KHttpManage.h"
#include "KRequestQueue.h"
#include "time_utils.h"
inline rb_node *rbInsertRequest(rb_root *root,KBlockRequest *brq,bool &isfirst)
{
	struct rb_node **n = &(root->rb_node), *parent = NULL;
	KBlockRequest *tmp = NULL;
	while (*n) {
		tmp = (KBlockRequest *)((*n)->data);
		INT64 result = brq->active_msec - tmp->active_msec;
		parent = *n;
		if (result < 0) {
			n = &((*n)->rb_left);
		} else if (result > 0) {
			n = &((*n)->rb_right);
			isfirst = false;	
		} else {
			isfirst = false;
			brq->next = tmp;
			//prev指向最后一个
			brq->prev = tmp->prev;
			(*n)->data = brq;
		    return *n;
		}
	}
	rb_node *node = new rb_node;
	node->data = brq;
	brq->next = NULL;
	brq->prev = brq;
	rb_link_node(node, parent, n);
	rb_insert_color(node, root);
	return node;
}
FUNC_TYPE FUNC_CALL selectorThread(void *param) {
	srand((unsigned) (time(NULL) * (INT64)pthread_self()));
	KSelector *selector = (KSelector*) param;
	selector->selectThread();
	KTHREAD_RETURN;
}
void KSelector::selectThread()
{
	thread_id = pthread_self();
	select();
}
KSelector::KSelector() {
	count = 0;
	utm = false;
	tmo_msec = 1000;
	blockBeginNode = NULL;
	blockList.rb_node = NULL;
	for (int i = 0; i < KGL_LIST_BLOCK; i++) {
		klist_init(&list[i]);
	}
}
KSelector::~KSelector() {
	
}
bool KSelector::startSelect() {
	return m_thread.start((void *) this, selectorThread);
}
void KSelector::adjustTime(INT64 t)
{
	listLock.Lock();
	rb_node *node = rb_first(&blockList);
	while (node) {
		KBlockRequest *brq = (KBlockRequest *)node->data;
		assert(brq);
		brq->active_msec += t;
		node = rb_next(node);
	}
	listLock.Unlock();
}
void KSelector::addTimer(KSelectable *rq,timer_func func,void *arg,int msec)
{
	KBlockRequest *brq = new KBlockRequest;
	brq->rq = rq;
	brq->op = STAGE_OP_TIMER;
	brq->active_msec = kgl_current_msec + msec;
	brq->func = func;
	brq->arg = arg;
	listLock.Lock();
	assert(rq==NULL || rq->selector == this);
	if (rq && rq->queue.next) {
		assert(count > 0);
		count--;			
		klist_remove(&rq->queue);
		memset(&rq->queue, 0, sizeof(rq->queue));
	}
	bool isFirst = true;
	rb_node *node = rbInsertRequest(&blockList,brq,isFirst);
	if (isFirst) {
		blockBeginNode = node;
	}
	listLock.Unlock();
}
void KSelector::addList(KSelectable *rq, int list)
{
	rq->tmo_left = rq->tmo;
	assert(rq->selector == this);
	rq->active_msec = kgl_current_msec;
	assert(list >= 0 && list<KGL_LIST_NONE);
	listLock.Lock();
	if (rq->queue.next) {
		klist_remove(&rq->queue);
	} else {
		count++;
	}
	klist_append(&this->list[list], &rq->queue);
	listLock.Unlock();
}
void KSelector::removeList(KSelectable *st)
{
	assert(st->selector == this);
	listLock.Lock();
	if (st->queue.next == NULL) {
		listLock.Unlock();
		return;
	}
	klist_remove(&st->queue);
	memset(&st->queue, 0, sizeof(st->queue));
	assert(count > 0);
	count--;
	listLock.Unlock();
}
void KSelector::checkTimeOut() {
	listLock.Lock();
	for(int i=0;i<KGL_LIST_SYNC;i++){
		for (;;) {
			kgl_list *l = klist_head(&list[i]);
			if (l == &list[i]) {
				break;
			}
			KSelectable *rq = kgl_list_data(l,KSelectable,queue);
			if ((kgl_current_msec - rq->active_msec) < (time_t)timeout[i]) {
				break;
			}
			klist_remove(l);			
			if (rq->tmo_left > 0) {
				//还有额外超时时间
				rq->tmo_left--;
				rq->active_msec = kgl_current_msec;
				klist_append(&list[i], l);
				continue;
			}
			memset(l, 0, sizeof(kgl_list));
#ifndef NDEBUG
			klog(KLOG_DEBUG, "request timeout st=%p\n", (KSelectable *)rq);
#endif
			rq->getSocket()->shutdown(SHUT_RDWR);
			assert(count > 0);
			count--;			
#ifdef _WIN32
			rq->getSocket()->cancelIo();
#endif
		}
	}
	KBlockRequest *activeRequest = NULL;
	KBlockRequest *last = NULL;
	while (blockBeginNode) {
		KBlockRequest *rq = (KBlockRequest *)blockBeginNode->data;
		assert(rq);
		if (kgl_current_msec<rq->active_msec) {
			break;
		}
		rb_node *next = rb_next(blockBeginNode);
		rb_erase(blockBeginNode,&blockList);
		delete blockBeginNode;
		blockBeginNode = next;
		if (activeRequest==NULL) {
			activeRequest = rq;
		} else {
			last->next = rq;
		}
		last = rq->prev;
		assert(last && last->next==NULL);
	}
	listLock.Unlock();
	while (activeRequest) {
		last = activeRequest->next;
		//debug("%p is active\n",activeRequest);
		int op = activeRequest->op;
		assert(op==STAGE_OP_TIMER);
		if (op==STAGE_OP_TIMER) {
			//自定义的定时器
			activeRequest->func(activeRequest->arg);
			delete activeRequest;
			activeRequest = last;
			continue;
		}
		delete activeRequest;
		activeRequest = last;
	}
}
void KSelector::bindSelectable(KSelectable *st)
{
	st->selector = this;
}

unsigned KSelector::getConnection(std::stringstream &s,const char *vh_name,bool translate,volatile uint32_t *total_count)
{
	time_t now_time = kgl_current_sec;
	unsigned totalCount = 0;
	rb_node *node;
	listLock.Lock();
	s << "\n//selector index=" << this->sid << ",count=" << this->count << "\n";
	for(int i = 0;i<KGL_LIST_BLOCK;i++){
		s << "\n//list=" << i << "\n";
		kgl_list *l;
		klist_foreach(l, &list[i])
		{
			KSelectable *st = (KSelectable *)kgl_list_data(l, KSelectable, queue);
			if (st->app_data.rq == NULL) {
				continue;
			}
#ifdef ENABLE_HTTP2
			if (TEST(st->st_flags, STF_APP_HTTP2)) {		
				KHttp2 *http2 = st->app_data.http2;
				
				http2->lock.Lock();
				for (int k = 0; k < kgl_http_v2_index_size(); k++) {
					KHttp2Node *node = http2->streams_index[k];
					while (node) {
						if (node->stream == NULL || node->stream->request==NULL) {
							node = node->index;
							continue;
						}
						KHttpRequest *rq = node->stream->request;
						node = node->index;
						if (vh_name == NULL || (rq->svh && strcmp(rq->svh->vh->name.c_str(), vh_name) == 0)) {
							s << "rqs.push(new Array(";
							getConnectionTr(rq, s, now_time, translate);
							s << "));\n";
							totalCount++;
							if (conf.max_connect_info>0 && katom_inc((void *)total_count)>conf.max_connect_info) {
								http2->lock.Unlock();
								goto done;
							}
						}						
					}
				}
				http2->lock.Unlock();				
				continue;
			}
#endif
			KHttpRequest *rq = st->app_data.rq;
			if (vh_name == NULL || (rq->svh && strcmp(rq->svh->vh->name.c_str(), vh_name) == 0)) {
				s << "rqs.push(new Array(";
				getConnectionTr(rq, s, now_time, translate);
				s << "));\n";
				totalCount++;
				if (conf.max_connect_info>0 && katom_inc((void *)total_count)>conf.max_connect_info) {
					goto done;
				}
			}
		}		
	}
	node = blockBeginNode;
	while (node) {
		s << "\n//list=block\n";
		KBlockRequest *brq = (KBlockRequest *)node->data;
		KHttpRequest *rq = NULL;
		if (brq->rq && !TEST(brq->rq->st_flags, STF_APP_HTTP2)) {
			rq = brq->rq->app_data.rq;
		}
		if (rq) {
		
				if (vh_name == NULL || (rq->svh && strcmp(rq->svh->vh->name.c_str(),vh_name)==0)){
					s << "rqs.push(new Array(";
					getConnectionTr(rq,s,now_time,translate);
					s << "));\n";
					totalCount++;
					if (conf.max_connect_info>0 && katom_inc((void *)total_count)>conf.max_connect_info) {
						goto done;
					}
				}	

		}
		node = rb_next(node);
	}
done:
	listLock.Unlock();
	return totalCount;
}
struct next_timer_func {
	resultEvent func;
	void *arg;
};
static void WINAPI next_call_back(void *arg)
{
	next_timer_func *timer_arg = (next_timer_func *)arg;
	timer_arg->func(timer_arg->arg, 0);
	delete timer_arg;
}
void KSelector::callback(KSelectable *st, resultEvent func, void *arg)
{
	if (st == NULL || !next(st, func, arg)) {
		next_timer_func *timer_arg = new next_timer_func;
		timer_arg->arg = arg;
		timer_arg->func = func;
		addTimer(st, next_call_back, timer_arg, 0);
	}
}
void KSelector::addTimer(KSelectable *st, resultEvent func, void *arg, int msec)
{
	next_timer_func *timer_arg = new next_timer_func;
	timer_arg->arg = arg;
	timer_arg->func = func;
	addTimer(st, next_call_back, timer_arg, 0);
}
void KSelector::getConnectionTr(KHttpRequest *rq, std::stringstream &s,time_t now_time,bool translate)
{	
	s << "'";
	if (translate) {
		s << "rq=" << (void *)(KSelectable *)rq->c << ",st_flags=" << (int)rq->c->st_flags;
		s << ",meth=" << (int)rq->meth;
		s << ",port=" << rq->c->socket->get_remote_port();
#ifdef ENABLE_REQUEST_QUEUE
		s << ",queue=" << rq->queue;
#endif
#ifdef ENABLE_HTTP2
		if (rq->http2_ctx) {
			s << ",http2 out_closed=" << rq->http2_ctx->out_closed;
			s << " in_closed=" << rq->http2_ctx->in_closed;
			s << " read_wait=" << rq->http2_ctx->read_wait;
			s << " write_wait=" << rq->http2_ctx->write_wait;
			s << " send_window=" << rq->http2_ctx->send_window;
			s << " recv_window=" << rq->http2_ctx->recv_window;
		}
#endif
	}
	s << "','" << rq->getClientIp();
	s << "','" << (now_time - rq->request_msec/1000);
	s << "','";
	if (translate) {
		s << klang[rq->getState()];
	} else {
		s << rq->getState();
	}
	s << "','";
	s << rq->getMethod();
	if (rq->mark!=0) {
		s << " " << (int)rq->mark;
	}
	s << "','";
	std::string url = rq->getInfo();
	if (url.size() > 0 && url.find('\'')==std::string::npos) {		
		s << url;
	}
	s << "','";
	if (!translate) {
		sockaddr_i self_addr;
		char ips[MAXIPLEN];
		rq->c->socket->get_self_addr(&self_addr);
		KSocket::make_ip(&self_addr,ips,sizeof(ips));
		s << ips << ":" << self_addr.get_port();
	}
	s << "','";
	const char *referer = rq->parser.getHttpValue("Referer");
	if (referer) {
		s << KXml::encode(referer);
	}
	s << "','";
#ifdef ENABLE_HTTP2
	if (rq->http2_ctx) {
		s << "2";
	}
	else
#endif
		s << "1";
	s << "',";
	if (rq->fetchObj) {
		s << "1";
	} else {
		s << "0";
	}
}
