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
#include "log.h"
#include "KHttpProtocolParser.h"
#include "KHttpObjectParserHook.h"
#include "KPoolableSocketContainer.h"
#include "KRewriteMarkEx.h"
#include "malloc_debug.h"
#include "lang.h"

#include "KSimulateRequest.h"
void print_buff(buff *buf) {
	while (buf && buf->used > 0) {
		char *s = (char *) xmalloc(buf->used+1);
		memcpy(s, buf->data, buf->used);
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
	v.WSTR("ip=");
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

Parse_Result KHttpProxyFetchObject::parseHead(KHttpRequest *rq,char *buf,int len)
{
	//fwrite(buf,1,len,stdout);
	assert(header && hot);
	assert(client->parser);
	int ret = client->parser->parse(header,hot-header,client->hook);
	switch(ret){
		case HTTP_PARSE_FAILED:
			//重置hot,这里一定要hot=NULL,因为nextBody里面要用到。
			hot = NULL;
			return Parse_Failed;
		case HTTP_PARSE_SUCCESS:
			if (rq->ctx->obj->data->status_code==100) {
				client->parser->setStarted(false);
				rq->ctx->obj->data->status_code = 0;
				if (client->parser->bodyLen>0) {
					return parseHead(rq,buf,len);
				}
				return Parse_Continue;
			}
			rq->ctx->obj->data->headers = client->parser->stealHeaders(rq->ctx->obj->data->headers);
			//重置hot
			hot = NULL;
			return Parse_Success;
	}
	return Parse_Continue;
}

void KHttpProxyFetchObject::buildHead(KHttpRequest *rq)
{
	assert(buffer == NULL);
	buffer = new KSocketBuffer(16384);
	client->prepare_parser(rq);

	char tmpbuff[50];
	KSocketBuffer &s = *buffer;
	KStreamHttpEnv env(buffer);
	struct KHttpHeader *av;
	const char *connectionState = "close";
	const char *meth = rq->getMethod();
	if (meth == NULL)
		return;
	char ips[MAXIPLEN];
	int via_inserted = FALSE;
	bool x_forwarded_for_inserted = false;
	int defaultPort = 80;
	if (TEST(rq->raw_url.flags,KGL_URL_SSL)) {			
		defaultPort = 443;
	}
	KUrl *url = rq->url;
	if (TEST(rq->filter_flags,RF_PROXY_RAW_URL) || !TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
		url = &rq->raw_url;
	}
	char *path = url->path;
	if (url==rq->url && TEST(url->flags,KGL_URL_ENCODE)) {
		size_t path_len;
		path = url_encode(url->path, strlen(url->path), &path_len);
	}
	s << meth << " ";
	if (TEST(rq->filter_flags,RF_PROXY_FULL_URL)) {
		if(TEST(rq->raw_url.flags,KGL_URL_SSL)) {
			s << "https";
		} else {
			s << "http";
		}
		s << "://" << url->host;
		if (url->port != defaultPort) {
			s << ":" << url->port;
		}
	}
	s << path;
	char *param_buf = NULL;
	const char *param = url->getParam(&param_buf);
	if (param) {		
		s << "?" << param;
	}
	if (param_buf) {
		free(param_buf);
	}
	s << " HTTP/1." << (int)rq->http_minor << "\r\n";
	s << "Host: " << url->host;
	if (url->port != defaultPort) {
		s << ":" << url->port;
	}
	s << "\r\n";
	av = rq->parser.getHeaders();
	
	while (av) {
#ifdef HTTP_PROXY
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
#endif

			if (strcasecmp(av->attr, X_REAL_IP_SIGN) == 0) {
				goto do_not_insert;
			}
			if (TEST(rq->filter_flags,RF_X_REAL_IP)) {
				if (is_attr(av, "X-Real-IP") || is_attr(av,"X-Forwarded-Proto")) {
					goto do_not_insert;
				}
			}
			if (!TEST(rq->filter_flags,RF_NO_X_FORWARDED_FOR) && is_attr(av, "X-Forwarded-For")) {
				if (x_forwarded_for_inserted) {
					goto do_not_insert;
				}
				x_forwarded_for_inserted = true;
				s << "X-Forwarded-For: " << av->val << ",";
				rq->c->socket->get_remote_ip(ips,sizeof(ips));
				s << ips << "\r\n";
				goto do_not_insert;

			}
			if (is_attr(av, "Via") && TEST(rq->filter_flags,RF_VIA)) {
				if (via_inserted) {
					goto do_not_insert;
				}
				insert_via(rq, s, av->val);
				via_inserted = true;
				goto do_not_insert;
			}

		if (is_attr(av,"Connection")) {
			goto do_not_insert;
		}
		if (is_attr(av,"Host")) {
			goto do_not_insert;
		}
		if (TEST(rq->flags,RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}

		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		s << av->attr << ": " << av->val << "\r\n";
		do_not_insert: av = av->next;
	}
	if (TEST(rq->flags, RQ_HAS_CONTENT_LEN)) {
		s.WSTR("Content-Length: ");
		int2string(rq->content_length,tmpbuff);
		s << tmpbuff;
		s.WSTR("\r\n");
	}
	if (rq->ctx->lastModified != 0) {
		mk1123time(rq->ctx->lastModified, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			s << "If-Range: ";
		} else {
			s << "If-Modified-Since: ";
		}
		s << tmpbuff << "\r\n";
	} else if (rq->ctx->if_none_match!=NULL) {
		if (rq->ctx->mt == modified_if_range_etag) {
			s.WSTR("If-Range: ");
		} else {
			s.WSTR("If-None-Match: ");
		}
		s.write_all(rq->ctx->if_none_match->data,rq->ctx->if_none_match->len);
		s.WSTR("\r\n");
	}
	
	if (TEST(rq->workModel,WORK_MODEL_INTERNAL)) {
		s << "User-Agent: " << PROGRAM_NAME << "/" << VERSION << "\r\n";
	}
	if (TEST(rq->flags,RQ_HAS_CONNECTION_UPGRADE)) {
		connectionState = "upgrade";
	} else if (!TEST(rq->filter_flags,RF_UPSTREAM_NOKA) && client->getLifeTime()>0) {
		connectionState = "keep-alive";
	}
	s << "Connection: " << connectionState << "\r\n";

		if (!TEST(rq->filter_flags,RF_NO_X_FORWARDED_FOR) && !x_forwarded_for_inserted) {
			rq->c->socket->get_remote_ip(ips,sizeof(ips));
			s << "X-Forwarded-For: " << ips << "\r\n";
		}
		if (TEST(rq->filter_flags,RF_VIA) && !via_inserted) {
			insert_via(rq, s, NULL);
		}
		if (TEST(rq->filter_flags,RF_X_REAL_IP)) {
			s.WSTR("X-Real-IP: ");
			s << rq->getClientIp();
			s.WSTR("\r\n");
			if(TEST(rq->raw_url.flags,KGL_URL_SSL)) {
				s.WSTR("X-Forwarded-Proto: https\r\n");
			} else {
				s.WSTR("X-Forwarded-Proto: http\r\n");
			}
		}

	s.WSTR("\r\n");
	if (TEST(rq->flags,RQ_HAS_CONNECTION_UPGRADE) && rq->parser.bodyLen>0) {
		//处理pre loaded post数据
		//printf("pre_post_len=%d\n",pre_post_len);
		s.write_all(rq->parser.body,rq->parser.bodyLen);
		rq->parser.body +=rq->parser.bodyLen;
		rq->parser.bodyLen = 0;
		rq->pre_post_length = 0;
	}
	//s.print();
	if (path != url->path) {
		xfree(path);
	}
}

