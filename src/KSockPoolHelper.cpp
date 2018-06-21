/*
 * KSockPoolHelper.cpp
 *
 *  Created on: 2010-6-4
 *      Author: keengo
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
#include <sstream>
#include "KSockPoolHelper.h"
#include "utils.h"
#include "KAsyncFetchObject.h"
#include "KThreadPool.h"
#include "KAddr.h"
struct KSockPoolDns
{
	KHttpRequest *rq;
	KUpstreamSelectable *socket;
	KSockPoolHelper *sh;
};
using namespace std;
static void monitor_connect_result(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	KSockPoolHelper *sph = (KSockPoolHelper *)rq->hot;
	if (got < 0) {
		sph->disable();
	} else {
		if (rq->c->socket->write("H", 1) <= 0) {			
			sph->disable();
		} else {
			sph->enable();
		}
	}
	rq->c->socket->close();
	delete rq;
	int monitor_tick = (int)(kgl_current_msec - sph->monitor_start_time);
	if (monitor_tick <= 0) {
		monitor_tick = 1;
	}
	if (monitor_tick > 30000) {
		monitor_tick = 30000;
	}	
	if (sph->avg_monitor_tick == 0) {
		sph->avg_monitor_tick = monitor_tick;
	} else {
		sph->avg_monitor_tick = (int)(0.9 * float(sph->avg_monitor_tick) + 0.1 * float(monitor_tick));
	}	
	sph->monitorNextTick();
}
static void start_monitor_call_back(void *arg,int got)
{
	KSockPoolHelper *sph = (KSockPoolHelper *)arg;
	sph->start_monitor_call_back();
}
void next_monitor_call_back(void *arg, int got)
{
	KSockPoolHelper *sph = (KSockPoolHelper *)arg;
	sph->selector->add_timer(::start_monitor_call_back, arg, got,NULL);
}
static void WINAPI first_start_monitor_call_back(void *arg)
{
	//rand start monitor request.
	KSockPoolHelper *sph = (KSockPoolHelper *)arg;
	sph->selector = selectorManager.getSelector();
	sph->selector->next(next_monitor_call_back, arg, rand() % 10000);
}
#if 0
static KTHREAD_FUNCTION asyncSockPoolDnsMonitorCallBack(void *data,int msec)
{
	KSockPoolDns *spdns = (KSockPoolDns *)data;
	assert(spdns->socket);
	KHttpRequest *rq = spdns->rq;
	if (msec<0
		|| msec > (int)conf.connect_time_out * 1000
		|| !spdns->sh->real_connect(rq, spdns->socket,true)) {
		spdns->sh->selector->next(next_monitor_name_resolved_call_back, spdns, 0);	
		KTHREAD_RETURN;
	}
	spdns->sh->selector->next(next_monitor_name_resolved_call_back, spdns,1);	
	KTHREAD_RETURN;
}
static KTHREAD_FUNCTION asyncSockPoolDnsCallBack(void *data,int msec)
{
	//printf("**********************************asyncSockPoolDnsCallBack\n");
	KSockPoolDns *spdns = (KSockPoolDns *)data;
	assert(spdns->socket);
	KHttpRequest *rq = spdns->rq;
	if (msec > (int)conf.connect_time_out * 1000
		|| !spdns->sh->real_connect(rq,spdns->socket,true)) {
		spdns->socket->isBad(BadStage_Connect);
		spdns->socket->destroy();
		spdns->socket = NULL;
	}
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->connectCallBack(rq,spdns->socket,true);
	spdns->sh->release();
	delete spdns;
	KTHREAD_RETURN;
}
#endif
static void sockpool_name_resovled_monitor_call_back(void *arg, KAddr *addr)
{
	sockaddr_i a;
	KSockPoolDns *spdns = (KSockPoolDns *)arg;
	assert(spdns->socket);
	KHttpRequest *rq = spdns->rq;
	if (addr==NULL ||
		!addr->get_addr(spdns->sh->port, a) ||
		!spdns->sh->connect_addr(rq, spdns->socket, a)) {
		delete rq;
		spdns->socket->destroy();
		spdns->sh->disable();
		spdns->sh->monitorNextTick();
		delete spdns;
		return;
	}
	spdns->sh->monitorConnectStage(rq, spdns->socket);
	delete spdns;
}
static void sockpool_name_resovled_call_back(void *arg, KAddr *addr)
{
	sockaddr_i a;
	KSockPoolDns *spdns = (KSockPoolDns *)arg;
	assert(spdns->socket);
	KHttpRequest *rq = spdns->rq;
	if (addr == NULL ||
		!addr->get_addr(spdns->sh->port, a) ||
		!spdns->sh->connect_addr(rq,spdns->socket,a)) {
		spdns->socket->isBad(BadStage_Connect);
		spdns->socket->destroy();
		spdns->socket = NULL;
	}
	KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
	fo->connectCallBack(rq, spdns->socket, true);
	spdns->sh->release();
	delete spdns;
}
FUNC_TYPE FUNC_CALL checkNodeActive(void *param)
{
	KSockPoolHelper *sockHelper = (KSockPoolHelper *)param;
	sockHelper->syncCheckConnect();
	sockHelper->release();
	KTHREAD_RETURN;
}
void KSockPoolHelper::monitorConnectStage(KHttpRequest *rq, KUpstreamSelectable *us)
{
	rq->c = us;
	rq->c->selector = selector;
	rq->hot = (char *)this;
	us->connect(rq, monitor_connect_result);
}
void KSockPoolHelper::start_monitor_call_back()
{
#ifdef MALLOCDEBUG
	if (quit_program_flag > 0) {
		return;
	}
#endif
	if (!monitor) {
		release();
		return;
	}
	KHttpRequest *rq = new KHttpRequest(NULL);
	rq->init(NULL);
	bool need_name_resolved = false;
	KUpstreamSelectable *us = newConnection(rq, need_name_resolved);
	if (us == NULL) {
		disable();
		delete rq;
		monitorNextTick();
		return;
	}
	this->monitor_start_time = kgl_current_msec;
	if (need_name_resolved) {
		KSockPoolDns *spdns = new KSockPoolDns;
		spdns->socket = us;
		spdns->rq = rq;
		spdns->sh = this;
		find_addr(this->host.c_str(), kgl_addr_ip, sockpool_name_resovled_monitor_call_back, spdns, selector);
		return;
	}
	monitorConnectStage(rq, us);
}
KSockPoolHelper::KSockPoolHelper() {
	ip = NULL;
	tryTime = 0;
	error_count = 0;
	isUnix = false;
	max_error_count = 5;
	hit = 0;
	weight = 1;
	avg_monitor_tick = 0;
	monitor = false;
#ifdef ENABLE_UPSTREAM_SSL
	ssl_ctx = NULL;
#endif
	total_error = 0;
	total_connect = 0;
	sign = false;
	selector = NULL;
}
KSockPoolHelper::~KSockPoolHelper() {
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl_ctx) {
		KSSLSocket::clean_ctx(ssl_ctx);
	}
#endif
	if (ip) {
		free(ip);
	}
}
void KSockPoolHelper::monitorNextTick()
{
	selector->add_timer(::start_monitor_call_back,this,error_try_time*1000,NULL);
}
void KSockPoolHelper::startMonitor()
{
	if (monitor) {
		return;
	}
	monitor = true;
	addRef();
	selectorManager.onReady(::first_start_monitor_call_back,this);
}
void KSockPoolHelper::checkActive()
{
	//TODO:现在采用最简单的线程去检测，以后可以加到主循环里面去检测，节省资源。
	if (monitor) {
		return;
	}
	addRef();
	if (!m_thread.start(this,checkNodeActive)) {
		release();
	}
}

void KSockPoolHelper::syncCheckConnect()
{
	KClientSocket socket;
	bool result = false;
	int tmo = 5;
	sockaddr_i *bind_addr = NULL;
	if (ip) {
		bind_addr = new sockaddr_i;
		if (!KSocket::getaddr(ip,0,bind_addr,AF_UNSPEC,AI_NUMERICHOST)) {
			delete bind_addr;
			return;
		}
	}
	//TODO:已知问题,如果检测过程中对host修改，可能导致异常。所以未来一定要放到主循环中去检测
#ifdef KSOCKET_UNIX
	if (isUnix) {
		result = socket.connect(host.c_str(),tmo);
	} else {
#endif
		result = socket.connect(this->host.c_str(),this->port,tmo,bind_addr);
#ifdef KSOCKET_UNIX
	}
#endif
	if (bind_addr) {
		delete bind_addr;
	}
	if (result) {
		enable();
	}
}

KUpstreamSelectable *KSockPoolHelper::getConnection(KHttpRequest *rq, bool &half, bool &need_name_resolved)
{
	KUpstreamSelectable *socket = NULL;
	if (!TEST(rq->flags, RQ_UPSTREAM_ERROR|RQ_HAS_CONNECTION_UPGRADE)) {
		//如果是发生错误重连或upgrade的连接，则排除连接池
		socket = getPoolSocket(rq);
		if (socket) {
			half = false;
			return socket;
		}
	}
	half = true;
	return newConnection(rq, need_name_resolved);
}
KUpstreamSelectable *KSockPoolHelper::newConnection(KHttpRequest *rq, bool &need_name_resolved)
{
	
	KClientSocket *sockfd;
	int flag = 0;
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl.size()>0 && ssl_ctx) {
		sockfd = new KSSLSocket(ssl_ctx);
		flag = STF_SSL;
	} else
#endif
		sockfd = new KClientSocket;
	KUpstreamSelectable *socket = new KUpstreamSelectable(sockfd);
	socket->set_flag(flag);
	bind(socket);
#ifdef KSOCKET_UNIX
	if (isUnix) {
		if(!socket->socket->halfconnect(host.c_str())){
			socket->destroy();
			return NULL;
		}
		return socket;
	}
#endif
	if (!try_numerichost_connect(rq,socket, need_name_resolved) && !need_name_resolved) {			
		socket->destroy();
		return NULL;
	}
	return socket;
}
void KSockPoolHelper::connect(KHttpRequest *rq)
{
	assert(rq->fetchObj);
	rq->ctx->upstream_sign = this->sign;
	bool half;
	bool need_name_resolved=false;
	KUpstreamSelectable *socket = getConnection(rq,half,need_name_resolved);
	if (!need_name_resolved || socket==NULL) {
		KAsyncFetchObject *fo = static_cast<KAsyncFetchObject *>(rq->fetchObj);
		fo->connectCallBack(rq,socket,half);
		return;
	}
	//异步解析
	KSockPoolDns *spdns = new KSockPoolDns;
	addRef();
	spdns->socket = socket;
	spdns->rq = rq;
	spdns->sh = this;
	find_addr(this->host.c_str(), kgl_addr_ip, sockpool_name_resovled_call_back, spdns, rq->c->selector);
}
bool KSockPoolHelper::connect_addr(KHttpRequest *rq, KUpstreamSelectable *socket, sockaddr_i &addr)
{
	katom_inc64((void *)&total_connect);
	sockaddr_i *bind_addr = NULL;
	const char *bind_ip = ip ? ip : rq->bind_ip;
	if (bind_ip) {
		bind_addr = new sockaddr_i;
		if (!KSocket::getaddr(bind_ip, 0, bind_addr, AF_UNSPEC, AI_NUMERICHOST)) {
			delete bind_addr;
			return false;
		}
	}
	bool result = socket->socket->halfconnect(addr, bind_addr, TEST(rq->filter_flags, RF_TPROXY_UPSTREAM)>0);
	if (bind_addr) {
		delete bind_addr;
	}
#ifdef ENABLE_UPSTREAM_SSL
	if (result && socket->isSSL()) {
		KSSLSocket *sslSocket = static_cast<KSSLSocket *>(socket->socket);
		if (!sslSocket->ssl_connect()) {
			klog(KLOG_ERR, "cann't bind_fd for ssl socket\n");
		}
	}
#endif
	return result;
}
bool KSockPoolHelper::try_numerichost_connect(KHttpRequest *rq,KUpstreamSelectable *socket, bool &need_name_resolved)
{
	sockaddr_i addr;
	bool is_numerichost = KSocket::getaddr(this->host.c_str(), this->port, &addr, AF_UNSPEC, AI_NUMERICHOST);
	need_name_resolved = !is_numerichost;
	if (!is_numerichost) {
		return false;
	}
	return connect_addr(rq, socket, addr);
}
bool KSockPoolHelper::setHostPort(std::string host,int port,const char *ssl)
{
	bool destChanged = false;
	lock.Lock();
	if(this->host != host || this->port!=port){
		destChanged = true;
	}
	this->host = host;
	this->port = port;
#ifdef ENABLE_UPSTREAM_SSL
	char *ssl_buf = NULL;
	char *protocols = NULL;
	char *chiper = NULL;
	if (ssl) {
		this->ssl = ssl;
		ssl_buf = strdup(ssl);
		protocols = strchr(ssl_buf, '/');
		if (protocols) {
			*protocols = '\0';
			protocols++;
			chiper = strchr(protocols, '/');
			if (chiper) {
				*chiper = '\0';
				chiper++;
			}
		}
	} else {
		this->ssl.clear();
	}
	

	if (ssl_ctx) {
		KSSLSocket::clean_ctx(ssl_ctx);
		ssl_ctx = NULL;
	}
	if (ssl) {
		ssl_ctx = KSSLSocket::init_client(NULL,NULL);
		if (ssl_ctx) {
			if (protocols) {
				KSSLSocket::set_ssl_protocols(ssl_ctx, protocols);
			}
			if (chiper) {
				SSL_CTX_set_cipher_list(ssl_ctx, chiper);
			}
			if (chiper == NULL || protocols == NULL) {
				//check global
				conf.admin_lock.Lock();
				std::string ssl_client_chiper = cconf ? cconf->ssl_client_chiper : conf.ssl_client_chiper;
				std::string ssl_client_protocols = cconf ? cconf->ssl_client_protocols : conf.ssl_client_protocols;
				conf.admin_lock.Unlock();
				if (chiper == NULL && !ssl_client_chiper.empty()) {
					SSL_CTX_set_cipher_list(ssl_ctx, ssl_client_chiper.c_str());
				}
				if (protocols == NULL && !ssl_client_protocols.empty()) {
					KSSLSocket::set_ssl_protocols(ssl_ctx, ssl_client_protocols.c_str());
				}
			}
			
		}		
	}
#endif
	if (destChanged) {
		//clean();
	}
#ifdef KSOCKET_UNIX
	isUnix = false;
	if(strncasecmp(this->host.c_str(),"unix:",5)==0){
		isUnix = true;
		this->host = this->host.substr(5);
	}
	if (this->host[0]=='/') {
		isUnix = true;
	}
#endif
	lock.Unlock();
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl_buf) {
		free(ssl_buf);
	}
#endif
	return true;
}
bool KSockPoolHelper::setHostPort(std::string host, const char *port) {
	return setHostPort(host,atoi(port),strchr(port,'s'));
}
void KSockPoolHelper::disable() {
	if (error_try_time==0) {
		tryTime = kgl_current_sec + ERROR_RECONNECT_TIME;
	} else {
		tryTime = kgl_current_sec + error_try_time;
	}
}
bool KSockPoolHelper::isEnable() {
	if (tryTime == 0) {
		return true;
	}
	if (tryTime < kgl_current_sec) {
		tryTime += MAX(error_try_time,10);
		checkActive();
		return false;
	}
	return false;
}
void KSockPoolHelper::enable() {
	tryTime = 0;
	error_count = 0;
}
bool KSockPoolHelper::parse(std::map<std::string,std::string> &attr)
{
	setHostPort(attr["host"],attr["port"].c_str());
	setLifeTime(atoi(attr["life_time"].c_str()));

	setIp(attr["self_ip"].c_str());
	sign = (attr["sign"] == "1");
	return true;
}
void KSockPoolHelper::buildXML(std::stringstream &s)
{
	s << " host='" ;
	s << host << "' port='" << port ;
#ifdef ENABLE_UPSTREAM_SSL
	if (ssl.size()>0) {
		s << ssl;
	}
#endif
	s << "' life_time='" << getLifeTime() << "' ";

	if (ip && *ip) {
		s << "self_ip='" << ip << "' ";
	}
	if (sign) {
		s << "sign='1' ";
	}
}


