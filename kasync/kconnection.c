#include <assert.h>
#include <string.h>
#include "kconnection.h"
#include "ksocket.h"
#include "kserver.h"
#include "kselector.h"
#include "kssl_bio.h"
#include "kmalloc.h"

#ifdef KSOCKET_SSL
typedef struct {
	kconnection *c;
	result_callback cb;
	void *arg;
} kconnection_ssl_handshake_param;

kgl_ssl_create_sni_f kgl_ssl_create_sni = NULL;
kgl_ssl_free_sni_f kgl_ssl_free_sni = NULL;


#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
int kgl_ssl_sni(SSL *ssl, int *ad, void *arg)
{
	kassert(kgl_ssl_create_sni);
	const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (servername == NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	kconnection *c = (kconnection *)SSL_get_ex_data(ssl, kangle_ssl_conntion_index);
	if (c == NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	if (c->sni) {
		return SSL_TLSEXT_ERR_OK;
	}
	c->sni = kgl_ssl_create_sni(ssl, c, servername);
	return SSL_TLSEXT_ERR_OK;
}
#endif
#endif
#ifdef _WIN32
static int kconnection_buffer_addr(void *arg, LPWSABUF buffer, int bc)
{
	kconnection *c = (kconnection *)arg;
	buffer[0].iov_base = (char *)&c->addr;
	buffer[0].iov_len = ksocket_addr_len(&c->addr);
	return 1;
}
#endif
kconnection *kconnection_new(sockaddr_i *addr)
{
	kconnection *c = (kconnection *)xmalloc(sizeof(kconnection));
	memset(c, 0, sizeof(kconnection));
	c->pool = kgl_create_pool(8192);
	if (addr) {
		memcpy(&c->addr, addr, sizeof(sockaddr_i));
	}
	ksocket_init(c->st.fd);
	return c;
}
static void kconnection_real_destroy(kconnection *c)
{
	selectable_clean(&c->st);
#ifdef KSOCKET_SSL
	if (c->sni) {
		kgl_ssl_free_sni(c->sni);
	}
#endif
	if (c->server) {
		kserver_release(c->server);
	}
	kgl_destroy_pool(c->pool);
	xfree(c);
}
#ifdef KSOCKET_SSL
static kev_result result_ssl_shutdown(void *arg, int got)
{
	kconnection *c = (kconnection *)arg;
	if (got < 0) {
		kconnection_real_destroy(c);
		return kev_destroy;
	}
	kassert(ksocket_opened(c->st.fd));
	kssl_status status = kgl_ssl_shutdown(c->st.ssl->ssl);
#ifdef ENABLE_KSSL_BIO
	if (status != ret_error && BIO_pending(c->st.ssl->bio[OP_WRITE].bio) > 0) {
		return selectable_write(&c->st, result_ssl_shutdown, NULL, c);		
	}
#endif
	switch (status) {
	case SSL_ERROR_WANT_READ:
#ifndef ENABLE_KSSL_BIO
		selectable_clear_flags(&c->st,STF_RREADY);
#endif
		return selectable_read(&c->st, result_ssl_shutdown, NULL, c);
	case SSL_ERROR_WANT_WRITE:
#ifndef ENABLE_KSSL_BIO
		selectable_clear_flags(&c->st,STF_WREADY);
#endif
		return selectable_write(&c->st, result_ssl_shutdown, NULL, c);
	default:
		kconnection_real_destroy(c);
		return kev_destroy;
	}
}
#endif
void kconnection_destroy(kconnection *c)
{
#ifdef KSOCKET_SSL
	if (c->st.ssl) {
		c->st.app_data = NULL;
		if (!TEST(c->st.st_flags, STF_ERR)) {
			result_ssl_shutdown(c, 0);
			return;
		}
	}
#endif
	kconnection_real_destroy(c);
}
bool kconnection_half_connect(kconnection *c, sockaddr_i *bind_addr, int tproxy_mask)
{
	c->st.fd = ksocket_half_connect(&c->addr, bind_addr, tproxy_mask);
	return ksocket_opened(c->st.fd);
}
kev_result kconnection_connect(kconnection *c,result_callback cb, void *arg)
{
#ifdef _WIN32
	c->st.e[OP_READ].buffer = kconnection_buffer_addr;
	c->st.e[OP_READ].arg = c;
#endif
	if (!kgl_selector_module.connect(c->st.selector, &c->st, cb, arg)) {
		return cb(arg, -1);
	}
	return kev_ok;
}
#ifdef KSOCKET_SSL
static kev_result ssl_handshake_result(kconnection_ssl_handshake_param *sh,bool result)
{
	kev_result ret = sh->cb(sh->arg, result ? 0 : -1);
	xfree(sh);
	return ret;
}
static kev_result result_ssl_handshake(void *arg, int got)
{
	kconnection_ssl_handshake_param *sh = (kconnection_ssl_handshake_param *)arg;
	if (got < 0) {
		return ssl_handshake_result(sh, false);
	}
	kssl_session *ssl = sh->c->st.ssl;
	kassert(ssl);
	kssl_status status = kgl_ssl_handshake(ssl->ssl);
#ifdef ENABLE_KSSL_BIO
	if (status != ret_error && BIO_pending(ssl->bio[OP_WRITE].bio) > 0) {
		return selectable_write(&sh->c->st, result_ssl_handshake, NULL,sh);
	}
#endif
	switch (status) {
	case ret_ok:
		return ssl_handshake_result(sh, true);
	case ret_want_read:
#ifndef ENABLE_KSSL_BIO
		selectable_clear_flags(&sh->c->st, STF_RREADY);
#endif
		return selectable_read(&sh->c->st, result_ssl_handshake, NULL, sh);
	case ret_want_write:
#ifndef ENABLE_KSSL_BIO
		selectable_clear_flags(&sh->c->st, STF_WREADY);
#endif
		return selectable_write(&sh->c->st, result_ssl_handshake, NULL, sh);
	default:
		return ssl_handshake_result(sh, false);
	}
}
static void kconnection_ssl_init(kconnection *c, SSL *ssl)
{
	kassert(c->st.ssl == NULL);
	c->st.ssl = xmemory_new(kssl_session);
	memset(c->st.ssl, 0, sizeof(kssl_session));
#ifdef ENABLE_KSSL_BIO
	c->st.ssl->bio[0].bio = BIO_new(BIO_kgl_method());
	c->st.ssl->bio[1].bio = BIO_new(BIO_kgl_method());
	SSL_set_bio(ssl, c->st.ssl->bio[OP_READ].bio, c->st.ssl->bio[OP_WRITE].bio);
#endif
	c->st.ssl->ssl = ssl;
}
kev_result kconnection_ssl_handshake(kconnection *c,result_callback cb, void *arg)
{	
	kconnection_ssl_handshake_param *sh = xmemory_new(kconnection_ssl_handshake_param);
	memset(sh, 0, sizeof(kconnection_ssl_handshake_param));
	sh->c = c;
	sh->cb = cb;
	sh->arg = arg;
	return result_ssl_handshake(sh, 0);
}
static SSL *kconnection_new_ssl(kconnection *c,SSL_CTX *ssl_ctx)
{
	SSL *ssl = SSL_new(ssl_ctx);
	if (ssl == NULL) {		
		return NULL;
	}
#ifdef _WIN32
	ksocket_no_block(c->st.fd);
#endif
	if (SSL_set_fd(ssl, (int)c->st.fd) != 1) {
		SSL_free(ssl);		
		return NULL;
	}
	return ssl;
}
bool kconnection_ssl_connect(kconnection *c, SSL_CTX *ssl_ctx, const char *sni_hostname)
{
	SSL *ssl = kconnection_new_ssl(c, ssl_ctx);
	if (ssl == NULL) {
		return false;
	}
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	if (sni_hostname) {
		SSL_set_tlsext_host_name(ssl, sni_hostname);
	}
#endif
	SSL_set_connect_state(ssl);
	kconnection_ssl_init(c, ssl);
	return true;
}

bool kconnection_ssl_accept(kconnection *c, SSL_CTX *ssl_ctx)
{
	SSL *ssl = kconnection_new_ssl(c, ssl_ctx);
	if (ssl == NULL) {
		return false;
	}
	SSL_set_accept_state(ssl);
	SSL_set_ex_data(ssl, kangle_ssl_conntion_index, c);
	kconnection_ssl_init(c, ssl);
	return true;
}
#endif
