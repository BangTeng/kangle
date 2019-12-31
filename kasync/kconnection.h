#ifndef MSOCKET_KCONNECTION_SELECTABLE_H
#define MSOCKET_KCONNECTION_SELECTABLE_H
#include "kselectable.h"
#include "kmalloc.h"
KBEGIN_DECLS
typedef struct kserver_s kserver;
#ifdef ENABLE_PROXY_PROTOCOL
typedef struct kgl_proxy_pp2_tlv_s kgl_proxy_pp2_tlv;
typedef struct kgl_proxy_pp2_data_s kgl_proxy_pp2_data;
typedef struct kgl_proxy_protocol_s kgl_proxy_protocol;

struct kgl_proxy_pp2_tlv_s {
	uint8_t type;
	uint8_t length_hi;
	uint8_t length_lo;
};
struct kgl_proxy_pp2_data_s {
	uint8_t type;
	uint16_t len;
	char *data;
	kgl_proxy_pp2_data *next;
};
struct kgl_proxy_protocol_s {
	sockaddr_i *src;
	sockaddr_i *dst;
	kgl_proxy_pp2_data *data;
};
#endif
struct kconnection_s {
	kselectable st;
	sockaddr_i addr;
#ifdef KSOCKET_SSL
	void *sni;
#endif
#ifdef ENABLE_PROXY_PROTOCOL
	kgl_proxy_protocol *proxy;
#endif
	kgl_pool_t *pool;
	kserver *server;
};
struct kupstream_connection_s {
	kconnection *c;
};
INLINE void kconnection_delay(kconnection *c)
{
	ksocket_delay(c->st.fd);
}
INLINE void kconnection_no_delay(kconnection *c,bool forever)
{
	ksocket_no_delay(c->st.fd,forever);
}
void kconnection_destroy(kconnection *c);
kconnection *kconnection_new(sockaddr_i *addr);
bool kconnection_half_connect(kconnection *c, sockaddr_i *bind_addr, int tproxy_mask);
kev_result kconnection_connect(kconnection *c,result_callback cb, void *arg);
INLINE int kconnection_self_addr(kconnection *c,sockaddr_i *addr)
{
	socklen_t name_len = sizeof(sockaddr_i);
	return getsockname(c->st.fd, (struct sockaddr *)addr, &name_len);
}
#ifdef KSOCKET_SSL
kev_result kconnection_ssl_handshake(kconnection *c,result_callback cb, void *arg);
bool kconnection_ssl_connect(kconnection *c,SSL_CTX *ssl_ctx,const char *sni_hostname);
bool kconnection_ssl_accept(kconnection *c, SSL_CTX *ssl_ctx);
#endif
KEND_DECLS
#endif
