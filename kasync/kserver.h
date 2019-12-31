#ifndef MSOCKET_KSERVER_SELECTABLE_H
#define MSOCKET_KSERVER_SELECTABLE_H
#include <assert.h>
#include "kfeature.h"
#include "kselectable.h"
#include "kcountable.h"
#include "ksocket.h"
#include "kconnection.h"
#include "kmalloc.h"
KBEGIN_DECLS
typedef void (*kserver_accept_callback)(kconnection *c,void *ctx);
typedef void (*kserver_close_callback)(void *ctx);
struct kserver_selectable_s{
	kselectable st;
	kserver *server;
	kserver_selectable *next;
#ifdef _WIN32
	char *tmp_addr_buf;
	SOCKET accept_sockfd;
#endif
};
struct kserver_s {
	kcountable_t refs;
	kserver_accept_callback accept_callback;
	kserver_close_callback  close_callback;
	void *ctx;
	kserver_selectable *ss;
#ifdef KSOCKET_UNIX
	union {
		sockaddr_i addr;
		struct sockaddr_un un_addr;
	};
#else
	sockaddr_i addr;
#endif
#ifdef KSOCKET_SSL
	SSL_CTX *ssl_ctx;
	bool http2;
	bool early_data;
#endif
	uint8_t flags;
	uint8_t ssl : 1;
	uint8_t closed:1;
	uint8_t started:1;
	uint8_t dynamic:1;
	uint8_t static_flag:1;
	uint8_t remove_static_flag : 1;
};
INLINE bool is_server_multi_selectable(kserver *server) {
	kassert(server->ss);
	return (server->ss->next != NULL);
}
kserver *kserver_init();
void kserver_bind(kserver *server,kserver_accept_callback accept_callback, kserver_close_callback close_callback, void *ctx);
bool kserver_open(kserver *server, const char *ip, uint16_t port, int flag);
void kserver_close(kserver *server);
void kserver_release(kserver *server);
bool kserver_accept(kserver *server);
INLINE void kserver_refs(kserver *server)
{
	katom_inc((void *)&server->refs);
}
KEND_DECLS
#endif
