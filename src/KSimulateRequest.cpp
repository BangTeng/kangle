#include "KSimulateRequest.h"
#include "KHttpRequest.h"
#include "http.h"
#include "KSelectorManager.h"
#include "KHttpProxyFetchObject.h"
#include "KDynamicListen.h"
#include "KTimer.h"
#ifdef ENABLE_SIMULATE_HTTP
int asyncHttpRequest(kgl_async_http *ctx)
{
	if (ctx->postLen>0 && ctx->post==NULL) {
		return 1;
	}
	KSimulateSocket *ss = new KSimulateSocket;
	KConnectionSelectable *c = new KConnectionSelectable(ss);
	KHttpRequest *rq = new KHttpRequest(c);
	if (ctx->selector==0) {
		rq->c->selector = selectorManager.getSelector();
	} else {
		rq->c->selector = selectorManager.getSelectorByIndex(ctx->selector - 1);
	}
	if (!parse_url(ctx->url,&rq->raw_url)) {
		rq->raw_url.destroy();
		KStringBuf nu;
		nu << ctx->url << "/";
		if (!parse_url(nu.getString(), &rq->raw_url)) {
			delete rq;
			return 1;
		}
	}
	if (rq->raw_url.host==NULL) {
		delete rq;
		return 1;
	}
	if (TEST(rq->raw_url.flags,KGL_URL_ORIG_SSL)) {
		SET(rq->raw_url.flags,KGL_URL_SSL);
	}
	
	if (ctx->host) {
		ss->host = strdup(ctx->host);
	}
	ss->port = ctx->port;
	ss->life_time = ctx->life_time;
	ss->arg = ctx->arg;
	ss->post = ctx->post;
	ss->header = ctx->header;
	ss->body = ctx->body;
	KHttpHeader *head = ctx->rh;
	bool user_agent = false;
	while (head) {
		if (strcasecmp(head->attr,"User-Agent")==0) {
			user_agent = true;
		}
		//copy to header
		KHttpHeader *h = new_http_header(head->attr,head->attr_len,head->val,head->val_len);
		h->next = ss->rh;
		ss->rh = h;
		head = head->next;
	}
	if (!user_agent) {
		//add default user-agent header
		KHttpHeader *h = new_http_header(kgl_expand_string("User-Agent"),conf.serverName,conf.serverNameLength);
		h->next = ss->rh;
		ss->rh = h;
	}
	rq->c->socket = ss;
	rq->workModel = WORK_MODEL_SIMULATE;
	rq->init(NULL);
	if (TEST(ctx->flags, KF_SIMULATE_GZIP)) {
		KHttpHeader *h = new_http_header(kgl_expand_string("Accept-Encoding"), kgl_expand_string("gzip"));
		h->next = ss->rh;
		ss->rh = h;
		SET(rq->raw_url.encoding, KGL_ENCODING_GZIP);
	}
	rq->meth = KHttpKeyValue::getMethod(ctx->meth);
	rq->content_length = ctx->postLen;
	rq->http_major = 1;
	rq->http_minor = 1;
	SET(rq->flags,RQ_CONNECTION_CLOSE);
	
	if (!TEST(ctx->flags, KF_SIMULATE_CACHE)) {
		SET(rq->flags, RQ_HAS_NO_CACHE);
		SET(rq->filter_flags, RF_NO_CACHE);
	}
	if (rq->content_length>0) {
		SET(rq->flags,RQ_HAS_CONTENT_LEN);
	}
	ss->startTime = kgl_current_msec;	
#ifndef _WIN32
#ifndef NDEBUG
	ss->setnoblock();
#endif
#endif
	if (TEST(ctx->flags,KF_SIMULATE_LOCAL)) {
		rq->c->ls = conf.gvm->refsServer(rq->raw_url.port);
		if (rq->c->ls==NULL) {
			ss->body = NULL;
			delete rq;
			return 2;
		}
		KHttpHeader *h = ss->rh;
		while (h) {
			int val_len = h->val_len;
			rq->parseHeader(h->attr,h->val,val_len,false);
			h = h->next;
		}
		handleStartRequest(rq,0);
	} else {
		rq->beginRequest();
		rq->fetchObj = new KHttpProxyFetchObject();
		SET(rq->workModel,WORK_MODEL_SKIP_ACCESS);
		async_http_start(rq);
	}	
	return 0;
}
int WINAPI test_header_hook(void *arg,int code,KHttpHeader *header)
{
	return 0;
}
int WINAPI test_body_hook(void *arg,const char *data,int len)
{
	//printf("len = %d\n",len);
	if (data) {
		//fwrite(data, 1, len, stdout);
	}
	return 0;
}
int WINAPI test_post_hook(void *arg,char *buf,int len)
{
	memcpy(buf,"test",4);
	return 4;
}
static void WINAPI timer_simulate(void *arg)
{
	test_simulate_request();
}
bool test_simulate_request()
{
	return true;
	timer_run(timer_simulate, NULL, 2000, 0);
	kgl_async_http ctx;
	memset(&ctx,0,sizeof(ctx));
	ctx.url = "http://test.monitor.dnsdun.com:1112/monitor?a=status_all&name=test&version=2.2";
	ctx.meth = "get";
	ctx.postLen = 0;
	//ctx.header = test_header_hook;
	ctx.flags = KF_SIMULATE_DELTA|KF_SIMULATE_GZIP;
	ctx.body = test_body_hook;
	ctx.arg = NULL;
	ctx.rh = NULL;
	ctx.post = test_post_hook;
	asyncHttpRequest(&ctx);
	//asyncHttpRequest(METH_GET,"http://www.kangleweb.net/test.php",NULL,test_header_hook,test_body_hook,NULL);
	return true;
}
int KSimulateSocket::sendError(int code,const char *msg)
{
	if (header) {
		KHttpHeader head;
		head.next = NULL;
		head.attr = (char *)"msg";
		head.val = (char *)msg;
		header(arg,code+10000,&head);
	}
	return 0;
}
KSimulateSocket::KSimulateSocket()
{
	host = NULL;
	rh = NULL;
	body = NULL;
}
KSimulateSocket::~KSimulateSocket()
{
	if (host) {
		xfree(host);
	}
	if (body) {
		body(arg,NULL,(int)(kgl_current_msec - startTime));
	}
	free_header(rh);
}
int KSimulateSocket::sendHeader(int code,KHttpHeader *header)
{
	if (this->header) {
		return this->header(arg,code,header);
	}
	return 0;
}
int KSimulateSocket::sendBody(const char *buf,int len)
{
	if (this->body) {
		return this->body(arg,buf,len);
	}
	return 0;
}
#endif

