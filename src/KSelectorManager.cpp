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
#include <vector>
#include <string.h>
#include <stdlib.h>
#include "KSelectorManager.h"
#include "KThreadPool.h"
#include "log.h"
#include "lang.h"
#include "malloc_debug.h"
#include "KEpollSelector.h"
#include "KKqueueSelector.h"
#include "KPortSelector.h"

#include "KVirtualHost.h"
#include "time_utils.h"
intmap m_ip;
KMutex ipLock;
#ifdef RQ_LEAK_DEBUG
kgl_list all_connection;
#endif
unsigned total_connect = 0;
KSelectorManager selectorManager;
using namespace std;
void adjustTime(INT64 diffTime)
{
	selectorManager.adjustTime(diffTime);
}
string get_connect_per_ip() {

	stringstream s;
	list<connect_per_ip_t> m_per_ip;
	list<connect_per_ip_t>::iterator it;
	bool have_insert = false;
	connect_per_ip_t m_tmp;
	s
			<< "<html><head><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
	s << LANG_MAX_CONNECT_PER_IP << conf.max_per_ip;
	int total = 0;
	ipLock.Lock();
	intmap::iterator it2;
	for (it2 = m_ip.begin(); it2 != m_ip.end(); it2++) {
		have_insert = false;
		m_tmp.ip = (*it2).first;
		m_tmp.per_ip = (*it2).second;
		total += m_tmp.per_ip;
		for (it = m_per_ip.begin(); it != m_per_ip.end(); it++) {
			if (m_tmp.per_ip > (*it).per_ip) {
				m_per_ip.insert(it, m_tmp);
				have_insert = true;
				break;
			}
		}
		if (!have_insert) {
			m_per_ip.push_back(m_tmp);
		}
	}
	s << "<table border=1><tr><td>" << LANG_IP << "</td><td>"
			<< LANG_CONNECT_COUNT << "</td></tr>";
	for (it = m_per_ip.begin(); it != m_per_ip.end(); it++) {

		s << "<tr";
		/*if ((*it).per_ip > conf.max_per_ip) {
			s << " bgcolor=#CCFF99";
		}
		*/
		char ip[MAXIPLEN];
		KSocket::make_ip(&(*it).ip,ip,sizeof(ip));
		s << "><td>" << ip << "</td><td>" << (*it).per_ip << "</td>";
		s << "</tr>";
	}
	ipLock.Unlock();
	s << "</table>\n" ;
	s << "<!-- total connect = " << total << " -->\n";
	s << endTag() << "</body></html>";
	return s.str();
	//	return "";
}

int set_max_per_ip(int value) {
	conf.max_per_ip = value;
	return value;
}
KSelector *KSelectorManager::newSelector() {
#ifdef HAVE_SYS_EPOLL_H
	return new KEpollSelector();
#elif BSD_OS
	return new KKqueueSelector();
	
#else
	return NULL;
#endif
}
KSelectorManager::KSelectorManager() {
	selectors = NULL;
	sizeHash = 0;
	onReadyList = NULL;
}

void KSelectorManager::init(unsigned size)
{
#ifdef RQ_LEAK_DEBUG
	klist_init(&all_connection);
#endif
	for (int i=0;i<7;i++) {
		count = (1<<i);
		if(count==(int)size){
			break;
		}
		if (count>(int)size) {
			count--;
			break;
		}
	}
	sizeHash = count - 1;
#ifndef _WIN32
/*
	if (conf.worker<=1 && conf.select_count>1) {
		count++;
		listenIndex++;
	}
*/
#endif
	selectors = (KSelector **)xmalloc(sizeof(KSelector *)*count);
	for(int i=0; i<count; i++){
		selectors[i] = newSelector();
		selectors[i]->sid = i;
	}
	setTimeOut();
	//call onReadyList
	while (onReadyList) {
		onReadyList->call_back(onReadyList->arg);
		KOnReadyItem *n = onReadyList->next;
		delete onReadyList;
		onReadyList = n;
	}
}
void KSelectorManager::start()
{
	for(int i=0;i<count;i++){
	      selectors[i]->startSelect();
	}
}
void KSelectorManager::setTimeOut()
{
	if (conf.connect_time_out <= 0) {
		conf.connect_time_out = conf.time_out;
	}
	for (int i=0; i<count; i++) {	
		selectors[i]->tmo_msec = 100;
	}
	//set_time_out(KGL_LIST_KA,(conf.keep_alive>0 ? conf.keep_alive : conf.time_out) * 1000);
	set_time_out(KGL_LIST_RW,conf.time_out * 1000);
	set_time_out(KGL_LIST_CONNECT,conf.connect_time_out * 1000);
	selectors[0]->utm = true;
}
KSelectorManager::~KSelectorManager() {

}
#ifdef MALLOCDEBUG
void KSelectorManager::close()
{
	for (int i = 0; i<count; i++) {
		selectors[i]->timeout[KGL_LIST_RW] = 1;
		selectors[i]->timeout[KGL_LIST_CONNECT] = 1;
		selectors[i]->close();
	}
}
void KSelectorManager::destroy() {
	xfree(selectors);
}
#endif
void KSelectorManager::flush(time_t nowTime) {

}
bool KSelectorManager::listen(KServer *server,resultEvent result)
{
	if (server->server_selectable == NULL) {
		return false;
	}
	if (!server->is_multi_selectale()) {
		server->server_selectable->selector = getSelector();
		server->addRef();
		server->server_selectable->selector->listen(server->server_selectable, result);		
		return true;
	}
	KServerSelectable *ss = server->server_selectable;
	for (int i = 0; i < count; i++) {
		ss->selector = selectors[i];
		server->addRef();
		ss->selector->listen(ss, result);
		ss = ss->next;
	}
	assert(ss == NULL);
	return true;
}
void get_connection_call_back(void *arg,int got)
{
	KConnectionInfo *ci = (KConnectionInfo *)arg;
	ci->count = ci->selector->getConnection(ci->info,ci->vh_name,ci->translate,ci->total_count);
	ci->wait.notice();
}
std::string KSelectorManager::getConnectionInfo(int &totalCount, int debug,const char *vh_name,bool translate) {
	//time_t now_time = kgl_current_sec;
	totalCount = 0;
	stringstream s;
	uint32_t total_count = 0;
	KConnectionInfo *ci = new KConnectionInfo[sizeHash+1];
	for (unsigned i=0;i<=sizeHash;i++) {
		ci[i].translate = translate;
		ci[i].vh_name = vh_name;
		ci[i].count = 0;
		ci[i].total_count = &total_count;
		ci[i].selector = selectors[i];
		if (!selectors[i]->next(get_connection_call_back, &ci[i])) {
			ci[i].wait.notice();
		}
	}
	for (unsigned i=0;i<=sizeHash;i++) {
		ci[i].wait.wait();
		s << ci[i].info.str();
		totalCount += ci[i].count;
	}
	delete[] ci;
	return s.str();
}
#ifdef RQ_LEAK_DEBUG
void KSelectorManager::dump_all_connection()
{
	int connection_count = 0;
	ipLock.Lock();
	kgl_list *l;
	klist_foreach(l, &all_connection)
	{
		connection_count++;
		KSelectable *st = (KSelectable *)kgl_list_data(l, KSelectable, queue_edge);
		char ip[MAXIPLEN];
		KConnectionSelectable *c = static_cast<KConnectionSelectable *>(st);
		c->socket->get_remote_ip(ip, MAXIPLEN);
		klog(KLOG_ERR,"%s:%d\tc=[%p]\tqueue=[%p]\tst_flags=[%d]\ttime=[%d]\n", ip,c->socket->get_remote_port(), c, c->queue.next,c->st_flags, (int)(kgl_current_msec - c->active_msec));
	}
	klog(KLOG_ERR, "dump_count=[%d],total_connect=[%d]\n", connection_count, total_connect);
	ipLock.Unlock();
}
#endif
void KSelectorManager::onReady(void (WINAPI *call_back)(void *arg),void *arg)
{
	if (isInit()) {
		call_back(arg);
		return;
	}
	KOnReadyItem *onReadyItem = new KOnReadyItem();
	onReadyItem->call_back = call_back;
	onReadyItem->arg = arg;
	onReadyItem->next = onReadyList;
	onReadyList = onReadyItem;
}
