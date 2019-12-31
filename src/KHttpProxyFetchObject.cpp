/*
 * KHttpProxyFetchObject.cpp
 *
 *  Created on: 2010-4-20
 *      Author: keengo
 */
#include "do_config.h"
#include "KHttpProxyFetchObject.h"
#include "lib.h"
#include "http.h"
#include "kmd5.h"
#include "log.h"
#include "KHttpResponseParser.h"
#include "KPoolableSocketContainer.h"
#include "KRewriteMarkEx.h"
#include "kmalloc.h"
#include "KSimulateRequest.h"
void print_buff(kbuf *buf) {
	while (buf && buf->used > 0) {
		char *s = (char *) xmalloc(buf->used+1);
		kgl_memcpy(s, buf->data, buf->used);
		s[buf->used] = '\0';
		printf("%s", s);
		xfree(s);
		buf = buf->next;
	}
}
void upstream_sign_request(KHttpRequest *rq, KHttpEnv *s)
{
	KStringBuf v;
	if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
		v.WSTR("p=https,");
	}
	v.WSTR("sp=");
	v << rq->GetSelfPort();
	if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
		v.WSTR("s");
	}
	v.WSTR(",ip=");
	v << rq->getClientIp();
	v.WSTR(",t=");
	v << (int)kgl_current_sec;
	char buf[33];
	KMD5_CTX context;
	unsigned char digest[17];
	KMD5Init(&context);
	KMD5Update(&context, (unsigned char *)v.getBuf(), v.getSize());
	int upstream_sign_len = conf.upstream_sign_len;
	if (upstream_sign_len > (int)sizeof(conf.upstream_sign)) {
		upstream_sign_len = sizeof(conf.upstream_sign);
	}
	KMD5Update(&context, (unsigned char *)conf.upstream_sign, upstream_sign_len);
	KMD5Final(digest, &context);
	make_digest(buf, digest);
	v.WSTR("|");
	v.write_all(buf, 32);
	s->add(kgl_expand_string(X_REAL_IP_SIGN), v.getBuf(), v.getSize());
}
void KHttpProxyFetchObject::buildHead(KHttpRequest *rq)
{
	assert(buffer == NULL);
	buffer = new KSocketBuffer(16384);
	if (!client->BuildHttpHeader(rq, buffer)) {		
		return;
	}
	if (client->IsMultiStream()) {
		parser_ctx.proto = Proto_spdy;
		return;
	}
	parser_ctx.proto = Proto_http;
}

