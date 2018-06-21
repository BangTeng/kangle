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
struct adjust_time_param
{
	KSelector *selector;
	INT64 t;
};
void next_adjust_time(void *arg, int got)
{
	adjust_time_param *param = (adjust_time_param *)arg;
	param->selector->adjustTime(param->t);
	delete param;
}
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
			//brq->prev = tmp->prev;
			(*n)->data = brq;
		    return *n;
		}
	}
	rb_node *node = new rb_node;
	node->data = brq;
	brq->next = NULL;
	//brq->prev = brq;
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
#ifdef LINUX
	cpu_set_t mask;
	CPU_ZERO(&mask);
        CPU_SET(sid, &mask);
	if (pthread_setaffinity_np(thread_id, sizeof(mask), &mask) < 0) {
            klog(KLOG_ERR,"sched_setaffinity error. errno=[%d]\n",errno);
        }
#endif
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
#ifdef MALLOCDEBUG
	closed_flag = false;
#endif
}
KSelector::~KSelector() {
	
}
bool KSelector::startSelect() {
	return m_thread.start((void *) this, selectorThread);
}
void KSelector::adjustTime(INT64 t)
{
	if (!is_same_thread()) {
		adjust_time_param *param = new adjust_time_param;
		param->selector = this;
		param->t = t;
		if (!next(next_adjust_time, param)) {
			delete param;
		}
		return;
	}
	rb_node *node = rb_first(&blockList);
	while (node) {
		KBlockRequest *brq = (KBlockRequest *)node->data;
		assert(brq);
		brq->active_msec += t;
		node = rb_next(node);
	}
}
void KSelector::add_timer(resultEvent func,void *arg,int msec, KSelectable *st)
{
	KBlockRequest *brq = new KBlockRequest;
	brq->active_msec = kgl_current_msec + msec;
	brq->func = func;
	brq->arg = arg;
	brq->st = st;
	assert(is_same_thread());
	bool is_first = true;
	rb_node *node = rbInsertRequest(&blockList, brq, is_first);
	if (is_first) {
		blockBeginNode = node;
	}
	assert(blockBeginNode==rb_first(&blockList));
}
void KSelector::add_list(KSelectable *rq, int list)
{
	assert(is_same_thread());
	rq->tmo_left = rq->tmo;
	assert(rq->selector == this);
	rq->active_msec = kgl_current_msec;
	assert(list >= 0 && list<KGL_LIST_NONE);
	if (rq->queue.next) {
		klist_remove(&rq->queue);
	} else {
		count++;
	}
	klist_append(&this->list[list], &rq->queue);
}
void KSelector::remove_list(KSelectable *st)
{
	assert(is_same_thread());
	assert(st->selector == this);
	if (st->queue.next == NULL) {
		return;
	}
	klist_remove(&st->queue);
	memset(&st->queue, 0, sizeof(st->queue));
	assert(count > 0);
	count--;
}
void KSelector::checkTimeOut() {
	for (int i=0;i<KGL_LIST_SYNC;i++) {
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
			memset(l, 0, sizeof(kgl_list));
			if (rq->tmo_left > 0) {
				rq->tmo_left--;
				rq->active_msec = kgl_current_msec;
				klist_append(&list[i], l);
				continue;
			}
			memset(l, 0, sizeof(kgl_list));
#ifndef NDEBUG
			klog(KLOG_DEBUG, "request timeout st=%p\n", (KSelectable *)rq);
#endif
			assert(count > 0);
			if (TEST(rq->st_flags, STF_RTIME_OUT | STF_READ) == (STF_RTIME_OUT | STF_READ)) {
				//set read time out
				klist_append(&list[i], l);
				rq->active_msec = kgl_current_msec;
				assert(rq->e[OP_READ].result);
				rq->e[OP_READ].result(rq->e[OP_READ].arg, ST_ERR_TIME_OUT);
				continue;
			}
			count--;
			rq->shutdown_socket();
		}
	}
	while (blockBeginNode) {
		KBlockRequest *rq = (KBlockRequest *)blockBeginNode->data;
		assert(rq);
		if (kgl_current_msec<rq->active_msec) {
			break;
		}
		rb_node *next = rb_next(blockBeginNode);
		rb_erase(blockBeginNode,&blockList);
		while (rq) {
			KBlockRequest *rq_next = rq->next;
			rq->func(rq->arg, 0);
			delete rq;
			rq = rq_next;
		}
		delete blockBeginNode;
		blockBeginNode = next;
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
	assert(is_same_thread());
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
								goto done;
							}
						}						
					}
				}
								
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
		while (brq) {
			KHttpRequest *rq = NULL;
			if (brq->st && !TEST(brq->st->st_flags, STF_APP_HTTP2)) {
				rq = brq->st->app_data.rq;
			}
			if (rq) {
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
			brq = brq->next;
		}
		node = rb_next(node);
	}

done:
	return totalCount;
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
	s << "','" << (now_time - rq->begin_time_msec/1000);
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
