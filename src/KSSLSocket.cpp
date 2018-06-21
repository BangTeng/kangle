#include "KSSLSocket.h"
#include "KHttpRequest.h"
#include "KSelector.h"
#include "ssl_utils.h"
#include "KAddr.h"

#ifdef ENABLE_TCMALLOC
#include "google/heap-checker.h"
#endif
#ifdef KSOCKET_SSL

static KMutex *ssl_lock = NULL;
int kangle_ssl_conntion_index;
int kangle_ssl_ctx_index;
typedef struct {
	kgl_str_t                 name;
	int                       mask;
} kgl_string_bitmask_t;
#define KGL_SSL_SSLv2    0x0002
#define KGL_SSL_SSLv3    0x0004
#define KGL_SSL_TLSv1    0x0008
#define KGL_SSL_TLSv1_1  0x0010
#define KGL_SSL_TLSv1_2  0x0020
static kgl_string_bitmask_t  kgl_ssl_protocols[] = {
	{ kgl_string("SSLv2"), KGL_SSL_SSLv2 },
	{ kgl_string("SSLv3"), KGL_SSL_SSLv3 },
	{ kgl_string("TLSv1"), KGL_SSL_TLSv1 },
	{ kgl_string("TLSv1.1"), KGL_SSL_TLSv1_1 },
	{ kgl_string("TLSv1.2"), KGL_SSL_TLSv1_2 },
	{ kgl_null_string, 0 }
};
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
#include "KVirtualHostManage.h"
/*
 * return 0 failed
 * return 1 success
 * return -1 retry calse SSL_get_error SSL_ERROR_WANT_X509_LOOKUP
 */
int httpSSLCertCallback(SSL *ssl, void *arg)
{
	const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (servername == NULL) {
		return 1;
	}
	KConnectionSelectable *c = (KConnectionSelectable *)SSL_get_ex_data(ssl, kangle_ssl_conntion_index);
	if (c == NULL) {
		return 0;
	}
	if (c->sni == NULL) {
		c->sni = new KSSLSniContext;
		c->sni->result = query_vh_host_not_found;
	}
	if (c->sni->svh) {
		return 1;
	}
	if (c->sni->result==query_vh_host_not_found) {
		c->sni->result = conf.gvm->queryVirtualHost(c->ls,&c->sni->svh,servername,0);
	}
	if (query_vh_success != c->sni->result) {
		return 1;
	}
	if (c->sni->svh->vh && c->sni->svh->vh->ssl_ctx) {
		SSL_set_SSL_CTX(ssl, c->sni->svh->vh->ssl_ctx);
	}
	return 1;
}
static int http_ssl_sni(SSL *ssl)
{
	const char *servername = SSL_get_servername(ssl, TLSEXT_NAMETYPE_host_name);
	if (servername == NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	KConnectionSelectable *c = (KConnectionSelectable *)SSL_get_ex_data(ssl, kangle_ssl_conntion_index);
	if (c == NULL) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	if (c->sni) {
		return SSL_TLSEXT_ERR_OK;
	}
	c->sni = new KSSLSniContext;
	c->sni->result = query_vh_host_not_found;
	if (c->sni->result == query_vh_host_not_found) {
		c->sni->result = conf.gvm->queryVirtualHost(c->ls, &c->sni->svh, servername, 0);
		if (c->sni->result != query_vh_success) {
			return SSL_TLSEXT_ERR_OK;
		}
	}
#ifdef ENABLE_SVH_SSL
	if (c->sni->svh->ssl_ctx) {
		SSL_set_SSL_CTX(ssl, c->sni->svh->ssl_ctx);
		return SSL_TLSEXT_ERR_OK;
	}
#endif
	if (c->sni->svh->vh && c->sni->svh->vh->ssl_ctx) {
		SSL_set_SSL_CTX(ssl, c->sni->svh->vh->ssl_ctx);
	}
	return SSL_TLSEXT_ERR_OK;
}
//sni lookup server name
int httpSSLServerName(SSL *ssl,int *ad,void *arg)
{
	return http_ssl_sni(ssl);
}
#endif
RSA * kgl_ssl_rsa512_key_callback(SSL *ssl_conn, int is_export,int key_length)
{
	static RSA  *key;

	if (key_length != 512) {
		return NULL;
	}

#ifndef OPENSSL_NO_DEPRECATED

	if (key == NULL) {
		key = RSA_generate_key(512, RSA_F4, NULL, NULL);
	}

#endif

	return key;
}


bool kgl_ssl_dhparam(SSL_CTX *ctx)
{
	DH   *dh;
	//BIO  *bio;

	/*
	* -----BEGIN DH PARAMETERS-----
	* MIGHAoGBALu8LcrYRnSQfEP89YDpz9vZWKP1aLQtSwju1OsPs1BMbAMCducQgAxc
	* y7qokiYUxb7spWWl/fHSh6K8BJvmd4Bg6RqSp1fjBI9osHb302zI8pul34HcLKcl
	* 7OZicMyaUDXYzs7vnqAnSmOrHlj6/UmI0PZdFGdX2gcd8EXP4WubAgEC
	* -----END DH PARAMETERS-----
	*/

	static unsigned char dh1024_p[] = {
		0xBB, 0xBC, 0x2D, 0xCA, 0xD8, 0x46, 0x74, 0x90, 0x7C, 0x43, 0xFC, 0xF5,
		0x80, 0xE9, 0xCF, 0xDB, 0xD9, 0x58, 0xA3, 0xF5, 0x68, 0xB4, 0x2D, 0x4B,
		0x08, 0xEE, 0xD4, 0xEB, 0x0F, 0xB3, 0x50, 0x4C, 0x6C, 0x03, 0x02, 0x76,
		0xE7, 0x10, 0x80, 0x0C, 0x5C, 0xCB, 0xBA, 0xA8, 0x92, 0x26, 0x14, 0xC5,
		0xBE, 0xEC, 0xA5, 0x65, 0xA5, 0xFD, 0xF1, 0xD2, 0x87, 0xA2, 0xBC, 0x04,
		0x9B, 0xE6, 0x77, 0x80, 0x60, 0xE9, 0x1A, 0x92, 0xA7, 0x57, 0xE3, 0x04,
		0x8F, 0x68, 0xB0, 0x76, 0xF7, 0xD3, 0x6C, 0xC8, 0xF2, 0x9B, 0xA5, 0xDF,
		0x81, 0xDC, 0x2C, 0xA7, 0x25, 0xEC, 0xE6, 0x62, 0x70, 0xCC, 0x9A, 0x50,
		0x35, 0xD8, 0xCE, 0xCE, 0xEF, 0x9E, 0xA0, 0x27, 0x4A, 0x63, 0xAB, 0x1E,
		0x58, 0xFA, 0xFD, 0x49, 0x88, 0xD0, 0xF6, 0x5D, 0x14, 0x67, 0x57, 0xDA,
		0x07, 0x1D, 0xF0, 0x45, 0xCF, 0xE1, 0x6B, 0x9B
	};

	static unsigned char dh1024_g[] = { 0x02 };




		dh = DH_new();
		if (dh == NULL) {
		
			return false;
		}

		dh->p = BN_bin2bn(dh1024_p, sizeof(dh1024_p), NULL);
		dh->g = BN_bin2bn(dh1024_g, sizeof(dh1024_g), NULL);

		if (dh->p == NULL || dh->g == NULL) {
			
			DH_free(dh);
			return false;
		}
		SSL_CTX_set_tmp_dh(ctx, dh);
		DH_free(dh);
		return true;
}


bool kgl_ssl_ecdh_curve(SSL_CTX *ctx,const char *name)
{
#if OPENSSL_VERSION_NUMBER >= 0x0090800fL
#ifndef OPENSSL_NO_ECDH
	int      nid;
	EC_KEY  *ecdh;

	/*
	* Elliptic-Curve Diffie-Hellman parameters are either "named curves"
	* from RFC 4492 section 5.1.1, or explicitly described curves over
	* binary fields. OpenSSL only supports the "named curves", which provide
	* maximum interoperability.
	*/

	nid = OBJ_sn2nid(name);
	if (nid == 0) {
		return false;
	}

	ecdh = EC_KEY_new_by_curve_name(nid);
	if (ecdh == NULL) {
		return false;
	}

	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_ECDH_USE);

	SSL_CTX_set_tmp_ecdh(ctx, ecdh);
	EC_KEY_free(ecdh);
#endif
#endif

	return true;
}
#if defined(TLSEXT_TYPE_next_proto_neg) || defined(TLSEXT_TYPE_application_layer_protocol_negotiation)
int httpSSLNpnSelected2(SSL *ssl, unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg)
{
	return httpSSLNpnSelected(ssl, (const unsigned char **)out, outlen, in, inlen, arg);
}
int httpSSLNpnSelected(SSL *ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg)
{	
	unsigned char *selected_protocol = (unsigned char *)KGL_HTTP_NPN_ADVERTISE;
	size_t selected_len = sizeof(KGL_HTTP_NPN_ADVERTISE) - 1;
#ifdef ENABLE_HTTP2
	http_ssl_sni(ssl);
	SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
	bool *spdy = (bool *)SSL_CTX_get_ex_data(ctx, kangle_ssl_ctx_index);
	if (spdy && *spdy) {
		selected_protocol = (unsigned char *)KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE;
		selected_len = sizeof(KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE) - 1;
	}
#endif
	if (SSL_select_next_proto(
		(unsigned char **)out,
		outlen,
		selected_protocol,
		selected_len,
		in,
		inlen
		) != OPENSSL_NPN_NEGOTIATED) {
		return SSL_TLSEXT_ERR_NOACK;
	}
	//klog(KLOG_DEBUG,"SSL ALPN selected: %*s", *outlen, *out);
	return SSL_TLSEXT_ERR_OK;
}
int httpSSLNpnAdvertised(SSL *ssl,const unsigned char **out, unsigned int *outlen, void *arg)
{
#ifdef ENABLE_HTTP2
	SSL_CTX *ctx = SSL_get_SSL_CTX(ssl);
	bool *spdy = (bool *)SSL_CTX_get_ex_data(ctx, kangle_ssl_ctx_index);
	if (spdy && *spdy) {
		*out = (unsigned char *)KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE;
		*outlen = sizeof(KGL_HTTP_V2_NPN_ADVERTISE KGL_HTTP_NPN_ADVERTISE) - 1;
		return SSL_TLSEXT_ERR_OK;
	}
#endif
	*out = (unsigned char *) KGL_HTTP_NPN_ADVERTISE;
	*outlen = sizeof(KGL_HTTP_NPN_ADVERTISE) - 1;
	return SSL_TLSEXT_ERR_OK;
}
#endif
static unsigned long __get_thread_id (void)
{
	return (unsigned long) pthread_self();
}
static void __lock_thread (int mode, int n, const char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		ssl_lock[n].Lock();
	} else {
		ssl_lock[n].Unlock();
	}
}
bool kgl_ssl_session_id_context(SSL_CTX *ssl_ctx, const char *cert_file)
{
	int                   n, i;
//	X509                 *cert;
	X509_NAME            *name;
	EVP_MD_CTX            md;
	unsigned int          len;
	STACK_OF(X509_NAME)  *list;
	u_char                buf[EVP_MAX_MD_SIZE];
	KFile fp;
	/*
	* Session ID context is set based on the string provided,
	* the server certificate, and the client CA list.
	*/

	EVP_MD_CTX_init(&md);

	if (EVP_DigestInit_ex(&md, EVP_sha1(), NULL) == 0) {
		klog(KLOG_ERR,"EVP_DigestInit_ex() failed");
		goto failed;
	}

	if (EVP_DigestUpdate(&md, cert_file, strlen(cert_file)) == 0) {
		klog(KLOG_ERR,"EVP_DigestUpdate() failed");
		goto failed;
	}
	
	if (fp.open(cert_file, fileRead)) {
		char buffer[512];
		int total_read = 0;
		while (total_read < 1048576) {
			int read_len = fp.read(buffer, sizeof(buffer));
			if (read_len <= 0) {
				break;
			}
			total_read += read_len;
			if (EVP_DigestUpdate(&md, buffer, read_len) == 0) {
				klog(KLOG_ERR,"EVP_DigestUpdate() failed");
				break;
			}
		}
	}
	fp.close();
	list = SSL_CTX_get_client_CA_list(ssl_ctx);

	if (list != NULL) {
		n = sk_X509_NAME_num(list);

		for (i = 0; i < n; i++) {
			name = sk_X509_NAME_value(list, i);

			if (X509_NAME_digest(name, EVP_sha1(), buf, &len) == 0) {
				klog(KLOG_ERR,"X509_NAME_digest() failed");
				goto failed;
			}

			if (EVP_DigestUpdate(&md, buf, len) == 0) {
				klog(KLOG_ERR,"EVP_DigestUpdate() failed");
				goto failed;
			}
		}
	}

	if (EVP_DigestFinal_ex(&md, buf, &len) == 0) {
		klog(KLOG_ERR,"EVP_DigestUpdate() failed");
		goto failed;
	}

	EVP_MD_CTX_cleanup(&md);

	if (SSL_CTX_set_session_id_context(ssl_ctx, buf, len) == 0) {
		klog(KLOG_ERR,"SSL_CTX_set_session_id_context() failed");
		return false;
	}
	return true;

failed:

	EVP_MD_CTX_cleanup(&md);

	return false;
}

void init_ssl()
{
#ifdef ENABLE_TCMALLOC
	HeapLeakChecker::Disabler disabler;
#endif
	load_ssl_library();
	if ((CRYPTO_get_id_callback()      == NULL) &&
	    (CRYPTO_get_locking_callback() == NULL))
	{
		//cuint_t n;

		CRYPTO_set_id_callback (__get_thread_id);
		CRYPTO_set_locking_callback (__lock_thread);

		int locks_num = CRYPTO_num_locks();
		ssl_lock = new KMutex[locks_num];	
	}
}
void resultSSLAccept(void *arg,int got);
void resultSSLAccept(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	assert(TEST(rq->workModel,WORK_MODEL_SSL));
	if (got<0) {
		delete rq;
		return;
	}
	KSSLSocket *socket = static_cast<KSSLSocket *>(rq->c->socket);
	ssl_status status = socket->handshake();
#ifdef ENABLE_KSSL_BIO
	if (status!=ret_error && BIO_pending(socket->ssl_bio[WRITE_PIPE].bio)>0) {
		rq->c->write(rq, resultSSLAccept, NULL);
		return;
	}
#endif
	switch(status){
	case ret_ok:
		{
#if (TLSEXT_TYPE_next_proto_neg && ENABLE_HTTP2)
			const unsigned char *data = NULL;
			unsigned len = 0;
			socket->get_next_proto_negotiated(&data,&len);
			if (len == sizeof(KGL_HTTP_V2_NPN_NEGOTIATED)-1 &&
				memcmp(data,KGL_HTTP_V2_NPN_NEGOTIATED,len)==0) {
				KConnectionSelectable *c = rq->c;
				if (c->sni) {
					delete c->sni;
					c->sni = NULL;
				}
				rq->c = NULL;
				//c->removeRequest(rq,false);
				delete rq;
				c->http2 = new KHttp2();
				c->set_flag(STF_APP_HTTP2);
				c->app_data.http2 = c->http2;
				c->http2->server(c);
				return;
			}
#endif
			rq->init(NULL);
			SET(rq->raw_url.flags,KGL_URL_SSL);
			rq->c->read(rq,resultRequestRead,bufferRequestRead);
			return;
		}
	case ret_want_read:
#ifndef ENABLE_KSSL_BIO
		rq->c->clear_flag(STF_RREADY);
#endif
		rq->c->read(rq,resultSSLAccept,NULL);
		return;
	case ret_want_write:
#ifndef ENABLE_KSSL_BIO
		rq->c->clear_flag(STF_WREADY);
#endif
		rq->c->write(rq,resultSSLAccept,NULL);
		return;
	default:
		delete rq;
		return;
	}
}
KSSLSocket::KSSLSocket(SSL_CTX *ctx) {
	ssl = NULL;
	this->ctx = ctx;
#ifdef ENABLE_KSSL_BIO
	memset(&ssl_bio, 0, sizeof(ssl_bio));
#endif
}
KSSLSocket::~KSSLSocket() {
	if (ssl) {
		SSL_free(ssl);
	}

}
void KSSLSocket::get_next_proto_negotiated(const unsigned char **data,unsigned *len)
{
#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
	SSL_get0_alpn_selected(ssl, data, len);
#ifdef TLSEXT_TYPE_next_proto_neg
	if (*len == 0) {
		SSL_get0_next_proto_negotiated(ssl, data, len);
	}
#endif
#else
#ifdef TLSEXT_TYPE_next_proto_neg
	SSL_get0_next_proto_negotiated(ssl,data,len);
#endif
#endif
}
SSL_CTX * KSSLSocket::init_server(const char *cert_file, const char *key_file,const char *verified_file) {
	SSL_CTX * ctx = init_ctx(true);
	if (ctx == NULL) {
		fprintf(stderr, "cann't init_ctx\n");
		return NULL;
	}
	if (cert_file == NULL) {
		cert_file = key_file;
	}
	if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
		klog(KLOG_ERR,
				"SSL use certificate file [%s]: Error: %s\n",
				cert_file,
				ERR_error_string(ERR_get_error(), NULL));
		clean_ctx(ctx);
		return NULL;
	}
	if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
		klog(KLOG_ERR,
			"SSL use privatekey file [%s]: Error: %s\n",
			key_file,
			ERR_error_string(ERR_get_error(), NULL));
		clean_ctx(ctx);
		return NULL;
	}

	if (!SSL_CTX_check_private_key(ctx)) {
		klog(KLOG_ERR, 
			"SSL check_private_key [%s] [%s]: Error: %s\n", 
			cert_file,
			key_file,
			ERR_error_string(ERR_get_error(), NULL));
		clean_ctx(ctx);
		return NULL;
	}
	if (verified_file) {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER
				| SSL_VERIFY_FAIL_IF_NO_PEER_CERT, NULL);
		SSL_CTX_set_verify_depth(ctx, 1);
		if (SSL_CTX_load_verify_locations(ctx, verified_file, NULL) <= 0) {
			fprintf(stderr, "SSL error %s:%d: Error allocating handle: %s\n",
					__FILE__, __LINE__, ERR_error_string(ERR_get_error(), NULL));
			clean_ctx(ctx);
			return NULL;
		}
	}
	/*
	int session_context_len = strlen(cert_file);
	const char *session_context = cert_file;
	int pos = session_context_len - SSL_MAX_SSL_SESSION_ID_LENGTH;
	if (pos>0) {
		session_context_len -= pos;
		session_context += pos;
	}
	SSL_CTX_set_session_id_context(ctx,(const unsigned char *)session_context,session_context_len);
	*/
	kgl_ssl_session_id_context(ctx, cert_file);
	SSL_CTX_set_session_cache_mode(ctx,SSL_SESS_CACHE_SERVER);
	//SSL_CTX_sess_set_cache_size(ctx,1000);
	return ctx;
}
bool KSSLSocket::verifiedSSL() {
	if (SSL_get_verify_result(ssl) != X509_V_OK) {
		return false;
	}
	return true;
}
SSL_CTX * KSSLSocket::init_ctx(bool server) {
	SSL_CTX *ctx;
	if (server) {
		ctx = SSL_CTX_new(SSLv23_method());
	} else {
		ctx = SSL_CTX_new(SSLv23_method());
	}
	if (ctx == NULL) {
		fprintf(stderr, "ssl_ctx_new function error\n");
		return NULL;
	}
	/* client side options */

	SSL_CTX_set_options(ctx, SSL_OP_MICROSOFT_SESS_ID_BUG);
	SSL_CTX_set_options(ctx, SSL_OP_NETSCAPE_CHALLENGE_BUG);

	/* server side options */

	SSL_CTX_set_options(ctx, SSL_OP_SSLREF2_REUSE_CERT_TYPE_BUG);
	SSL_CTX_set_options(ctx, SSL_OP_MICROSOFT_BIG_SSLV3_BUFFER);

#ifdef SSL_OP_MSIE_SSLV2_RSA_PADDING
	/* this option allow a potential SSL 2.0 rollback (CAN-2005-2969) */
	SSL_CTX_set_options(ctx, SSL_OP_MSIE_SSLV2_RSA_PADDING);
#endif
	SSL_CTX_set_options(ctx, SSL_OP_SSLEAY_080_CLIENT_DH_BUG);
	SSL_CTX_set_options(ctx, SSL_OP_TLS_D5_BUG);
	SSL_CTX_set_options(ctx, SSL_OP_TLS_BLOCK_PADDING_BUG);
	SSL_CTX_set_options(ctx, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
	SSL_CTX_set_options(ctx, SSL_OP_SINGLE_DH_USE);
#ifdef SSL_OP_NO_COMPRESSION
	SSL_CTX_set_options(ctx, SSL_OP_NO_COMPRESSION);
#endif
#ifdef SSL_MODE_RELEASE_BUFFERS
	SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);
#endif
#ifdef SSL_MODE_NO_AUTO_CHAIN
	SSL_CTX_set_mode(ctx, SSL_MODE_NO_AUTO_CHAIN);
#endif
	SSL_CTX_set_read_ahead(ctx, 0);
	//SSL_CTX_set_mode(ctx,SSL_MODE_ENABLE_PARTIAL_WRITE);
#ifndef LIBRESSL_VERSION_NUMBER
	SSL_CTX_set_tmp_rsa_callback(ctx, kgl_ssl_rsa512_key_callback);
#endif
	//disable SSLV2
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
	//kgl_ssl_dhparam(ssl_ctx);
	kgl_ssl_ecdh_curve(ctx, "prime256v1");
	return ctx;
}
SSL_CTX * KSSLSocket::init_client(const char *path, const char *file) {
	SSL_CTX *ctx = init_ctx(false);
	if (ctx) {
		if (file != NULL) {
			SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
			SSL_CTX_set_verify_depth(ctx, 1);

			if (SSL_CTX_load_verify_locations(ctx, file, path) <= 0) {
				fprintf(stderr, "SSL error %s:%d: Error allocating handle: %s\n",
						__FILE__, __LINE__, ERR_error_string(ERR_get_error(), NULL));
				clean_ctx(ctx);
				return NULL;
			}
		}
		SSL_CTX_set_session_id_context(ctx,(const unsigned char *)PROGRAM_NAME,sizeof(PROGRAM_NAME)-1);
		SSL_CTX_set_session_cache_mode(ctx,SSL_SESS_CACHE_BOTH);
	}
	return ctx;
}
void KSSLSocket::clean_ctx(SSL_CTX *ctx) {
	SSL_CTX_free(ctx);
}
void KSSLSocket::close() {
	if (ssl) {
		SSL_free(ssl);
	}
	ssl = NULL;
	KSocket::close();
}
int KSSLSocket::get_ssl_error(int re)
{
	return SSL_get_error(ssl,re);
}
int KSSLSocket::write(const char *str, int len) {
	return SSL_write(ssl, str, len);
}
int KSSLSocket::read(char *str, int len) {
	return SSL_read(ssl, str, len);
}
bool KSSLSocket::bind_fd()
{
	assert(ssl==NULL);
	ssl = SSL_new(ctx);
	if (ssl==NULL) {
		return false;
	}
	if (SSL_set_fd(ssl, sockfd)!=1) {
		return false;
	}
	SSL_set_accept_state(ssl);
#ifdef ENABLE_KSSL_BIO
	ssl_bio[0].bio = BIO_new(BIO_kgl_method());
	ssl_bio[1].bio = BIO_new(BIO_kgl_method());
	SSL_set_bio(ssl, ssl_bio[READ_PIPE].bio, ssl_bio[WRITE_PIPE].bio);
#endif
	return true;
}
void KSSLSocket::set_ssl_protocols(SSL_CTX *ctx, const char *protocols)
{	
	char *buf = strdup(protocols);
	char *hot = buf;
	int mask = 0;
	for (;;) {
		while (*hot && isspace((unsigned char)*hot)) {
			hot++;
		}
		char *p = hot;
		while (*p && !isspace((unsigned char)*p)) {
			p++;
		}		
		if (p == hot) {
			break;
		}	
		if (*p) {		
			*p = '\0';
			p++;
		}
		kgl_string_bitmask_t *h = kgl_ssl_protocols;
		while (h->name.data) {
			if (strcasecmp(h->name.data, hot) == 0) {
				SET(mask, h->mask);
			}
			h++;
		}
		if (*p == '\0') {
			break;
		}
		hot = p;
	}
	xfree(buf);
	if (mask > 0) {
		if (!(mask & KGL_SSL_SSLv2)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
		}
		if (!(mask & KGL_SSL_SSLv3)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
		}
		if (!(mask & KGL_SSL_TLSv1)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
		}
#ifdef SSL_OP_NO_TLSv1_1
		if (!(mask & KGL_SSL_TLSv1_1)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1);
		}
#endif
#ifdef SSL_OP_NO_TLSv1_2
		if (!(mask & KGL_SSL_TLSv1_2)) {
			SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_2);
		}
#endif
	}	
}
ssl_status KSSLSocket::ssl_shutdown()
{
	if (ssl == NULL) {
		return ret_error;
	}
	int n = SSL_shutdown(ssl);
	if (n == 1) {
		return ret_ok;
	}
	int err = SSL_get_error(ssl, n);
	switch (err) {
	case SSL_ERROR_WANT_READ:		
		return ret_want_read;
	case SSL_ERROR_WANT_WRITE:		
		return ret_want_write;
	default:
		return ret_error;
	}
}
ssl_status KSSLSocket::handshake() {
	assert(ssl);
	int re = SSL_do_handshake(ssl);
	if (re<=0) {
		int err = SSL_get_error(ssl, re);
		switch (err) {
		case SSL_ERROR_WANT_READ:
			return ret_want_read;
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
		case SSL_ERROR_WANT_ACCEPT:
			return ret_want_write;
		case SSL_ERROR_SYSCALL:
#ifndef _WIN32
			if (errno == EAGAIN) {
				//return ret_error;
			}
#endif
			err = errno;
			//printf("system errno=%d\n",err);
			return ret_error;
		case SSL_ERROR_SSL:
		case SSL_ERROR_ZERO_RETURN:
			//printf("error = %d\n",err);
			return ret_error;
		case SSL_ERROR_WANT_X509_LOOKUP:
			//printf("SSL_ERROR_WANT_X509_LOOKUP\n");
			return ret_sni_resolve;
		default:
			//printf("error = %d\n",err);
			return ret_error;
		}
	}
	if (ssl->s3) {
		ssl->s3->flags |= SSL3_FLAGS_NO_RENEGOTIATE_CIPHERS;
	}
	return ret_ok;
}
bool KSSLSocket::ssl_connect() {
	assert(ssl==NULL);
	assert(ctx);
	if ((ssl = SSL_new(ctx)) == NULL) {
		int err = ERR_get_error();
		fprintf(stderr, "SSL: Error allocating handle: %s\n", ERR_error_string(
				err, NULL));
		return false;
	}
	if (SSL_set_fd(ssl, sockfd)!=1) {
		return false;
	}
	SSL_set_connect_state(ssl);
#ifdef ENABLE_KSSL_BIO
	ssl_bio[0].bio = BIO_new(BIO_kgl_method());
	ssl_bio[1].bio = BIO_new(BIO_kgl_method());
	SSL_set_bio(ssl, ssl_bio[READ_PIPE].bio, ssl_bio[WRITE_PIPE].bio);
#endif
	return true;
}
bool KSSLSocket::setHostName(const char *hostname)
{
#ifdef SSL_CTRL_SET_TLSEXT_HOSTNAME
	SSL_set_tlsext_host_name(ssl,hostname);
	return true;
#else
	return false;
#endif
}
#endif
