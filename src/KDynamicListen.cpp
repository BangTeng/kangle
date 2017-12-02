#include "KDynamicListen.h"
#include "KVirtualHost.h"
#include "KSocket.h"
#include "KServerListen.h"
#include "KVirtualHostManage.h"
#include "KSelectorManager.h"
#include "lib.h"
#define DEFAULT_IPV4_IP  "0.0.0.0"
#define DEFAULT_IPV6_IP  "::"
KDynamicListen dlisten;
void KDynamicListen::add_dynamic(const char *listen,KVirtualHost *vh)
{
	std::list<KListenKey> lk;
	parseListen(listen,lk);
	if (lk.size()<=0) {
		return;
	}
	std::map<KListenKey,KServer *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		KServer *server = NULL;
		it = listens.find((*it2));
		if (it==listens.end()) {
			server = new KServer;
			server->model = 0;
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
			if ((*it2).ssl) {
				server->certificate = vh->getCertfile();
				server->certificate_key = vh->getKeyfile();
				server->cipher = (vh->cipher?vh->cipher:"");
				server->protocols = (vh->protocols ? vh->protocols : "");
				server->sslParsed = true;
				server->sni = true;
#ifdef ENABLE_HTTP2
				server->http2 = vh->http2;
#endif			
				SET(server->model,WORK_MODEL_SSL);
			}
#endif
			
			server->dynamic = true;
			listens.insert(std::pair<KListenKey,KServer *>((*it2),server));
			initListen((*it2),server);
		} else {
			server = (*it).second;
#ifdef ENABLE_HTTP2
			server->http2 = vh->http2;
#endif
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
			if (!server->isOpened() && (*it2).ssl && server->dynamic) {
				//update ssl certificate and try it again.
				server->certificate = vh->getCertfile();
				server->certificate_key = vh->getKeyfile();
				server->cipher = (vh->cipher?vh->cipher:"");
				server->protocols = (vh->protocols ? vh->protocols : "");
				server->sslParsed = true;
				server->sni = true;

				SET(server->model,WORK_MODEL_SSL);
				initListen((*it2),server);
			}
#endif
			
			server->dynamic = true;
		}
		server->bindVirtualHost(vh,true);
	}
}
void KDynamicListen::remove(const char *listen,KVirtualHost *vh)
{
	std::list<KListenKey> lk;
	parseListen(listen,lk);
	if (lk.size()<=0) {
		return;
	}	
	std::map<KListenKey,KServer *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		it = listens.find((*it2));
		if (it!=listens.end()) {
			(*it).second->removeVirtualHost(vh);
		}
	}	
}
void KDynamicListen::flush()
{
	std::map<KListenKey,KServer *>::iterator it,it_next;
	for (it=listens.begin();it!=listens.end();) {
		KServer *server = (*it).second;
		it_next = it;
		it_next++;
		if (server->server == NULL) {
			continue;
		}
		if ((server->dynamic && server->isEmpty()) || (!server->dynamic && !server->static_flag)) {
#ifdef _WIN32
			server->close();
#else
			server->setClosed();
			server->server->shutdown(SHUT_RDWR);
#endif
			listens.erase(it);
			server->release();
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
	std::map<KListenKey,KServer *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		std::map<KListenKey,KServer *>::iterator it = listens.find((*it2));
		if (it!=listens.end()) {
			KServer *server = (*it).second;
			if (server->dynamic && !server->static_flag && server->isEmpty() && server->server!=NULL) {
#ifdef _WIN32
				server->close();
#else
				server->setClosed();
				server->server->shutdown(SHUT_RDWR);
#endif
				listens.erase(it);
				server->release();
			}
		}
	}
}

bool KDynamicListen::add_static(KListenHost *listen,bool start)
{
	std::list<KListenKey> lk;
	parseListen(listen,lk);
	if (lk.size()<=0) {
		return false;
	}	
	std::map<KListenKey,KServer *>::iterator it;
	std::list<KListenKey>::iterator it2;
	for (it2=lk.begin();it2!=lk.end();it2++) {
		KServer *server = NULL;
		it = listens.find((*it2));
		if (it == listens.end()) {
			server = new KServer;
			server->model = listen->model;
			server->static_flag = true;
			//server->name = listen->name;
			//server->event_driven = listen->event_driven;
#ifdef KSOCKET_SSL
			if ((*it2).ssl) {
				server->certificate = listen->certificate;
				server->certificate_key = listen->certificate_key;
				server->cipher = listen->cipher;
				server->protocols = listen->protocols;
				server->sslParsed = true;
				server->sni = listen->sni;
#ifdef ENABLE_HTTP2
				server->http2 = listen->http2;
#endif
			}
#endif
			listens.insert(std::pair<KListenKey, KServer *>((*it2), server));
			if (initListen((*it2), server)) {
				if (start) {
					conf.gvm->bindVirtualHost(server);
				}
			}
		} else {
			server = (*it).second;
			server->static_flag = true;
#ifdef ENABLE_HTTP2
			server->http2 = listen->http2;
#endif
#ifdef KSOCKET_SSL
			server->sni = listen->sni;
			if (!server->isOpened() && (*it2).ssl) {
				//update ssl certificate and try it again.
				server->certificate = listen->certificate;
				server->certificate_key = listen->certificate_key;
				server->cipher = listen->cipher;
				server->protocols = listen->protocols;
				server->sslParsed = true;
				server->sni = listen->sni;

				SET(server->model, WORK_MODEL_SSL);
				initListen((*it2), server);
			}
#endif


		}
	}
	return true;
}
KServer *KDynamicListen::refsServer(u_short port)
{
	//KServer *server = NULL;
	std::map<KListenKey,KServer *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		if ((*it).first.port==port) {
			KServer *server = (*it).second;
			server->addRef();
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
		if (KSocket::getaddr(lh->ip.c_str(),0,&addr,0,AI_NUMERICHOST)) {
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
		lk.port = atoi(p);
		lk.ssl = false;
		if (strchr(p,'s')) {
			lk.ssl = true;
		}
		
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
			if (KSocket::getaddr(buf,0,&addr,0,AI_NUMERICHOST)) {
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
bool KDynamicListen::initListen(const KListenKey &lk,KServer *server)
{
	int flag = (lk.ipv4?KSOCKET_ONLY_IPV4:KSOCKET_ONLY_IPV6);
#ifdef ENABLE_TPROXY
	if (TEST(server->model,WORK_MODEL_TPROXY)) {
		flag |= KSOCKET_TPROXY;
	}
#endif
	SAFE_STRCPY(server->ip,lk.ip.c_str());
	server->port = lk.port;
	bool result = false;
	for (;;) {
		result = server->open((lk.ip.size()>0?lk.ip.c_str():NULL),lk.port,flag);
		if (result) {
			break;
		}
		server->close();
		if (failedTries>10) {
			break;
		}
		failedTries++;
		my_msleep(500);
	}
	if (!result) {
		klog(KLOG_ERR, "Cann't listen %s:%d,errno=%d\n",lk.ip.c_str(), lk.port,errno);
		return false;
	}
#ifdef KSOCKET_SSL
	if (TEST(server->model,WORK_MODEL_SSL)) {
		if (!server->load_ssl()) {
			server->close();
			return false;
		}
	}
#endif
	klog(KLOG_NOTICE, "listen %s:%d success\n",	lk.ip.c_str(), lk.port);
	if (selectorManager.isInit()) {
		KServerListen::start(server);
	}
	return result;
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
		int port = atoi(hot);
		if (port>=0) {
			getListenKey(lh,atoi(hot),ipv4,lk);
		}
		if (p==NULL) {
			break;
		}
		hot = p;
	}
	free(buf);
}
void KDynamicListen::getListenKey(KListenHost *lh,int port,bool ipv4,std::list<KListenKey> &lk)
{
	KListenKey key;
	key.port = port;
	key.ssl = TEST(lh->model,WORK_MODEL_SSL)>0;
	key.ipv4 = ipv4;
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
	assert(selectorManager.isInit());
	std::map<KListenKey,KServer *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		KServer *server = (*it).second;
		if (server->isOpened() && !server->started) {
			KServerListen::start(server);
		}
	}
}
void KDynamicListen::addStaticVirtualHost(KVirtualHost *vh)
{
	std::map<KListenKey,KServer *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		KServer *server = (*it).second;
		if (server->isOpened() && server->static_flag) {
			server->addVirtualHost(vh);
		}
	}
}
void KDynamicListen::removeStaticVirtualHost(KVirtualHost *vh)
{
	std::map<KListenKey,KServer *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		KServer *server = (*it).second;
		if (server->isOpened() && server->static_flag) {
			server->removeVirtualHost(vh);
		}
	}
}
void KDynamicListen::clear()
{
	std::map<KListenKey,KServer *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		KServer *server = (*it).second;
		server->close();
		server->release();
	}
	listens.clear();
}
void KDynamicListen::close()
{
	std::map<KListenKey,KServer *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		KServer *server = (*it).second;
		if (server->isOpened()) {
			server->close();
		}
	}
}
void KDynamicListen::getListenHtml(std::stringstream &s)
{
	std::map<KListenKey,KServer *>::iterator it;
	for (it=listens.begin();it!=listens.end();it++) {
		KServer *server = (*it).second;
		if (server->started && server->server!=NULL){
			bool unix_socket = (*server->ip=='/');
			s << "<tr>";			
			s << "<td>" << server->ip << "</td>";
			s << "<td>" << (unix_socket?0:server->server->get_self_port()) << "</td>";
			s << "<td>" << getWorkModelName(server->model) << "</td>";
			s << "<td>" ;
			if (unix_socket) {
				s << "unix";
			} else {
				s << "tcp/ipv" << server->server->getIpVer();
			}
			s << "</td>";
			s << "<td>" << (server->dynamic?"yes":"&nbsp;");
			if (server->isEmpty()) {
				s << " EMP";
			}
			//if (server->event_driven) {
			//	s << " EV";
			//}
			s << " " << server->getRef();
			s << "</td>";
			s << "</tr>";
		}
	}
}
