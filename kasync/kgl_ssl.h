#ifndef KSSH_H_99
#define KSSH_H_99
#include "kfeature.h"
#include "kforwin32.h"
#ifdef KSOCKET_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/ocsp.h>
#include "kselector.h"
#include "kssl_bio.h"
#define KGL_HTTP_NPN_ADVERTISE  "\x08http/1.1"
KBEGIN_DECLS
typedef void (*kgl_ssl_npn_f)(void *ssl_ctx_data, const unsigned char **out, unsigned int *outlen);
typedef void *(*kgl_ssl_create_sni_f)(SSL *ssl, kconnection *c, const char *hostname);
typedef void (*kgl_ssl_free_sni_f)(void *sni);
typedef enum
{
	ret_error,
	ret_ok,
	ret_want_read,
	ret_want_write,
	ret_sni_resolve
} kssl_status;

typedef struct {
	SSL *ssl;
#ifdef ENABLE_KSSL_BIO
	kssl_bio bio[2];
#endif
#ifdef SSL_READ_EARLY_DATA_SUCCESS
	u_char early_buf;
	uint8_t try_early_data : 1;
	uint8_t in_early : 1;
	uint8_t early_preread : 1;
#endif
} kssl_session;

void kssl_init(kgl_ssl_npn_f npn, kgl_ssl_create_sni_f create_sni, kgl_ssl_free_sni_f free_sni);
void kgl_ssl_ctx_set_early_data(SSL_CTX *ssl_ctx, bool early_data);
SSL_CTX *kgl_ssl_ctx_new_server(const char *cert_file, const char *key_file, const char *ca_path,const char *ca_file,void *ssl_ctx_data);
SSL_CTX *kgl_ssl_ctx_new_client(const char *ca_path, const char *ca_file,void *ssl_ctx_data);

kssl_status kgl_ssl_shutdown(SSL *ssl);
kssl_status kgl_ssl_handshake(SSL *ssl);
kssl_status kgl_ssl_handshake_status(SSL *ssl, int re);
void kgl_ssl_get_next_proto_negotiated(SSL *ssl,const unsigned char **data, unsigned *len);
void kgl_ssl_ctx_set_protocols(SSL_CTX *ctx, const char *protocols);
bool kgl_ssl_ctx_set_cipher_list(SSL_CTX *ctx, const char *cipher);
extern int kangle_ssl_conntion_index;
extern int kangle_ssl_ctx_index;
KEND_DECLS
#endif
#endif
