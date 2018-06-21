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
#ifndef KSELECTORMANAGER_H_
#define KSELECTORMANAGER_H_
#include<map>
#include<list>
#include<string>
#include "KSocket.h"
#include "KHttpRequest.h"
#include "KSelector.h"
#include "KMutex.h"
#include "KServer.h"
#include "time_utils.h"
#include "KVirtualHostManage.h"
#include "KIpAclBase.h"
#include "katom.h"
#include "KCondWait.h"
#include "KList.h"
#define MAX_SELECTORS	2
#define ADD_REQUEST_SUCCESS                                     0
#define ADD_REQUEST_TOO_MANY_CONNECTION 	1
#define ADD_REQUEST_PER_IP_LIMIT                                2
#define ADD_REQUEST_UNKNOW_ERROR                        3
#define MAX_UNUSED_REQUEST                             1024
#define MAX_UNUSED_REQUEST_HASH                        1023
void handleMultiListen(KSelectable *st,int got);
typedef std::map<ip_addr,unsigned> intmap;
struct KOnReadyItem
{
	void (WINAPI *call_back)(void *arg);
	void *arg;
	KOnReadyItem *next;
};
struct KConnectionInfo {
	const char *vh_name;
	bool translate;
	std::stringstream info;
	volatile uint32_t *total_count;
	unsigned count;
	KCondWait wait;
	KSelector *selector;
};
#ifdef RQ_LEAK_DEBUG
extern kgl_list all_connection;
#endif
extern intmap m_ip;
extern KMutex ipLock;
extern unsigned total_connect ;
struct KPerIpConnect
{
	IP_MODEL src;
	unsigned max;
	bool deny;
	KPerIpConnect *next; 
};
struct connect_per_ip_t {
        ip_addr ip;
        unsigned per_ip;
};
inline const char *getErrorMsg(int errorCode) {
        switch (errorCode) {
        case ADD_REQUEST_SUCCESS:
                return "success";
        case ADD_REQUEST_TOO_MANY_CONNECTION:
                return "too many connection";
        case ADD_REQUEST_PER_IP_LIMIT:
                return "per ip limit";
        }
        return "unknow error";
}
inline int addRequest(KConnectionSelectable *c) {
        unsigned max = (unsigned)conf.max;
		unsigned max_per_ip = conf.max_per_ip;
		assert(c->ls);
		if (TEST(c->ls->model,WORK_MODEL_MANAGE)) {
			if (max>0) {
				max+=100;
			}
			if (max_per_ip>0) {
				max_per_ip+=10;
			}
		}
		bool deny = false;
		intmap::iterator it2;
		ip_addr ip;
        ipLock.Lock();
        if (max>0 && total_connect >= max) {
				//printf("total_connect=%d,max=%d\n",total_connect,max);
                ipLock.Unlock();
                return ADD_REQUEST_TOO_MANY_CONNECTION;
        }
		if (conf.keep_alive_count>0 && total_connect>=conf.keep_alive_count) {
			SET(c->st_flags,STF_NO_KA);
		}
		KPerIpConnect *per_ip = conf.per_ip_head;
		if (per_ip || max_per_ip>0) {
			c->socket->get_remote_addr(&ip);
		}
		while (per_ip) {
			if (KIpAclBase::matchIpModel(per_ip->src,ip)) {
				max_per_ip = per_ip->max;
				deny = per_ip->deny;
				break;
			}
			per_ip = per_ip->next;
		}
		if (deny) {
			goto max_per_ip;
		}
        if (max_per_ip==0) {
				total_connect++;
#ifdef RQ_LEAK_DEBUG
				klist_append(&all_connection, &c->queue_edge);
#endif
                ipLock.Unlock();
                SET(c->st_flags,STF_RQ_OK);
                return ADD_REQUEST_SUCCESS;
        }
       
        it2 = m_ip.find(ip);
        if (it2 == m_ip.end()) {
                m_ip.insert(std::pair<ip_addr, unsigned> (ip, 1));
        } else {
                if ((*it2).second >= max_per_ip) {
                        goto max_per_ip;
                }
                (*it2).second++;
        }
		total_connect++;
#ifdef RQ_LEAK_DEBUG
		klist_append(&all_connection, &c->queue_edge);
#endif
        ipLock.Unlock();
        SET(c->st_flags,STF_RQ_OK|STF_RQ_PER_IP);
        return ADD_REQUEST_SUCCESS;
        max_per_ip: ipLock.Unlock();
		
        return ADD_REQUEST_PER_IP_LIMIT;
}
inline void delRequest(KConnectionSelectable *c) {
	if(!TEST(c->st_flags,STF_RQ_PER_IP)){
		if (!TEST(c->st_flags, STF_RQ_OK)) {
			return;
		}
		ipLock.Lock();
		total_connect--;
#ifdef RQ_LEAK_DEBUG
		klist_remove(&c->queue_edge);
#endif
		ipLock.Unlock();
		return ;
	}
	ip_addr ip;
	c->socket->get_remote_addr(&ip);
	intmap::iterator it2;
	ipLock.Lock();
	total_connect--;
#ifdef RQ_LEAK_DEBUG
	klist_remove(&c->queue_edge);
#endif
	it2 = m_ip.find(ip);
	assert(it2!=m_ip.end());
	(*it2).second--;
	if ((*it2).second == 0) {
		m_ip.erase(it2);
	}
	ipLock.Unlock();
}
class KSelectorManager
{
public:
	KSelectorManager();
	virtual ~KSelectorManager();
	bool listen(KServer *st,resultEvent result);
#ifdef MALLOCDEBUG
	void destroy();
	void close();
#endif
	void init(unsigned size);
	inline KSelector *getSelectorByIndex(int index)
	{
		return selectors[index & sizeHash];
	}
	inline KSelector *getSelector()
	{
		unsigned i = index;
		for (int j = 0; j < count; j++,i++) {
			KSelector *selector = selectors[i & sizeHash];
			KSelector *next_selector = selectors[(i + 1) & sizeHash];
			if (selector->count < next_selector->count + 64) {
				index = i + 1;
				return selector;
			}
		}
		assert(false);
		return getSelectorByIndex(index++);
	}
	void adjustTime(INT64 t)
	{
		for (int i=0;i<count;i++) {
			selectors[i]->adjustTime(t);
		}
	}
	inline bool startRequest(KConnectionSelectable *c)
	{
#ifdef ENABLE_STAT_STUB
		katom_inc64((void *)&kgl_total_servers);
#endif
		assert(c->selector->is_same_thread());
		int ret = addRequest(c);
		if (unlikely(ret != ADD_REQUEST_SUCCESS)) {
#ifndef _WIN32
#ifdef KSOCKET_SSL
			if (!c->isSSL()) {
#endif
				c->socket->write_all("HTTP/1.0 503 Service Unavailable\r\n\r\nServer is busy,try it again");
#ifdef KSOCKET_SSL
			}
#endif
#endif
			char ips[MAXIPLEN];
			c->socket->get_remote_ip(ips, sizeof(ips));
			klog(KLOG_ERR, "cann't addRequest to thread %s:%d %s\n", ips, c->socket->get_remote_port(),
				getErrorMsg(ret));
			c->real_destroy();
			return false;
		}
#ifdef ENABLE_STAT_STUB
		katom_inc64((void *)&kgl_total_accepts);
#endif
#ifdef KSOCKET_SSL
		if (c->isSSL()) {
			KSSLSocket *socket = static_cast<KSSLSocket *>(c->socket);
			if (!socket->bind_fd()) {
				c->real_destroy();
				return false;
			}
			KHttpRequest *rq = new KHttpRequest(c);
			c->app_data.rq = rq;
			rq->workModel = c->ls->model;
			rq->filter_flags = 0;
			rq->begin_time_msec = kgl_current_msec;
			SSL_set_ex_data(socket->getSSL(), kangle_ssl_conntion_index, c);
			//ssl accept
			c->read(rq, resultSSLAccept, NULL);
			return true;
		}
#endif
		KHttpRequest *rq = new KHttpRequest(c);
		c->app_data.rq = rq;
		rq->workModel = c->ls->model;
		rq->init(NULL);
#ifdef WORK_MODEL_TCP
		if (TEST(rq->workModel,WORK_MODEL_TCP)) {
			rq->meth = METH_CONNECT;
			handleStartRequest(rq,0);
			return true;
		}
#endif
		c->read(rq, resultRequestRead, bufferRequestRead);
		return true;
	}
	void setTimeOut();
	int getSelectorCount()
	{
		return sizeHash+1;
	}
	void flush(time_t nowTime);
	const char *getName()
	{
		return selectors[0]->getName();
	}
	static KSelector *newSelector();
	void start();
	bool isInit()
	{
		return selectors!=NULL;
	}
	void onReady(void (WINAPI *call_back)(void *arg),void *arg);
public:
#ifdef RQ_LEAK_DEBUG
	void dump_all_connection();
#endif
	std::string getConnectionInfo(int &totalCount,int debug,const char *vh_name,bool translate=true);
private:
	void set_time_out(int time_out_index, int value)
	{
		if (value <= 0) {
			value = 60000;
		}
		for (int i = 0; i<count; i++) {
			selectors[i]->timeout[time_out_index] = value;
		}
	}
	KSelector **selectors;
	int count;
	unsigned sizeHash;
	unsigned index;
	KMutex unusedLock;
	KOnReadyItem *onReadyList;
};
extern KSelectorManager selectorManager;
#endif /*KSELECTORMANAGER_H_*/
