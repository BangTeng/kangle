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

//#define ENABLE_SSL_CNAME_BINDING 1
KServer::KServer() {
	memset(static_cast<KSelectable *>(this), 0, sizeof(KSelectable));
#ifdef KSOCKET_SSL
	ssl_ctx = NULL;
	sslParsed = false;
#endif
	closed = false;
	started = false;

	vhc = NULL;
#ifdef ENABLE_CNAME_BIND
	cname_vhc = NULL;
#endif
	dynamic = false;
	static_flag = false;
	server = NULL;	
}

KServer::~KServer() {
	this->close();
	if (server) {
		delete server;
		server = NULL;
	}
#ifdef KSOCKET_SSL
	if (ssl_ctx) {
		KSSLSocket::clean_ctx(ssl_ctx);
	}
#endif

	if (vhc) {
		delete vhc;
	}
#ifdef ENABLE_CNAME_BIND
	if (cname_vhc) {
		delete cname_vhc;
	}
#endif
}
void KServer::removeSocket()
{
	selector->removeSocket(this);
}
void KServer::close() {
	closed = true;
	if (server) {
		server->shutdown(SHUT_RDWR);
		server->close();
#ifdef KSOCKET_UNIX
		if (*ip=='/') {
			unlink(ip);
		}
#endif
	}
}
#ifdef KSOCKET_SSL
bool KServer::load_ssl()
{
	if (!sslParsed) {
		//多进程模型ssl证书查找
		std::vector<KListenHost *>::iterator it;
		int port = server->get_self_port();
		for (it=conf.service.begin();it!=conf.service.end();it++) {
			if( model==(*it)->model	&& port==atoi((*it)->port.c_str())) {
				certificate = (*it)->certificate;
				certificate_key = (*it)->certificate_key;
				cipher = (*it)->cipher;
				protocols = (*it)->protocols;
				break;
			}
		
		}
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
	ssl_ctx = KSSLSocket::init_server(
					(certFile.size() > 0 ? certFile.c_str() : NULL),
					privateKeyFile.c_str(),
					NULL);
	if (ssl_ctx == NULL) {
		klog(KLOG_ERR,
				"Cann't init ssl context certificate=[%s],certificate_key=[%s]\n",
				certificate.c_str(), certificate_key.c_str());
		return false;
	}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
#ifndef ENABLE_SSL_CNAME_BINDING
	if(sni && 0 == SSL_CTX_set_tlsext_servername_callback(ssl_ctx,httpSSLServerName)){
			klog(KLOG_WARNING, "kangle was built with SNI support, however, now it is linked "
				"dynamically to an OpenSSL library which has no tlsext support, "
				"therefore SNI is not available");
	}
#endif

#endif
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
	if (protocols.size() > 0) {
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
#ifdef ENABLE_CNAME_BIND
		if ((*it2)->cname)
			vhc = &this->cname_vhc;
		else
#endif
			vhc = &this->vhc;

		if (*vhc==NULL) {
			*vhc = new KVirtualHostContainer;
		}
		(*vhc)->bindVirtualHost((*it2),high?kgl_bind_high:kgl_bind_low);
	}
}
void KServer::addVirtualHost(KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	if (server==NULL) {
		return;
	}
	if (vh->binds.empty()) {
		//优先级低绑定
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
#ifdef ENABLE_CNAME_BIND
		if ((*it2)->cname)
			vhc = &this->cname_vhc;
		else
#endif
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
#ifdef ENABLE_CNAME_BIND
	if (cname_vhc && !cname_vhc->isEmpty()) {
		return false;
	}
#endif
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
bool KServer::open(const char *ip,int port,int flag)
{
	if (server) {
		delete server;
		server = NULL;
		fd = NULL;
	}
	closed = false;
#ifdef KSOCKET_UNIX
	if (ip && *ip=='/') {
		KUnixServerSocket *unix_server = new KUnixServerSocket;
		server = unix_server;
		SET(model,WORK_MODEL_UNIX_SOCKET);
		return unix_server->open(ip);
	}
#endif
	CLR(model,WORK_MODEL_UNIX_SOCKET);
	server = new KServerSocket;
	fd = server;
	return server->open(port,ip,flag);
}

