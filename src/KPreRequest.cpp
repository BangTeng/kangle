#include <map>
#include <string>
#include <sstream>
#include "KPreRequest.h"
#include "KHttpSink.h"

#include "KHttpRequest.h"
#include "KHttp2.h"
#include "KProxy.h"
#include "KConfig.h"
#include "KVirtualHostManage.h"
#define KGL_BUSY_MSG "HTTP/1.0 503 Service Unavailable\r\nConnection: close\r\n\r\nServer is busy."
unsigned total_connect = 0;
typedef std::map<ip_addr, unsigned> intmap;
intmap m_ip;
KMutex ipLock;
typedef enum {
	kgl_connection_success,
	kgl_connection_too_many,
	kgl_connection_per_limit,
	kgl_connection_unknow
} kgl_connection_result;

std::string get_connect_per_ip() {
	std::stringstream s;
	s << "<html><head><LINK href=/main.css type='text/css' rel=stylesheet></head><body>";
	s << LANG_MAX_CONNECT_PER_IP << conf.max_per_ip;
	s << "<script language='javascript'>\n\
	var ips=new Array();\n\
	function show_ips(){\n\
		document.write('<table border=1><tr><td>" << LANG_IP << "</td><td>" << LANG_CONNECT_COUNT << "</td></tr>');\n\
		for(var i=0;i<ips.length;i++){\n\
			document.write('<tr><td>'+ips[i][0]+'</td><td>'+ips[i][1]+'</td>');\n\
		}\
		document.write('</table>');\n\
	}\n\
	function compare_ips(a,b) {\n\
		return b[1] - a[1];\n\
	}\n";	
	ipLock.Lock();
	intmap::iterator it;
	for (it = m_ip.begin(); it != m_ip.end(); it++) {
		char ip[MAXIPLEN];
		ksocket_ipaddr_ip(&(*it).first, ip, sizeof(ip));
		s << "ips.push(new Array('" << ip << "'," << (*it).second << "));\n";
	}
	ipLock.Unlock();	
	s << "ips.sort(compare_ips);\n\
	show_ips();\n\
	</script>";
	//s << "<!-- total connect = " << total << " -->\n";
	s << endTag() << "</body></html>";
	return s.str();
}
void del_request(void *data)
{
	ipLock.Lock();
	total_connect--;
#ifdef RQ_LEAK_DEBUG
	klist_remove(&c->queue_edge);
#endif
	ipLock.Unlock();
}
void del_request_per_ip(void *data)
{
	kconnection *c = (kconnection *)data;
	ip_addr ip;
	ksocket_ipaddr(&c->addr, &ip);
	intmap::iterator it2;
	ipLock.Lock();
	total_connect--;
#ifdef RQ_LEAK_DEBUG
	klist_remove(&c->queue_edge);
#endif
	it2 = m_ip.find(ip);
	assert(it2 != m_ip.end());
	(*it2).second--;
	if ((*it2).second == 0) {
		m_ip.erase(it2);
	}
	ipLock.Unlock();
}
kgl_connection_result add_request(kconnection *c) {
	unsigned max = (unsigned)conf.max;
	unsigned max_per_ip = conf.max_per_ip;
	kassert(c->server);
	if (TEST(c->server->flags, WORK_MODEL_MANAGE)) {
		if (max > 0) {
			max += 100;
		}
		if (max_per_ip > 0) {
			max_per_ip += 10;
		}
	}
	kgl_pool_cleanup_t *pt;
	intmap::iterator it2;
	ip_addr ip;
	ipLock.Lock();
	if (max > 0 && total_connect >= max) {
		//printf("total_connect=%d,max=%d\n",total_connect,max);
		ipLock.Unlock();
		return kgl_connection_too_many;
	}
	if (conf.keep_alive_count > 0 && total_connect >= conf.keep_alive_count) {
		//SET(c->st_flags, STF_NO_KA);
	}
	if (max_per_ip == 0) {
		total_connect++;
#ifdef RQ_LEAK_DEBUG
		klist_append(&all_connection, &c->queue_edge);
#endif
		ipLock.Unlock();
		pt = kgl_pool_cleanup_add(c->pool,0);
		pt->handler = del_request;
		pt->data = c;
		return kgl_connection_success;
	}
	ksocket_ipaddr(&c->addr, &ip);
	it2 = m_ip.find(ip);
	if (it2 == m_ip.end()) {
		m_ip.insert(std::pair<ip_addr, unsigned>(ip, 1));
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
	pt = kgl_pool_cleanup_add(c->pool, 0);
	pt->handler = del_request_per_ip;
	pt->data = c;
	return kgl_connection_success;
max_per_ip: ipLock.Unlock();
	
	return kgl_connection_per_limit;
}
static kev_result handle_http_request(kconnection *cn)
{
	
	KHttpSink *sink = new KHttpSink(cn);
	KHttpRequest *rq = new KHttpRequest(sink, NULL);
	sink->cn->st.app_data = rq;
	return sink->ReadHeader(rq);
}
static kev_result result_proxy_request(void *arg, int got)
{
	kconnection *cn = (kconnection *)arg;
	if (got < 0) {
		kconnection_destroy(cn);
		return kev_destroy;
	}
	return handle_http_request(cn);
}
static kev_result handle_request(kconnection *cn)
{
#ifdef ENABLE_PROXY_PROTOCOL
	if (TEST(cn->server->flags, WORK_MODEL_PROXY)) {
		return handl_proxy_request(cn, result_proxy_request);
	}
#endif
	return handle_http_request(cn);
}

static kev_result handle_ssl_accept(void *arg,int got)
{
	kconnection *cn = (kconnection *)arg;
	if (got < 0) {
		kconnection_destroy(cn);
		return kev_destroy;
	}
#if (TLSEXT_TYPE_next_proto_neg && ENABLE_HTTP2)
	kassert(cn && cn->st.ssl);
	const unsigned char *data = NULL;
	unsigned len = 0;
	kgl_ssl_get_next_proto_negotiated(cn->st.ssl->ssl, &data, &len);
	if (len == sizeof(KGL_HTTP_V2_NPN_NEGOTIATED) - 1 &&
		memcmp(data, KGL_HTTP_V2_NPN_NEGOTIATED, len) == 0) {
		KHttp2 *http2 = new KHttp2();
		SET(cn->st.st_flags,STF_APP_HTTP2);
		cn->st.app_data = http2;
		http2->server(cn);
		return kev_ok;
	}
#endif
	return handle_http_request(cn);
}
inline static const char *get_connection_result_msg(kgl_connection_result errorCode) {
	switch (errorCode) {
	case kgl_connection_success:
		return "success";
	case kgl_connection_too_many:
		return "too many connection";
	case kgl_connection_per_limit:
		return "per ip limit";
	default:
		break;
	}
	return "unknow error";
}
void handle_connection_failed(kconnection *c, kgl_connection_result result)
{
	char ips[MAXIPLEN];
	ksocket_sockaddr_ip(&c->addr, ips, MAXIPLEN);
	klog(KLOG_ERR, "cann't service request %s:%d %s\n", ips,ksocket_addr_port(&c->addr), get_connection_result_msg(result));
#ifdef KSOCKET_SSL
	if (c->server->ssl_ctx) {
		kconnection_destroy(c);
		return;
	}
#endif
#ifdef _WIN32
	ksocket_no_block(c->st.fd);
#endif
	send(c->st.fd, KGL_BUSY_MSG,sizeof(KGL_BUSY_MSG),0);
	kconnection_destroy(c);
}
void handle_connection(kconnection *c, void *ctx)
{
	kgl_connection_result result = add_request(c);
	if (unlikely(result != kgl_connection_success)) {
		handle_connection_failed(c, result);
		return;
	}	
#ifdef KSOCKET_SSL
	if (c->server->ssl_ctx) {
		if (!kconnection_ssl_accept(c, c->server->ssl_ctx)) {
			kconnection_destroy(c);
			return;
		}
		kconnection_ssl_handshake(c, handle_ssl_accept, c);
		return;
	}
#endif
	handle_http_request(c);
}
