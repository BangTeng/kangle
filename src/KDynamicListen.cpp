#include "KDynamicListen.h"
#include "KVirtualHost.h"
#include "ksocket.h"
#include "KServerListen.h"
#include "KVirtualHostManage.h"
#include "kselector_manager.h"
#include "KPreRequest.h"
#include "lib.h"
#define DEFAULT_IPV4_IP  "0.0.0.0"
#define DEFAULT_IPV6_IP  "::"
KDynamicListen dlisten;
static void kserver_clean_ctx(void *ctx)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)ctx;
	delete vhc;
}
void kserver_remove_vh(kserver *server, KVirtualHost *vh)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)server->ctx;
#ifndef HTTP_PROXY
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		vhc->unbindVirtualHost((*it2));
	}
#endif
}
bool kserver_start(kserver *server,const KListenKey &lk)
{
#ifdef KSOCKET_SSL
	if (server->ssl && server->ssl_ctx == NULL) {
		//如果是ssl端口，但没有ssl_ctx，则不侦听
		return false;
	}
#endif
	kserver_open(server, lk.ip.c_str(), lk.port, lk.ipv4);
	return kserver_accept(server);
}
void kserver_bind_vh(kserver *server, KVirtualHost *vh,bool high)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)server->ctx;
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		vhc->bindVirtualHost((*it2), high ? kgl_bind_high : kgl_bind_low);
	}
}
void kserver_add_static_vh(kserver *server, KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	if (vh->binds.empty()) {
		kserver_bind_vh(server,vh, false);
	}
#endif
}
void kserver_remove_static_vh(kserver *server, KVirtualHost *vh)
{
#ifndef HTTP_PROXY
	if (vh->binds.empty()) {
		kserver_remove_vh(server,vh);
		return;
	}
#endif
}
#ifdef KSOCKET_SSL
void kserver_load_ssl(kserver *server, const char *cert_file, const char *key_file, const char *cipher, const char *protocol)
{
	server->ssl = 1;
	SET(server->flags, WORK_MODEL_SSL);
	std::string certificate_file;
	std::string private_file;
	if (cert_file && !isAbsolutePath(cert_file)) {
		certificate_file = conf.path + cert_file;
		cert_file = certificate_file.c_str();
	}
	if (key_file && !isAbsolutePath(key_file)) {
		private_file = conf.path + key_file;
		key_file = private_file.c_str();
	}
	SSL_CTX *ssl_ctx = kgl_ssl_ctx_new_server(cert_file, key_file, NULL, NULL, &server->http2);
	if (ssl_ctx) {
		if (cipher && *cipher) {
			if (!kgl_ssl_ctx_set_cipher_list(ssl_ctx, cipher)) {
				klog(KLOG_ERR, "set chiper [%s] failed.\n", cipher);
			}
		}		
		kgl_ssl_ctx_set_protocols(ssl_ctx, protocol);
		kassert(server->ssl_ctx == NULL);
		server->ssl_ctx = ssl_ctx;
	}
}
#endif
static bool kserver_is_empty(kserver *server)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)server->ctx;
	return vhc->isEmpty();
}
static kserver *kserver_new()
{
	KVirtualHostContainer *vhc = new KVirtualHostContainer();
	kserver *server = kserver_init();
	kserver_bind(server, handle_connection, kserver_clean_ctx, vhc);
	return server;
}
void KDynamicListen::add_dynamic(const char *listen,KVirtualHost *vh)
{
	std::list<KListenKey> lk;
	parseListen(listen,lk);
	if (lk.size()<=0) {
		return;
	}
	std::map<KListenKey,kserver *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		kserver *server = NULL;
		it = listens.find((*it2));
		if (it==listens.end()) {
			server = kserver_new();
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
			if ((*it2).ssl) {
#ifdef ENABLE_HTTP2
				server->http2 = vh->http2;
#endif	
				kserver_load_ssl(server,
					vh->getCertfile().c_str(),
					vh->getKeyfile().c_str(),
					vh->cipher,
					vh->protocols);
			}
#endif
			(*it2).set_work_model(server);
			server->dynamic = 1;
			listens.insert(std::pair<KListenKey,kserver *>((*it2),server));
			initListen((*it2),server);
		} else {
			server = (*it).second;
#ifdef ENABLE_HTTP2
			server->http2 = vh->http2;
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
			if (server->ssl_ctx==NULL && server->ss==NULL && (*it2).ssl && server->dynamic) {
				//update ssl certificate and try it again.
				kserver_load_ssl(server,
					vh->getCertfile().c_str(),
					vh->getKeyfile().c_str(),
					vh->cipher,
					vh->protocols);
				initListen((*it2),server);
			}
#endif
			(*it2).set_work_model(server);
			server->dynamic = true;
		}
		kserver_bind_vh(server,vh,true);
	}
}
void KDynamicListen::remove(const char *listen,KVirtualHost *vh)
{
	std::list<KListenKey> lk;
	parseListen(listen,lk);
	if (lk.size()<=0) {
		return;
	}	
	std::map<KListenKey,kserver *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		it = listens.find((*it2));
		if (it!=listens.end()) {
			kserver_remove_vh((*it).second,vh);
		}
	}	
}
void KDynamicListen::flush()
{
	std::map<KListenKey,kserver *>::iterator it,it_next;
	for (it=listens.begin();it!=listens.end();) {
		kserver *server = (*it).second;
		it_next = it;
		it_next++;
		if (server->remove_static_flag) {
			server->remove_static_flag = false;
			if (server->static_flag) {
				server->static_flag = false;
				conf.gvm->remove_static(server);
			}
		}
		if ((server->dynamic && kserver_is_empty(server)) || (!server->dynamic && !server->static_flag)) {
			kserver_close(server);
			listens.erase(it);
			kserver_release(server);
		}
		it = it_next;
	}
}
void KDynamicListen::flush(const char *listen)
{
	std::list<KListenKey> lk;
	parseListen(listen,lk);
	if (lk.size()<=0) {
		return;
	}
	std::map<KListenKey,kserver *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		std::map<KListenKey,kserver *>::iterator it = listens.find((*it2));
		if (it!=listens.end()) {
			kserver *server = (*it).second;
			if (server->dynamic && !server->static_flag && kserver_is_empty(server)) {
				kserver_close(server);
				listens.erase(it);
				kserver_release(server);				
			}
		}
	}
}

bool KDynamicListen::add_static(KListenHost *listen)
{
	std::list<KListenKey> lk;
	parseListen(listen,lk);
	if (lk.size()<=0) {
		return false;
	}	
	std::map<KListenKey,kserver *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		kserver *server = NULL;
		it = listens.find((*it2));
		if (it == listens.end()) {
			server = kserver_new();
			server->flags = listen->model;
			server->static_flag = true;
			server->remove_static_flag = false;
#ifdef KSOCKET_SSL
			if ((*it2).ssl) {
#ifdef ENABLE_HTTP2
				server->http2 = listen->http2;
#endif
				kserver_load_ssl(server,
					listen->certificate.c_str(),
					listen->certificate_key.c_str(),
					listen->cipher.c_str(),
					listen->protocols.c_str());
			}
#endif
			(*it2).set_work_model(server);
			listens.insert(std::pair<KListenKey, kserver *>((*it2), server));
			initListen((*it2), server);
			conf.gvm->add_static(server);
		} else {
			server = (*it).second;
			if (!server->static_flag) {
				conf.gvm->add_static(server);
			}
			server->static_flag = true;
			server->remove_static_flag = false;
#ifdef ENABLE_HTTP2
			server->http2 = listen->http2;
#endif
#ifdef KSOCKET_SSL
			if (server->ssl_ctx==NULL && server->ss == NULL && (*it2).ssl) {
				//update ssl certificate and try it again.
				kserver_load_ssl(server,
					listen->certificate.c_str(),
					listen->certificate_key.c_str(),
					listen->cipher.c_str(),
					listen->protocols.c_str());
				initListen((*it2), server);
			}
#endif
			(*it2).set_work_model(server);
		}
	}
	return true;
}
kserver *KDynamicListen::refsServer(u_short port)
{
	std::map<KListenKey,kserver *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		if ((*it).first.port==port) {
			kserver *server = (*it).second;
			kserver_refs(server);
			return server;
		}
	}
	return NULL;
}
void KDynamicListen::parseListen(KListenHost *lh,std::list<KListenKey> &lk)
{
	if (strcmp(lh->ip.c_str(),"*")==0) {
		getListenKey(lh,true,lk);
#ifdef KSOCKET_IPV6
		getListenKey(lh,false,lk);
#endif
	} else {		
		bool ipv4 = true;
#ifdef KSOCKET_IPV6
		sockaddr_i addr;
		if (ksocket_getaddr(lh->ip.c_str(), 0, AF_UNSPEC, AI_NUMERICHOST,&addr)) {
			if(PF_INET == addr.v4.sin_family){
				ipv4 = true;
			} else {
				ipv4 = false;
			}
		}

#endif
		getListenKey(lh,ipv4,lk);
	}
	return;
}
void KDynamicListen::parseListen(const char *listen,std::list<KListenKey> &lks)
{
	char *buf = strdup(listen);
	char *p = strrchr(buf,':');
	if (p) {
		*p = '\0';
		p++;
		KListenKey lk;
		parse_port(p, &lk);
		if (strcmp(buf,"*")==0) {
			lk.ip = DEFAULT_IPV4_IP;
			lk.ipv4 = true;
			lks.push_back(lk);
#ifdef KSOCKET_IPV6
			lk.ip = DEFAULT_IPV6_IP;
			lk.ipv4 = false;
			lks.push_back(lk);
#endif
		} else {		
			lk.ip = buf;
#ifdef KSOCKET_IPV6	
			sockaddr_i addr;
			if (ksocket_getaddr(buf, 0, AF_UNSPEC, AI_NUMERICHOST, &addr)) {
				if(PF_INET == addr.v4.sin_family){
					lk.ipv4 = true;
				} else {
					lk.ipv4 = false;
				}
			}
#else
			lk.ipv4 = true;
#endif
			lks.push_back(lk);
		}
	}
	free(buf);
}
bool KDynamicListen::initListen(const KListenKey &lk,kserver *server)
{
	if (is_selector_manager_init()) {
		return kserver_start(server,lk);
	}
	return true;
}
void KDynamicListen::getListenKey(KListenHost *lh,bool ipv4,std::list<KListenKey> &lk)
{
	char *buf = strdup(lh->port.c_str());
	char *hot = buf;
	while (*hot) {
		char *p = strchr(hot,',');
		if (p) {
			*p++ = '\0';
		}
		if (IS_DIGIT(*hot)) {
			getListenKey(lh, hot, ipv4, lk);
		}
		if (p==NULL) {
			break;
		}
		hot = p;
	}
	free(buf);
}
void KDynamicListen::getListenKey(KListenHost *lh,const char *port,bool ipv4,std::list<KListenKey> &lk)
{
	KListenKey key;
	key.ssl = TEST(lh->model,WORK_MODEL_SSL)>0;
	key.ipv4 = ipv4;
	parse_port(port, &key);
	if (lh->ip=="*") {
		if (ipv4) {
			key.ip = DEFAULT_IPV4_IP;
		} else {
			key.ip = DEFAULT_IPV6_IP;
		}
	} else {
		key.ip = lh->ip;
	}
	lk.push_back(key);
}
void KDynamicListen::delayStart()
{
	kassert(is_selector_manager_init());
	std::map<KListenKey,kserver *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		kserver *server = (*it).second;
		if (!server->started) {
			kserver_start(server,(*it).first);
		}
	}
}
void KDynamicListen::addStaticVirtualHost(KVirtualHost *vh)
{
	std::map<KListenKey,kserver *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		kserver *server = (*it).second;
		if (server->static_flag) {
			kserver_add_static_vh(server, vh);
		}
	}
}
void KDynamicListen::parse_port(const char *p, KListenKey *lk)
{
	lk->port = atoi(p);
	if (strchr(p, 's')) {
		lk->ssl = true;
	}
	
#ifdef WORK_MODEL_PROXY
	if (strchr(p, 'P')) {
		lk->proxy = true;
	}
#endif
}
void KDynamicListen::removeStaticVirtualHost(KVirtualHost *vh)
{
	std::map<KListenKey,kserver *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		kserver *server = (*it).second;
		if (server->static_flag) {
			kserver_remove_vh(server, vh);
		}
	}
}
void KDynamicListen::clear()
{
	std::map<KListenKey,kserver *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		kserver *server = (*it).second;
		kserver_close(server);
		kserver_release(server);
	}
	listens.clear();
}
void KDynamicListen::close()
{
	std::map<KListenKey,kserver *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		kserver *server = (*it).second;
		kserver_close(server);
	}
}
void KDynamicListen::get_listen_whm(WhmContext *ctx)
{
	std::map<KListenKey, kserver *>::iterator it;
	for (it = listens.begin(); it != listens.end(); it++) {
		kserver *server = (*it).second;
		if (server->started) {
			std::stringstream s;
			bool unix_socket = false;
#ifdef KSOCKET_UNIX
			if (server->addr.v4.sin_family==AF_UNIX) {
				unix_socket = true;
				s << "<ip>" << ksocket_unix_path(&server->un_addr) << "</ip>";
				s << "<port>-</port>";
			} else {
#endif
				char ip[MAXIPLEN];
				ksocket_sockaddr_ip(&server->addr, ip, sizeof(ip));
				s << "<ip>" << ip << "</ip>";
				s << "<port>" << ksocket_addr_port(&server->addr) << "</port>";
#ifdef KSOCKET_UNIX
			}
#endif
#ifdef WORK_MODEL_PROXY
			if (TEST(server->flags, WORK_MODEL_PROXY)) {
				s << "<proxy>1</proxy>";
			}
#endif
#ifdef WORK_MODEL_TPROXY
			if (TEST(server->flags, WORK_MODEL_TPROXY)) {
				s << "<tproxy>1</tproxy>";
			}
#endif
			s << "<service>" << getWorkModelName(server->flags) << "</service>";
			s << "<tcp_ip>";
			if (unix_socket) {
				s << "unix";
			} else if (server->ss) {
				s << (server->addr.v4.sin_family == PF_INET ? "4" : "6");
			}
			s << "</tcp_ip>";
			s << "<static>" << (server->static_flag?1:0) << "</static>";
			s << "<dynamic>" << (server->dynamic?1:0) << "</dynamic>";
			
			s << "<multi_bind>" << (is_server_multi_selectable(server) ? 1 : 0) << "</multi_bind>";
			s << "<refs>" << katom_get((void *)&server->refs) << "</refs>";
			ctx->add("listen", s.str().c_str(), false);
		}
	}
}
void KDynamicListen::getListenHtml(std::stringstream &s)
{
	std::map<KListenKey,kserver *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		kserver *server = (*it).second;
		if (server->started) {
			s << "<tr>";
			bool unix_socket = false;
#ifdef KSOCKET_UNIX
			if (server->addr.v4.sin_family==AF_UNIX) {
				unix_socket = true;
				s << "<td>" << ksocket_unix_path(&server->un_addr) << "</td>";
				s << "<td>-";
			} else {
#endif
				char ip[MAXIPLEN];
				ksocket_sockaddr_ip(&server->addr,  ip, sizeof(ip));
				s << "<td>" << ip << "</td>";
				s << "<td>" << ksocket_addr_port(&server->addr);
#ifdef KSOCKET_UNIX
			}
#endif
#ifdef WORK_MODEL_PROXY
			if (TEST(server->flags, WORK_MODEL_PROXY)) {
				s << "P";
			}
#endif
#ifdef WORK_MODEL_TPROXY
			if (TEST(server->flags, WORK_MODEL_TPROXY)) {
				s << "p";
			}
#endif
			s << "</td>";
			s << "<td>" << getWorkModelName(server->flags) << "</td>";
			s << "<td>" ;
			if (unix_socket) {
				s << "unix";
			} else if (server->ss) {
				s << "tcp/ipv" << (server->addr.v4.sin_family==PF_INET?"4":"6");
			}
			s << "</td>";
			s << "<td>";
			if (server->static_flag) {
				s << "S";
			}
			if (server->dynamic) {
				s << "D";
			}
			if (is_server_multi_selectable(server)) {
				s << "M";
			}
			s << "</td><td>" << katom_get((void *)&server->refs);
			s << "</td>";
			s << "</tr>";
		}
	}
}

