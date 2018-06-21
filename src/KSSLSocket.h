#ifndef KSSLSOCKET_H
#define KSSLSOCKET_H
#include "KSocket.h"
#include "KSelectable.h"

#ifdef KSOCKET_SSL
#ifdef ENABLE_KSSL_BIO
#include "KSSLBIO.h"
#endif
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/conf.h>
#include <openssl/engine.h>
#include <openssl/evp.h>
#include <openssl/ocsp.h>
#define KGL_HTTP_NPN_ADVERTISE  "\x08http/1.1"
enum ssl_status
{
	ret_error,
	ret_ok,
	ret_want_read,
	ret_want_write,
	ret_sni_resolve
};
class KSSLSocket: public KClientSocket {
public:
	KSSLSocket(SSL_CTX *ctx);
	~KSSLSocket();
	void close();
	bool bind_fd();
	//异步accept,返回-1，错误，0=成功，1=want read,2=want write
	ssl_status handshake();
	ssl_status ssl_shutdown();
	int read(char *buf,int len);
	int write(const char *buf,int len);
	int get_ssl_error(int re);
	bool ssl_connect();
	SSL *getSSL() {
		return ssl;
	}
	void get_next_proto_negotiated(const unsigned char **data,unsigned *len);
	bool verifiedSSL();
	static SSL_CTX * init_server(const char *cert_file, const char *key_file,
			const char *verified_file);
	static void set_ssl_protocols(SSL_CTX *ctx, const char *protocols);
	static SSL_CTX * init_client(const char *path, const char *file);
	static void clean_ctx(SSL_CTX *ctx);
	bool setHostName(const char *hostname);
#ifdef ENABLE_KSSL_BIO
	KSSLBIO ssl_bio[2];
#endif
private:
	static SSL_CTX * init_ctx(bool server);
	SSL *ssl;
	SSL_CTX *ctx;
};
void resultSSLAccept(void *arg,int got);
class KHttpRequest;
bool kgl_ssl_dhparam(SSL_CTX *ctx);
bool kgl_ssl_ecdh_curve(SSL_CTX *ctx, const char *name);
int httpSSLCertCallback(SSL *ssl, void *arg);
int httpSSLServerName(SSL *ssl,int *ad,void *arg);
int httpSSLNpnAdvertised(SSL *ssl_conn,const unsigned char **out, unsigned int *outlen, void *arg);
int httpSSLNpnSelected(SSL *ssl,const unsigned char **out,unsigned char *outlen,const unsigned char *in,unsigned int inlen,void *arg);
int httpSSLNpnSelected2(SSL *ssl, unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg);
void init_ssl();
extern int kangle_ssl_conntion_index;
extern int kangle_ssl_ctx_index;
#endif
#endif
