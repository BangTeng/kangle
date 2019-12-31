/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <errno.h>
#include "kmalloc.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "kthread.h"
#include "lib.h"
#include "http.h"
#include "KVirtualHost.h"
#include "kselector.h"
#include "KFilterHelper.h"
#include "KHttpKeyValue.h"
#include "KHttpField.h"
#include "KHttpFieldValue.h"
#include "KHttpBasicAuth.h"
#include "KHttpDigestAuth.h"

#include "time_utils.h"
#include "kselector_manager.h"
#include "KFilterContext.h"
#include "KUrlParser.h"
#include "KHttpFilterManage.h"
#include "KHttpFilterContext.h"
#include "KBufferFetchObject.h"
#include "KHttpTransfer.h"
#include "KVirtualHostManage.h"
#include "kselectable.h"
#include "KStaticFetchObject.h"
using namespace std;
void WINAPI free_auto_memory(void *arg)
{
	xfree(arg);
}
kev_result result_write_expected(void *arg, int got)
{
	kgl_request_event_context *stack = (kgl_request_event_context *)arg;
	return stack->rq->WriteExpectedCallback(stack->ev.arg, stack->ev.result, stack->ev.buffer);
}
#ifdef ENABLE_TF_EXCHANGE
kev_result stageTempFileWriteEnd(KHttpRequest *rq)
{
	delete rq->tf;
	rq->tf = NULL;
	return stageEndRequest(rq);
}
int bufferTempFileRequestWrite(void *arg, LPWSABUF buf, int bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	int len;
	buf[0].iov_base = (char *)rq->tf->readBuffer(len);
	buf[0].iov_len = len;
	return 1;
}
kev_result resultTempFileRequestWrite(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got<=0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return stageTempFileWriteEnd(rq);
	}
	rq->AddDownFlow(got);
	if (rq->tf->readSuccess(got)) {
		return rq->Write(rq, resultTempFileRequestWrite, bufferTempFileRequestWrite);
	}
	return stageTempFileWriteEnd(rq);
}
kev_result startTempFileWriteRequest(KHttpRequest *rq)
{
	kassert(rq->tf);
	return rq->Write(rq, resultTempFileRequestWrite, bufferTempFileRequestWrite);
}
#endif
kev_result stageWriteRequest(KHttpRequest *rq,kbuf *buf,int start,int len)
{
	kassert(!TEST(rq->flags, RQ_SYNC));
	kassert(rq->fetchObj == NULL);
	if (rq->fetchObj) {
		rq->closeFetchObject();
	}
	if (buf != NULL) {
		rq->fetchObj = new KBufferFetchObject(buf, start, len, rq->pool);
		return rq->fetchObj->open(rq);
	}
	return stageEndRequest(rq);
}
kev_result stageWriteRequest(KHttpRequest *rq, KAutoBuffer *buffer)
{
	if (unlikely(rq->IsSync())) {
		if (buffer) {
			if (rq->WriteBuff(buffer->getHead())) {
				return kev_ok;
			}
		}
		return kev_err;
	}
	if (unlikely(buffer == NULL)) {
		return stageEndRequest(rq);
	}
	return stageWriteRequest(rq, buffer->getHead(), 0, buffer->getLen());
}

void KHttpRequest::setState(uint8_t state) {
#ifdef ENABLE_STAT_STUB
	if (this->state == state) {
		return;
	}
	switch (this->state) {
	case STATE_IDLE:
	case STATE_QUEUE:
		katom_dec((void *)&kgl_waiting);
		break;
	case STATE_RECV:
		katom_dec((void *)&kgl_reading);
		break;
	case STATE_SEND:
		katom_dec((void *)&kgl_writing);
		break;
	}
#endif
	this->state = state;
#ifdef ENABLE_STAT_STUB
	switch (state) {
	case STATE_IDLE:
	case STATE_QUEUE:
		katom_inc((void *)&kgl_waiting);
		break;
	case STATE_RECV:
		katom_inc((void *)&kgl_reading);
		break;
	case STATE_SEND:
		katom_inc((void *)&kgl_writing);
		break;
	}
#endif		
}
void KHttpRequest::SetSelfPort(uint16_t port, bool ssl)
{
	if (port > 0) {
		self_port = port;
	}
	if (ssl) {
		SET(raw_url.flags, KGL_URL_SSL);
		if (raw_url.port == 80) {
			raw_url.port = 443;
		}
	} else {
		CLR(raw_url.flags, KGL_URL_SSL);
		if (raw_url.port == 443) {
			raw_url.port = 80;
		}
	}
}
const char *KHttpRequest::getState()
{
	
	switch (state) {
	case STATE_IDLE:
		return "idle";
	case STATE_SEND:
		return "send";
	case STATE_RECV:
		return "recv";
	case STATE_QUEUE:
		return "queue";
	}
	return "unknow";
	
}
void KHttpRequest::resetFetchObject()
{
	if (fetchObj) {
		KFetchObject *fo = fetchObj->clone(this);
		fo->next = fetchObj->next;
		fetchObj->close(this);
		fetchObj = fo;
	}
}
void KHttpRequest::closeFetchObject(bool destroy)
{
	KFetchObject *fo = fetchObj;
	while (fo) {
		KFetchObject *next = fo->next;
		fo->close(this);
		if (destroy) {
			delete fo;
		}
		fo = next;
	}
	if (destroy) {
		fetchObj = NULL;
	}
#ifdef ENABLE_REQUEST_QUEUE
	ReleaseQueue();
#endif
}
#ifdef ENABLE_REQUEST_QUEUE
void KHttpRequest::ReleaseQueue()
{
	if (queue) {
		if (ctx->queue_handled) {
			ctx->queue_handled = false;
			async_queue_destroy(queue);
		}
		queue->release();
		queue = NULL;
	}
	kassert(!ctx->queue_handled);
}
#endif
void KHttpRequest::set_url_param(char *param) {
	
	url->param = xstrdup(param);

}
void KHttpRequest::endRequest() {
	sink->EndRequest(this);
}
void KHttpRequest::beginRequest() {
#ifdef ENABLE_STAT_STUB
	katom_inc64((void *)&kgl_total_requests);
#endif
	setState(STATE_RECV);
	assert(url==NULL);
	mark = 0;
	url = new KUrl;
	if(raw_url.host){
		url->host = xstrdup(raw_url.host);
	}
	if (raw_url.param) {
		set_url_param(raw_url.param);
	}
	url->flags = raw_url.flags;
	url->port = raw_url.port;
	if(raw_url.path){
		url->path = xstrdup(raw_url.path);
		url_decode(url->path,0,url,false);
		KFileName::tripDir3(url->path, '/');
	}
	if (auth) {
		if (!auth->verifySession(this)) {
			delete auth;
			auth = NULL;
		}
	}
	if (TEST(flags,RQ_INPUT_CHUNKED)) {
		left_read = -1;
	} else {
		left_read = content_length;
	}	
	begin_time_msec = kgl_current_msec;
#ifdef MALLOCDEBUG
	if (quit_program_flag!=PROGRAM_NO_QUIT) {
		SET(flags,RQ_CONNECTION_CLOSE);
	}
#endif
}
const char *KHttpRequest::getMethod() {
	return KHttpKeyValue::getMethod(meth);
}
bool KHttpRequest::isBad() {
	if (unlikely(url==NULL || url->IsBad() || meth == METH_UNSET)) {
		return true;
	}
	return false;
}
void KHttpRequest::FreeLazyMemory() {
	if (client_ip) {
		free(client_ip);
		client_ip = NULL;
	}
	raw_url.destroy();
	free_header(header);
	header = last = NULL;
	mark = 0;
}

bool KHttpRequest::rewriteUrl(const char *newUrl, int errorCode, const char *prefix) {
	KUrl url2;
	if (!parse_url(newUrl, &url2)) {
		KStringBuf nu;
		if (prefix) {
			if (*prefix!='/') {
				nu << "/";
			}
			nu << prefix;
			int len = (int)strlen(prefix);
			if (len>0 && prefix[len-1]!='/') {
				nu << "/";
			}
			nu << newUrl;
		} else {
			char *basepath = strdup(url->path);
			char *p = strrchr(basepath,'/');
			if (p) {
				p++;
				*p = '\0';
			}
			nu << basepath;			
			free(basepath);		
			nu << newUrl;
		}	
		if(!parse_url(nu.getString(),&url2)){
			url2.destroy();
			return false;
		}
	}
	if (url2.path == NULL) {
		url2.destroy();
		return false;
	}
	if (TEST(url2.flags, KGL_URL_ORIG_SSL)) {
		SET(url2.flags, KGL_URL_SSL);
	}
	KStringBuf s;
	if (errorCode > 0) {
		s << errorCode << ";";
		if (TEST(url->flags, KGL_URL_SSL)) {
			s << "https";
		} else {
			s << "http";
		}
		s << "://" << url->host	<< ":" << url->port << url->path;
		if (url->param) {
			s << "?" << url->param;
		}
	}
	if (url2.host == NULL) {
		url2.host = strdup(url->host);
	}
	url_decode(url2.path, 0, &url2);
	if (ctx->obj && ctx->obj->url==url && !TEST(ctx->obj->index.flags,FLAG_URL_FREE)) {
		SET(ctx->obj->index.flags,FLAG_URL_FREE);
		ctx->obj->url = url->clone();
	}
	url->destroy();
	url->host = url2.host;
	url->path = url2.path;
	if (errorCode > 0) {
		url->param = s.stealString();
		if (url2.param) {
			xfree(url2.param);
			url2.param = NULL;
		}
	} else {
		url->param = url2.param;
	}
	if (url2.port > 0) {
		url->port = url2.port;
	}
	if (url2.flags > 0) {
		url->flags = url2.flags;
	}
	SET(raw_url.flags,KGL_URL_REWRITED);
	return true;
}
char *KHttpRequest::getUrl() {
	if (url==NULL) {
		return strdup("");
	}
	return url->getUrl();
}
std::string KHttpRequest::getInfo() {
	KStringBuf s;

	raw_url.getUrl(s,true);

	return s.getString();
}
void KHttpRequest::init(kgl_pool_t *pool) {
	KHttpRequestData *data = static_cast<KHttpRequestData *>(this);
	memset(data, 0, sizeof(KHttpRequestData));
	InitPool(pool);
	begin_time_msec = kgl_current_msec;
	setState(STATE_IDLE);
}
KHttpRequest::~KHttpRequest()
{
	//printf("~KHttpRequest=[%p].\n", this);
	clean();
#ifdef ENABLE_REQUEST_QUEUE
	assert(queue == NULL);
#endif
	releaseVirtualHost();
	FreeLazyMemory();
	delete ctx;
	delete sink;
	setState(STATE_UNKNOW);
}
void KHttpRequest::clean() {
	kassert(write_stack == NULL || (write_stack->hook_head == NULL && write_stack->hook_last==NULL));
	closeFetchObject();
	if (file){
		delete file;
		file = NULL;
	}
	assert(ctx);
	ctx->clean();
	//tr由ctx->st负责删除
	tr = NULL;
	if (of_ctx) {
		delete of_ctx;
		of_ctx = NULL;
	}
#ifdef ENABLE_INPUT_FILTER
	if (if_ctx) {
		delete if_ctx;
		if_ctx = NULL;
	}
#endif

	if (url) {
		url->destroy();
		delete url;
		url = NULL;
	}
	while (slh) {
		KSpeedLimitHelper *slh_next = slh->next;
		delete slh;
		slh = slh_next;
	}
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		delete tf;
		tf = NULL;
	}
#endif
	if (bind_ip) {
		free(bind_ip);
		bind_ip = NULL;
	}
	while (fh) {
		KFlowInfoHelper *fh_next = fh->next;
		delete fh;
		fh = fh_next;
	}
	if (auth) {
		delete auth;
		auth = NULL;
	}
	if (pool) {
		kgl_destroy_pool(pool);
		pool = NULL;
	}
}
void KHttpRequest::releaseVirtualHost()
{
	if (svh) {
		svh->release();
		svh = NULL;
	}
}
bool KHttpRequest::parseMeth(const char *src) {
	meth = KHttpKeyValue::getMethod(src);
	if (meth >= 0) {
		return true;
	}
	return false;
}
bool KHttpRequest::parseConnectUrl(char *src) {
	char *ss;
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	CLR(raw_url.flags, KGL_URL_ORIG_SSL);
	*ss = 0;
	raw_url.host = strdup(src);
	raw_url.port = atoi(ss + 1);
	return true;
}
kgl_header_result KHttpRequest::parseHost(char *val)
{
	if (raw_url.host == NULL) {
		char *p = NULL ;
		if(*val == '['){
			SET(raw_url.flags,KGL_URL_IPV6);
			val++;
			raw_url.host = strdup(val);
			p = strchr(raw_url.host,']');
			if(p){
				*p = '\0';
				p = strchr(p+1,':');
			}
		}else{
			raw_url.host = strdup(val);
			p = strchr(raw_url.host, ':');
			if(p){
				*p = '\0';
			}
		}
		if (p) {
			raw_url.port = atoi(p+1);
		} else {
			if (TEST(raw_url.flags,KGL_URL_SSL)) {
				raw_url.port = 443;
			} else {
				raw_url.port = 80;
			}
		}
	}
	return kgl_header_no_insert;
}
bool KHttpRequest::WriteBuff(kbuf *buf)
{
	while (buf) {
		if (!WriteAll(buf->data, buf->used)) {
			return false;
		}
		buf = buf->next;
	}
	return true;
}
int KHttpRequest::Write(const char *buf, int len)
{
	int sleep_msec = getSleepTime(len);
	if (sleep_msec > 0) {
		my_msleep(sleep_msec);
	}
	return sink->Write(buf, len);
}
bool KHttpRequest::WriteAll(const char *buf, int len)
{	
	while (len > 0) {
		int this_len = Write(buf, len);
		if (this_len <= 0) {
			return false;
		}
		send_size += len;
		len -= this_len;
		buf += this_len;
	}
	return true;
}
void KHttpRequest::startParse() {
	FreeLazyMemory();
	if (TEST(sink->GetBindServer()->flags, WORK_MODEL_SSL)) {
		SET(raw_url.flags, KGL_URL_SSL | KGL_URL_ORIG_SSL);
	}
	meth = METH_UNSET;
}
kgl_header_result KHttpRequest::InternalParseHeader(const char *attr, int attr_len, char *val,int *val_len, bool is_first)
{
#ifdef ENABLE_HTTP2
	if (this->http_major>1 && *attr==':') {
		attr++;
		if (strcmp(attr, "method") == 0) {
			if (!parseMeth(val)) {
				klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
				return kgl_header_failed;
			}
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "version") == 0) {
			parseHttpVersion(val);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "path") == 0) {
			parse_url(val, &raw_url);
			return kgl_header_no_insert;
		}
		if (strcmp(attr, "authority") == 0) {
			if (kgl_header_success == parseHost(val)) {
				//转换成HTTP/1的http头
				AddHeader(kgl_expand_string("Host"), val, *val_len, true);
			}
			return kgl_header_no_insert;
		}
		return kgl_header_no_insert;
	}
#endif
	if (is_first && http_major<=1) {
		if (!parseMeth(attr)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
			return kgl_header_failed;
		}
		char *space = strchr(val, ' ');
		if (space == NULL) {
			klog(KLOG_DEBUG,"httpparse:cann't get space seperator to parse HTTP/1.1 [%s]\n",val);
			return kgl_header_failed;
		}
		*space = 0;
		space++;

		while (*space && IS_SPACE(*space)) {
			space++;
		}
		bool result;
		if (meth == METH_CONNECT) {
			result = parseConnectUrl(val);
		} else {
			result = parse_url(val, &raw_url);
		}
		if (!result) {
			klog(KLOG_DEBUG, "httpparse:cann't parse url [%s]\n", val);
			return kgl_header_failed;
		}
		if (!parseHttpVersion(space)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n", space);
			return kgl_header_failed;
		}
		if (http_major > 1 || (http_major == 1 && http_minor == 1)) {
			SET(flags, RQ_HAS_KEEP_CONNECTION);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Host")) {
		return parseHost(val);
	}
	if (!strcasecmp(attr, "Connection")
		
		) {
		KHttpFieldValue field(val);
		do {
			if (field.is2("keep-alive", 10)) {
				flags |= RQ_HAS_KEEP_CONNECTION;
			} else if (field.is2("upgrade", 7)) {
				flags |= RQ_HAS_CONNECTION_UPGRADE;
			} else if (field.is2(kgl_expand_string("close"))) {
				CLR(flags, RQ_HAS_KEEP_CONNECTION);
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Accept-Encoding")) {
		if (!*val) {
			return kgl_header_no_insert;
		}
		KHttpFieldValue field(val);
		do {
			if (field.is2(kgl_expand_string("gzip"))) {
				SET(raw_url.encoding, KGL_ENCODING_GZIP);
			} else if (field.is2(kgl_expand_string("deflate"))) {
				SET(raw_url.encoding, KGL_ENCODING_DEFLATE);
			} else if (field.is2(kgl_expand_string("compress"))) {
				SET(raw_url.encoding, KGL_ENCODING_COMPRESS);
			} else if (field.is2(kgl_expand_string("br"))) {
				SET(raw_url.encoding, KGL_ENCODING_BR);
			} else if (!field.is2(kgl_expand_string("identity"))) {
				SET(raw_url.encoding, KGL_ENCODING_UNKNOW);
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "If-Range")) {
		time_t try_time = parse1123time(val);
		if (try_time == -1) {
			flags |= RQ_IF_RANGE_ETAG;
			if (this->ctx->if_none_match == NULL) {
				ctx->set_if_none_match(val, *val_len);
			}
		} else {
			if_modified_since = try_time;
			flags |= RQ_IF_RANGE_DATE;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-Modified-Since")) {
		if_modified_since = parse1123time(val);
		flags |= RQ_HAS_IF_MOD_SINCE;
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "If-None-Match")) {
		flags |= RQ_HAS_IF_NONE_MATCH;
		if (this->ctx->if_none_match == NULL) {
			ctx->set_if_none_match(val, *val_len);
		}
		return kgl_header_no_insert;
	}
	//	printf("attr=[%s],val=[%s]\n",attr,val);
	if (!strcasecmp(attr, "Content-length")) {
		content_length = string2int(val);
		left_read = content_length;
		flags |= RQ_HAS_CONTENT_LEN;
		return kgl_header_no_insert;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			SET(flags, RQ_INPUT_CHUNKED);
			content_length = -1;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Expect")) {
		if (strstr(val, "100-continue") != NULL) {
			flags |= RQ_HAVE_EXPECT;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "X-Forwarded-Proto")) {
		if (strcasecmp(val, "https") == 0) {
			SET(raw_url.flags, KGL_URL_ORIG_SSL);
		} else {
			CLR(raw_url.flags, KGL_URL_ORIG_SSL);
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Pragma")) {
		if (strstr(val, "no-cache"))
			flags |= RQ_HAS_NO_CACHE;
		return kgl_header_success;
	}
	if (
		
		!strcasecmp(attr, AUTH_REQUEST_HEADER)) {
		
		flags |= RQ_HAS_AUTHORIZATION;
		
#ifdef ENABLE_TPROXY
		if (!TEST(GetWorkModel(),WORK_MODEL_TPROXY)) {
#endif
			char *p = val;
			while (*p && !IS_SPACE(*p)) {
				p++;
			}
			char *p2 = p;
			while (*p2 && IS_SPACE(*p2)) {
				p2++;
			}
			KHttpAuth *tauth = NULL;
			if (strncasecmp(val, "basic", p - val) == 0) {
				tauth = new KHttpBasicAuth;
			} else if (strncasecmp(val, "digest", p - val) == 0) {
#ifdef ENABLE_DIGEST_AUTH
				tauth = new KHttpDigestAuth;
#endif
			}
			if (tauth) {
				if (!tauth->parse(this, p2)) {
					delete tauth;
					tauth = NULL;
				}
			}
			if (auth) {
				delete auth;
			}
			auth = tauth;
			
#ifdef ENABLE_TPROXY
		}
#endif
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Cache-Control")) {
		KHttpFieldValue field(val);
		do {
			if (field.is("no-store") || field.is("no-cache")) {
				flags |= RQ_HAS_NO_CACHE;
			} else if (field.is("only-if-cached")) {
				flags |= RQ_HAS_ONLY_IF_CACHED;
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Range")) {
		if (!strncasecmp(val, "bytes=", 6)) {
			val += 6;
			range_from = -1;
			range_to = -1;
			if (*val != '-') {
				range_from = string2int(val);
			}
			char *p = strchr(val, '-');
			if (p && *(p + 1)) {
				range_to = string2int(p + 1);
			}
			char *next_range = strchr(val, ',');
			if (next_range) {
				//we do not support multi range
				klog(KLOG_INFO, "cut multi_range %s\n", val);
				//SET(filter_flags,RF_NO_CACHE);
				*next_range = '\0';
			}
		}
		flags |= RQ_HAVE_RANGE;
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Content-Type")) {
#ifdef ENABLE_INPUT_FILTER
		if (if_ctx == NULL) {
			if_ctx = new KInputFilterContext(this);
		}
#endif
		if (strncasecmp(val, "multipart/form-data", 19) == 0) {
			SET(flags, RQ_POST_UPLOAD);
#ifdef ENABLE_INPUT_FILTER
			if_ctx->parseBoundary(val + 19);
#endif
		}
		return kgl_header_success;
	}
	return kgl_header_success;
}
int KHttpRequest::Read(char *buf, int len)
{
	kassert(this->IsSync());
	if (TEST(flags, RQ_HAVE_EXPECT)) {
		CLR(flags, RQ_HAVE_EXPECT);
		sink->ResponseStatus(100);
		sink->StartResponseBody(this, 0);
		sink->Flush();
		sink->StartHeader(this);
	}
	int length;
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		length = tf->readBuffer(buf, len);
	} else
#endif
	length = sink->Read(buf,len);
	if (length > 0) {
		left_read -= length;
		AddUpFlow((INT64)length);
	}
	return length;
}
kev_result KHttpRequest::WriteExpectedCallback(void *arg, result_callback result, buffer_callback buffer)
{
	kassert(!TEST(flags, RQ_HAVE_EXPECT));
	sink->Flush();
	sink->StartHeader(this);
	return sink->Read(arg, result, buffer);
}
kev_result KHttpRequest::LowRead(void *arg, result_callback result, buffer_callback buffer)
{
	kassert(!IsSync());
	//check expected 100-continue
	if (TEST(flags, RQ_HAVE_EXPECT) && !ctx->connection_upgrade) {
		CLR(flags, RQ_HAVE_EXPECT);
		sink->ResponseStatus(100);
		sink->StartResponseBody(this, 0);
		if (sink->HasHeaderDataToSend()) {
			kgl_request_event_context *ctx = (kgl_request_event_context *)kgl_pnalloc(pool, sizeof(kgl_request_event_context));
			ctx->ev.arg = arg;
			ctx->ev.result = result;
			ctx->ev.buffer = buffer;
			ctx->rq = this;
			return sink->Write(ctx, result_write_expected, NULL);
		}
	}
	return sink->Read(arg, result, buffer);
}
kev_result KHttpRequest::Read(void *arg, result_callback result, buffer_callback buffer)
{
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		WSABUF buf;
		int bc = buffer(arg, &buf, 1);
		kassert(bc == 1);
		int got = tf->readBuffer((char *)buf.iov_base, buf.iov_len);
		return result(arg, got);
	}
#endif
	return LowRead(arg, result, buffer);
}

int buffer_limit_write(void *arg, LPWSABUF buf, int buf_count)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	buffer_callback buffer = (buffer_callback)rq->GetCurrentWriteHookContext();
	return buffer(rq->write_stack->arg, buf, 1);	
}
kev_result result_limit_write_done(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	return rq->WriteHookCallBack();
}
kev_result speed_limit_write_hook(KHttpRequest *rq)
{
	if (rq->write_stack->got <= 0) {
		return rq->WriteHookCallBack();
	}
	int speed_msec = rq->getSleepTime(rq->write_stack->got);
	if (speed_msec > 0) {
		kgl_block_queue *brq = (kgl_block_queue *)xmemory_new(kgl_block_queue);
		memset(brq, 0, sizeof(kgl_block_queue));
		brq->active_msec = kgl_current_msec + speed_msec;
		brq->func = result_limit_write_done;
		brq->arg = rq;
		brq->got = 0;
		kselector_add_block_queue(rq->sink->GetSelector(), brq);
		return kev_ok;
	}
	return rq->WriteHookCallBack();
}
kev_result kgl_call_write_hook(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	kassert(rq->write_stack);
	rq->write_stack->got = got;
	rq->ctx->write_hook = 1;
	return rq->write_stack->hook_head->call(rq);
}
void KHttpRequest::AddWriteHook(void *arg, KHttpRequestWriteHook result,bool last)
{
	kassert(!IsSync());
	if (write_stack == NULL) {
		write_stack = (kgl_request_stack *)kgl_pnalloc(pool, sizeof(kgl_request_stack));
		memset(write_stack, 0, sizeof(kgl_request_stack));
	}
	kgl_write_hook *write_hook = new kgl_write_hook;
	write_hook->call = result;
	write_hook->arg = arg;
	if (last) {
		write_hook->next = NULL;
		if (write_stack->hook_last) {
			write_stack->hook_last->next = write_hook;
		} else {
			kassert(write_stack->hook_head == NULL);
			write_stack->hook_head = write_hook;
		}
		write_stack->hook_last = write_hook;
	} else {
		write_hook->next = write_stack->hook_head;
		write_stack->hook_head = write_hook;
		if (write_stack->hook_last == NULL) {
			write_stack->hook_last = write_hook;
		}
	}
}
kev_result KHttpRequest::WriteHookCallBack()
{
	kassert(write_stack && write_stack->hook_head);
	kgl_write_hook *hook = write_stack->hook_head;
	kassert(hook->next || hook == write_stack->hook_last);
	write_stack->hook_head = hook->next;
	delete hook;
	if (write_stack->hook_head) {
		return write_stack->hook_head->call(this);
	}
	write_stack->hook_last = NULL;
	kassert(write_stack->result);
	ctx->write_hook = 0;
	return write_stack->result(write_stack->arg, write_stack->got);
}
kev_result KHttpRequest::Write(void *arg, result_callback result, buffer_callback buffer)
{
	if (unlikely(slh && buffer)) {
		AddWriteHook((void *)buffer, speed_limit_write_hook,false);
		kassert(this->GetCurrentWriteHookContext() == (void *)buffer);
		buffer = buffer_limit_write;
	}
	if (HasWriteHook()) {
		write_stack->arg = arg;
		write_stack->result = result;
		return sink->Write(this, kgl_call_write_hook, buffer);
	}
	return sink->Write(arg, result, buffer);
}
bool KHttpRequest::ParseHeader(const char *attr, int attr_len,char *val,int val_len, bool is_first)
{
	if (is_first) {
		startParse();
	}
	//printf("attr=[%s] val=[%s]\n", attr, val);
	kgl_header_result ret = InternalParseHeader(attr, attr_len, val,&val_len, is_first);
	switch (ret) {
	case kgl_header_failed:
		return false;
	case kgl_header_insert_begin:
		return AddHeader(attr, attr_len, val, val_len, false);
	case kgl_header_success:
		return AddHeader(attr, attr_len, val, val_len, true);
	default:
		return true;
	}
}
bool KHttpRequest::has_post_data() {
	return left_read>0;
}
bool KHttpRequest::parseHttpVersion(char *ver) {
	char *dot = strchr(ver,'.');
	if (dot==NULL) {
		return false;
	}
	http_major = *(dot - 1) - 0x30;//major;
	http_minor = *(dot + 1) - 0x30;//minor;
	return true;
}

int KHttpRequest::checkFilter(KHttpObject *obj) {
	int action = JUMP_ALLOW;
	if (TEST(obj->index.flags,FLAG_NO_BODY)) {
		return action;
	}
	if (of_ctx) {
		if(of_ctx->charset == NULL){
			of_ctx->charset = obj->getCharset();
		}
		kbuf *bodys = obj->data->bodys;
		kbuf *head = bodys;
		while (head && head->used > 0) {
			action = of_ctx->checkFilter(this,head->data, head->used);
			if (action == JUMP_DENY) {
				break;
			}
			head = head->next;
		}
	}
	return action;
}
void KHttpRequest::addFilter(KFilterHelper *chain) {
	getOutputFilterContext()->addFilter(chain);
}
KOutputFilterContext *KHttpRequest::getOutputFilterContext()
{
	if (of_ctx==NULL) {
		of_ctx = new KOutputFilterContext;
	}
	return of_ctx;
}

bool KHttpRequest::responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len)
{
	return sink->ResponseHeader(name, name_len, val, val_len);
}
bool KHttpRequest::startResponseBody(INT64 body_len)
{
	assert(!TEST(flags,RQ_HAS_SEND_HEADER));
	if (TEST(flags,RQ_HAS_SEND_HEADER)) {
		return true;
	}
	SET(flags,RQ_HAS_SEND_HEADER);
	int header_len = sink->StartResponseBody(this, body_len);
	AddDownFlow(header_len,true);
	return header_len > 0;
}

void KHttpRequest::pushFetchObject(KFetchObject *fo)
{
	fo->next = fetchObj;
	fetchObj = fo;
}
void KHttpRequest::appendFetchObject(KFetchObject *fo)
{
	if (fo->filter==0 && TEST(filter_flags, RQ_NO_EXTEND) && !TEST(flags, RQ_IS_ERROR_PAGE)) {
		//无扩展处理
		if (fo->needQueue()) {
			delete fo;
			fo = new KStaticFetchObject();
		}
	}
	KFetchObject *last = fetchObj;
	while (last && last->next) {
		last = last->next;
	}
	if (last) {
		last->next = fo;
		return;
	}
	fetchObj = fo;
}
bool KHttpRequest::hasFinalFetchObject()
{
	KFetchObject *fo = this->fetchObj;
	while (fo) {
		if (!fo->filter) {
			return true;
		}
		fo = fo->next;
	}
	return false;
}
kev_result KHttpRequest::NextFetchObject(KRequestQueue *queue)
{
#ifdef ENABLE_REQUEST_QUEUE
	if (queue) {
		ReleaseQueue();
		this->queue = queue;
		queue->addRef();
	}
#endif
	KFetchObject *fo = fetchObj;
	if (fetchObj) {
		fetchObj = fetchObj->next;
	}
	if (fo != NULL) {
		fo->close(this);
		delete fo;
	}
	if (fetchObj) {
		if (this->queue && !ctx->queue_handled) {
			return processRequestUseQueue(this, queue);
		}
		return processReadyedRequest(this);
	}
	return stageEndRequest(this);
}
