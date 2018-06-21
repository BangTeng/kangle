/*
 * KServer.cpp
 *
 *  Created on: 2010-5-5
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

#include <stdlib.h>
#include <vector>
#include "KServer.h"
#include "log.h"
#include "malloc_debug.h"
#include "do_config.h"
#include "KSelectorManager.h"
#include "KThreadPool.h"
#include "forwin32.h"
#include "extern.h"
#include "KHttpRequest.h"
#include "KNsVirtualHost.h"
#include "malloc_debug.h"

int KServer::failedTries = 0;
void next_server_request(void *arg,int got)
{
	KConnectionSelectable *c = (KConnectionSelectable *)arg;
	selectorManager.startRequest(c);
}
void handle_server_listen(void *arg, int got)
{
	KServerSelectable *ss = (KServerSelectable *)arg;
#ifdef _WIN32
	ss->handleAcceptEx(got);
	return;
#endif
	if (ss->server->isClosed()) {
		ss->remove_socket_event();
		ss->server->started = false;
		ss->server->release();
		return;
	}
	ss->listen_event();
#ifdef SOLARIS
	//solaris port need every time call addSocket
	ss->selector->listen(ss, handleServerListen);
#endif
}

KServerSelectable::~KServerSelectable()
{
	
	if (server_socket) {
		delete server_socket;
	}
}
KServerSelectable::KServerSelectable(KServer *server, KServerSocket *server_socket)
{
	memset(static_cast<KSelectable *>(this), 0, sizeof(KSelectable));
	this->server = server;
	this->server_socket = server_socket;
	this->sock = server_socket;
	next = NULL;
	
}
KServer::KServer() {
	server_selectable = NULL;
#ifdef KSOCKET_SSL
	ssl_ctx = NULL;
#endif
	closed = false;
	started = false;
	vhc = NULL;
	dynamic = false;
	static_flag = false;
}
void KServerSelectable::remove_socket_event()
{
	selector->removeSocket(this);
}
void KServerSelectable::listen_event()
{
#ifdef _WIN32
	bool noblocking = false;
#else
	bool noblocking = true;
#endif
	bool ssl_model = false;
	KClientSocket *socket;
#ifdef KSOCKET_SSL
	if (TEST(server->model, WORK_MODEL_SSL)) {
		socket = new KSSLSocket(server->ssl_ctx);
		ssl_model = true;
#ifdef _WIN32
		noblocking = true;
#endif
	} else {
#endif
		socket = new KClientSocket;
#ifdef KSOCKET_SSL
	}
#endif
	if (!server_socket->accept(socket, noblocking)) {
		klog(KLOG_ERR, "cann't accept connect,errno=%s\n", strerror(errno));
		delete socket;
		return;
	}
#ifndef NDEBUG
	//klog(KLOG_DEBUG,"new client %s:%d connect to %s:%d sockfd=%d\n", socket->get_remote_ip().c_str(), socket->get_remote_port(),socket->get_self_ip().c_str(),socket->get_self_port(),socket->get_socket());
#endif
	KConnectionSelectable *c = new KConnectionSelectable(socket);
	c->ls = server;
	server->addRef();
	if (ssl_model) {
		c->set_flag(STF_SSL);
	}
	if (server->is_multi_selectale()) {
		selector->bindSelectable(c);
	} else {
		selectorManager.getSelector()->bindSelectable(c);
	}
	if (selector==c->selector) {
		selectorManager.startRequest(c);
		return;
	}
	if (!c->selector->next(next_server_request,c)) {
		perror("next call\n");
	}
}
KServer::~KServer() {
	this->close();
	while (server_selectable) {
		KServerSelectable *next = server_selectable->next;
		delete server_selectable;
		server_selectable = next;
	}
#ifdef KSOCKET_SSL
	if (ssl_ctx) {
		KSSLSocket::clean_ctx(ssl_ctx);
	}
#endif
	if (vhc) {
		delete vhc;
	}
}
void KServer::close() {
	closed = true;
	KServerSelectable *s = server_selectable;
	while (s) {
		if (s->server_socket) {
			s->server_socket->shutdown(SHUT_RDWR);
			s->server_socket->close();
		}
		s = s->next;
	}
#ifdef KSOCKET_UNIX
	if (server_selectable && *ip == '/') {
		unlink(ip);
	}
#endif
}
#ifdef KSOCKET_SSL
bool KServer::load_ssl()
{
	if (is_ssl_loaded()) {
		return true;
	}
	std::string certFile = certificate;
	if(!certFile.empty() && !isAbsolutePath(certFile.c_str())){
		certFile = conf.path + certificate;
	}
	std::string privateKeyFile = certificate_key;
	if(!privateKeyFile.empty() && !isAbsolutePath(privateKeyFile.c_str())){
		privateKeyFile = conf.path + certificate_key;
	}
	if (certFile.empty()) {
		return false;
	}
	ssl_ctx = KSSLSocket::init_server((certFile.size() > 0 ? certFile.c_str() : NULL),
					privateKeyFile.c_str(),
					NULL);
	if (ssl_ctx == NULL) {
		klog(KLOG_ERR,"Cann't init ssl context certificate=[%s],certificate_key=[%s]\n",
			certificate.c_str(), certificate_key.c_str());
		return false;
	}
	if(0 == SSL_CTX_set_tlsext_servername_callback(ssl_ctx,httpSSLServerName)){
		klog(KLOG_WARNING, "kangle was built with SNI support, however, now it is linked "
			"dynamically to an OpenSSL library which has no tlsext support, "
			"therefore SNI is not available");
	}
#ifdef ENABLE_HTTP2
	SSL_CTX_set_ex_data(ssl_ctx, kangle_ssl_ctx_index, &http2);
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
	SSL_CTX_set_alpn_select_cb(ssl_ctx, httpSSLNpnSelected, NULL);
#endif
#if TLSEXT_TYPE_next_proto_neg
	SSL_CTX_set_next_protos_advertised_cb(ssl_ctx,httpSSLNpnAdvertised,NULL);
#endif
#endif
	//const char *default_cipher = "HIGH:!aNULL:!MD5";
	if (!cipher.empty()) {
		if (SSL_CTX_set_cipher_list(ssl_ctx, cipher.c_str()) != 1) {
			klog(KLOG_WARNING, "cipher [%s] is not support\n", cipher.c_str());
		}
	}
	if (!protocols.empty()) {
		KSSLSocket::set_ssl_protocols(ssl_ctx, protocols.c_str());
	}
	return true;
}
#endif
void KServer::bindVirtualHost(KVirtualHost *vh,bool high)
{
	KVirtualHostContainer **vhc;
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		vhc = &this->vhc;
		if (*vhc==NULL) {
			*vhc = new KVirtualHostContainer;
		}
		(*vhc)->bindVirtualHost((*it2),high?kgl_bind_high:kgl_bind_low);
	}
}
void KServer::remove_static(KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	if (vh->binds.empty()) {
		removeVirtualHost(vh);
		return;
	}
#endif
}
void KServer::add_static(KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	if (vh->binds.empty()) {
		bindVirtualHost(vh,false);
		return;
	}
#endif
}
void KServer::removeVirtualHost(KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	KVirtualHostContainer **vhc;
	std::list<KSubVirtualHost *>::iterator it2;	
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		vhc = &this->vhc;
		if (*vhc==NULL) {
			continue;
		}
		if ((*vhc)->unbindVirtualHost((*it2))==kgl_del_empty) {
			delete *vhc;
			*vhc = NULL;
		}
	}
#endif	
}
void KServer::clear()
{
	if (vhc) {
		delete vhc;
		vhc = NULL;
	}
}
bool KServer::isEmpty()
{
	if (vhc && !vhc->isEmpty()) {
		return false;
	}
	return true;
}
void KServer::unbindAllVirtualHost()
{
	if (vhc) {
		delete vhc;
		vhc = NULL;
	}
}
#if 0
void KServer::addDefaultVirtualHost(KVirtualHost *vh)
{
#ifdef ENABLE_BASED_IP_VH
	ipVhc.addVirtualHost(vh);
#endif
#ifndef HTTP_PROXY
	std::list<u_short>::iterator it;
	std::list<KSubVirtualHost *>::iterator it2;
	if (vh->ports.size()==0 && vh->binds.size()==0) {
		for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
			if(!defaultVhc.bindVirtualHost((*it2)) ) {
				(*it2)->allSuccess = false;
			}
		}
		return;
	}
	for(it=vh->ports.begin();it!=vh->ports.end();it++){
		if(0 == (*it)){
			for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
				if(!defaultVhc.bindVirtualHost((*it2)) ) {
					(*it2)->allSuccess = false;
				}
			}
			return;
		}
	}
	return;
#endif
}
query_vh_result KServer::findVirtualHost(KSubVirtualHost **rq_svh,domain_t hostname)
{
	return defaultVhc.findVirtualHost(rq_svh,hostname);
}
void KServer::removeDefaultVirtualHost(KVirtualHost *vh)
{
#ifdef ENABLE_BASED_IP_VH
	ipVhc.removeVirtualHost(vh);
#endif
#ifndef HTTP_PROXY
	std::list<KSubVirtualHost *>::iterator it2;	
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		defaultVhc.unbindVirtualHost((*it2));
	}
	return;
#endif
}
#endif
bool KServer::internal_open(int flag)
{
#ifdef KSOCKET_UNIX
	if (ip && *ip == '/') {
		KUnixServerSocket *unix_server = new KUnixServerSocket;
		SET(model, WORK_MODEL_UNIX_SOCKET);
		if (unix_server->open(ip)) {
			add_server_socket(unix_server);
			return true;
		}
		delete unix_server;
		return false;
	}
#endif

	CLR(model, WORK_MODEL_UNIX_SOCKET);
	SET(flag, KSOCKET_REUSEPORT);
	bool all_result = true;
	for (int i = 0; i < selectorManager.getSelectorCount(); i++) {
		KServerSocket *server = new KServerSocket;
		if (server->open(port, ip, flag)) {
			add_server_socket(server);
#ifdef _WIN32
			break;
#endif
			continue;
		}
		all_result = false;
		delete server;
		break;
	}
	if (!all_result) {
		while (server_selectable && server_selectable->next) {
			KServerSelectable *next = server_selectable;
			delete server_selectable;
			server_selectable = next;
		}
	}
	return server_selectable != NULL;
}
bool KServer::start()
{
	if (!open()) {
		while (server_selectable) {
			KServerSelectable *next = server_selectable->next;
			delete server_selectable;
			server_selectable = next;
		}
#ifdef KSOCKET_SSL
		if (ssl_ctx) {
			KSSLSocket::clean_ctx(ssl_ctx);
			ssl_ctx = NULL;
		}
#endif
		return false;
	}
	started = selectorManager.listen(this, handle_server_listen);
	return started;
}
bool KServer::open()
{
	while (server_selectable) {
		KServerSelectable *next = server_selectable->next;
		delete server_selectable;
		server_selectable = next;
	}
	closed = false;
	bool result = false;
	int flag = (ipv4 ? KSOCKET_ONLY_IPV4 : KSOCKET_ONLY_IPV6);
#ifdef ENABLE_TPROXY
	if (TEST(model, WORK_MODEL_TPROXY)) {
		flag |= KSOCKET_TPROXY;
	}
#endif
#ifdef KSOCKET_SSL
	if (TEST(model, WORK_MODEL_SSL)) {
		if (!load_ssl()) {
			return false;
		}
	}
#endif
	for (;;) {
		result = internal_open(flag);
		if (result) {
			break;
		}
		close();
		if (failedTries>10) {
			break;
		}
		failedTries++;
		my_msleep(500);
	}
	if (!result) {
		int err = errno;
		klog(KLOG_ERR, "cann't listen [%s:%d],error=[%d %s]\n", ip, port, err,strerror(err));
		return false;
	}
	klog(KLOG_NOTICE, "listen [%s:%d] success\n", ip, port);
	return true;
}
void KServer::add_server_socket(KServerSocket *socket)
{
	KServerSelectable *ss = new KServerSelectable(this, socket);
	ss->bind_socket(socket);
	ss->next = server_selectable;
	server_selectable = ss;
}

