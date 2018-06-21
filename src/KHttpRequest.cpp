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
#include "malloc_debug.h"
#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KThreadPool.h"
#include "lib.h"
#include "http.h"
#include "KVirtualHost.h"
#include "KSelector.h"
#include "KFilterHelper.h"
#include "KHttpKeyValue.h"
#include "KHttpField.h"
#include "KHttpFieldValue.h"
#include "KHttpBasicAuth.h"
#include "KHttpDigestAuth.h"

#include "time_utils.h"
#include "KSelectorManager.h"
#include "KFilterContext.h"
#include "KSubRequest.h"
#include "KUrlParser.h"
#include "KHttpFilterManage.h"
#include "KHttpFilterContext.h"
using namespace std;
void WINAPI free_auto_memory(void *arg)
{
	xfree(arg);
}
void resultNextSubRequest(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	async_http_start(rq);
}
#ifdef ENABLE_TF_EXCHANGE
void stageTempFileWriteEnd(KHttpRequest *rq)
{
	delete rq->tf;
	rq->tf = NULL;
	stageEndRequest(rq);
}
void resultTempFileRequestWrite(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got<=0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		stageTempFileWriteEnd(rq);
		return;
	}
	rq->addFlow(got, rq->ctx->cache_hit );
	rq->send_size += got;
	if (rq->tf->readSuccess(got)) {
		startTempFileWriteRequest(rq);
		return;
	}
	stageTempFileWriteEnd(rq);
}
void bufferTempFileRequestWrite(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	bufCount = 1;
	int len;
	buf[0].iov_base = (char *)rq->tf->readBuffer(len);
	buf[0].iov_len = len;
}
void startTempFileWriteRequest(KHttpRequest *rq)
{
	assert(rq->tf);
	if (rq->slh != NULL) {
		int len = 0;
		rq->tf->readBuffer(len);
		int sendTime = rq->getSleepTime(len);
		if (sendTime > 0) {
			rq->c->delayWrite(rq, resultTempFileRequestWrite, bufferTempFileRequestWrite, sendTime);
			return;
		}
	}
	rq->c->write(rq,resultTempFileRequestWrite,bufferTempFileRequestWrite);
}
#endif
void bufferRequestWrite(void *arg,LPWSABUF buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	rq->get_write_buf(buf,bufCount);
}

void resultRequestWrite(void *arg,int got);

void startWriteRequest(KHttpRequest *rq)
{	
	if (rq->slh != NULL) {
		int len = 0;
		rq->get_write_buf(len);
		int sendTime = rq->getSleepTime(len);
		if (sendTime > 0) {
			rq->c->delayWrite(rq, resultRequestWrite, bufferRequestWrite, sendTime);
			return;
		}
	}
	rq->c->write(rq,resultRequestWrite,bufferRequestWrite);
}
void resultRequestWrite(void *arg,int got)
{

	KHttpRequest *rq = (KHttpRequest *)arg;
	switch(rq->canWrite(got)){
		case WRITE_FAILED:
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			rq->buffer.clean();
			stageEndRequest(rq);
			break;
		case WRITE_SUCCESS:
			rq->buffer.clean();
			if (rq->fetchObj && !rq->fetchObj->isClosed()) {
				//
				
				rq->fetchObj->readBody(rq);
			} else {
				stageEndRequest(rq);
			}
			break;
		default:
			startWriteRequest(rq);
	}
}
void bufferRequestRead(void *arg,iovec *buf,int &bufCount)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	int len;
	buf[0].iov_base = rq->get_read_buf(len);
	buf[0].iov_len = len;
	bufCount = 1;
}
void resultRequestRead(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	switch(rq->canRead(got)){
	case READ_FAILED:
		delete rq;
		break;
	case READ_SUCCESS:
		handleStartRequest(rq,0);
		break;
	default:
		rq->c->read(rq,resultRequestRead,bufferRequestRead);
	}
}

void stageWriteRequest(KHttpRequest *rq,buff *buf,int start,int len)
{
	rq->send_ctx.body_append(buf,(int)start,(int)len);
	if (rq->send_ctx.getBufferSize()==0) {
#ifdef ENABLE_HTTP2
		if (rq->http2_ctx) {
			if (rq->fetchObj && !rq->fetchObj->isClosed()) {
				//
				rq->fetchObj->readBody(rq);
				return;
			}
		}
#endif
		
		stageEndRequest(rq);
	} else {
		startWriteRequest(rq);
	}
}
void stageWriteRequest(KHttpRequest *rq)
{
	if (TEST(rq->flags,RQ_SYNC)) {
		if (rq->send_ctx.getBufferSize()>0) {
			if (!rq->sync_send_header()) {
				return;
			}
			rq->send_ctx.clean();
		}
		if (rq->buffer.getLen()>0) {
			rq->sync_send_buffer();
		}
		return;
	}
	stageWriteRequest(rq,rq->buffer.getHead(),0,rq->buffer.getLen());
}
int KHttpRequest::read(char *buf, int len) {
	if (left_read <= 0) {
		return 0;
	}
	len = (int)MIN((INT64)len,left_read);
#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		len = tf->readBuffer(buf,len);
	} else {
#endif

		len = c->read(this,buf, len);
#ifdef ENABLE_INPUT_FILTER
		if (len>0) {
			if (if_ctx && JUMP_DENY==if_ctx->check(buf,len,left_read<=len)) {
				char *u = url->getUrl();
				klog(KLOG_ERR,"access denied by input filter. ip=%s url=%s\n",getClientIp(),u);
				free(u);
				SET(flags,RQ_CONNECTION_CLOSE);
				return -1;
			}
		}
#endif
#ifdef ENABLE_TF_EXCHANGE
	}
#endif
	if (len > 0) {
		left_read -= len;
	}
	return len;
}
void KHttpRequest::resetFetchObject()
{
	if (fetchObj) {
		KFetchObject *fo = fetchObj->clone(this);
		closeFetchObject(true);
		fetchObj = fo;
	}
}
void KHttpRequest::closeFetchObject(bool destroy)
{
	//printf("~KHttpRequest closeFetchObject [%p]\n", this);
	if (fetchObj) {
		fetchObj->close(this);
		if (destroy) {
			delete fetchObj;
			fetchObj = NULL;
		}
	}
#ifdef ENABLE_REQUEST_QUEUE
	KRequestQueue *queue = this->queue;
	this->queue = NULL;
	if (queue) {
		if (ctx->queue_handled) {
			ctx->queue_handled = false;
			async_queue_destroy(queue);
		}
		queue->release();
	}
#endif
}
StreamState KHttpRequest::write_all(const char *buf, int len) {

#ifdef ENABLE_TF_EXCHANGE
	if (tf) {
		if(!tf->writeBuffer(this,buf,len)){
			return STREAM_WRITE_FAILED;
		}
		return STREAM_WRITE_SUCCESS;
	}
#endif
	if(TEST(flags,RQ_SYNC)){
		int sleepTime = getSleepTime(len);
		if (sleepTime>0) {
			my_msleep(sleepTime);
		}
		send_size += len;
		addFlow(len, ctx->cache_hit);
		if(c->write_all(this,buf, len)){
			return STREAM_WRITE_SUCCESS;
		}
		return STREAM_WRITE_FAILED;
	} else {
		buffer.write_all(buf,len);
		return STREAM_WRITE_SUCCESS;
	}
}
void KHttpRequest::get_write_buf(iovec *buffer,int &bufferCount)
{
	if (slh) {
		bufferCount = 1;		
	}
	send_ctx.getReadBuffer(buffer,bufferCount);
	return;
}
char *KHttpRequest::get_write_buf(int &size)
{
	iovec buffer;
	int bufferCount = 1;
	send_ctx.getReadBuffer(&buffer,bufferCount);
	size = buffer.iov_len;
	return (char *)buffer.iov_base;
}
char *KHttpRequest::get_read_buf(int &size)
{
	int used = (int)(hot - readBuf);
	if ((unsigned) used >= (current_size-1)) {
		current_size += current_size;
		char *nb = (char *) xmalloc(current_size);
		/* resize buf */
		if (!nb) {
			size = 0;
			return NULL;
		}
		memcpy(nb, readBuf, used);
		xfree(readBuf);
		readBuf = nb;
		hot = readBuf + used;		
	}
	size = (int)current_size - used - 1;
	return hot;
}
WriteState KHttpRequest::canWrite(int got)
{
	if (got<=0) {
		return WRITE_FAILED;
	}
	addFlow(got, ctx->cache_hit);
	send_size += got;
	if (send_ctx.readSuccess(got)) {
		return WRITE_CONTINUE;
	}
	return WRITE_SUCCESS;
}
ReadState KHttpRequest::canRead(int got) {
	if (got<=0) {
		return READ_FAILED;
	}
	hot += got;
	int status = parser.parse(readBuf, (int)(hot - readBuf), this);
	if (status == HTTP_PARSE_CONTINUE && current_size >= MAX_HTTP_HEAD_SIZE) {
		//head is too large
		SET(raw_url.flags,KGL_URL_BAD);
		return READ_FAILED;
	}
	if (status == HTTP_PARSE_FAILED) {
		//debug("Failed to check headers,client=%s:%d.\n",
		//		server->get_remote_ip().c_str(), server->get_remote_port());
		SET(raw_url.flags,KGL_URL_BAD);
	}
	if (status != HTTP_PARSE_CONTINUE) {
		return READ_SUCCESS;
	}
	if (kgl_current_msec - this->begin_time_msec > conf.time_out * 2000) {
		SET(raw_url.flags, KGL_URL_BAD);
		return READ_FAILED;
	}
	return READ_CONTINUE;
}
void KHttpRequest::set_url_param(char *param) {
	
	url->param = xstrdup(param);

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
		pre_post_length = parser.bodyLen; 
	} else {
		left_read = content_length;
		pre_post_length = (int)(MIN(left_read,(INT64)parser.bodyLen));
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
	if (url==NULL || url->host == NULL || url->path == NULL || meth == METH_UNSET) {
		return true;
	}
	return false;
}
void KHttpRequest::freeUrl() {
	raw_url.destroy();
	if (TEST(workModel,WORK_MODEL_SSL)) {
		SET(raw_url.flags,KGL_URL_SSL|KGL_URL_ORIG_SSL);
	}
	meth = METH_UNSET;
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
	memset(data,0,sizeof(KHttpRequestData));
	assert(this->pool == NULL);
	this->pool = pool;
	if (this->pool == NULL) {
		this->pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
	}
	http_major = 1;
	http_minor = 1;
	setState(STATE_IDLE);
	hot = readBuf;
	begin_time_msec = kgl_current_msec;
	parser.start();
}
void KHttpRequest::clean(bool keep_alive) {
	if (c) {
		c->end_response(this,keep_alive);
	}
	closeFetchObject();
	while (sr) {
		KSubRequest *nsr = sr->next;
		sr->destroy(this);
		delete sr;
		sr = nsr;
	}
	if (file){
		delete file;
		file = NULL;
	}
	assert(ctx);
	ctx->clean();	
	buffer.clean();
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
	send_ctx.clean();
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
	if (pool) {
		kgl_destroy_pool(pool);
		pool = NULL;
	}
	while (fh) {
		KFlowInfoHelper *fh_next = fh->next;
		delete fh;
		fh = fh_next;
	}
}
void KHttpRequest::releaseVirtualHost()
{
	if (svh) {
		svh->release();
		svh = NULL;
	}
}
void KHttpRequest::close()
{
#ifdef ENABLE_KSAPI_FILTER
	/* http filter end connection hook */
	if (svh && svh->vh->hfm) {
		KHttpFilterHookCollect *end_connection = svh->vh->hfm->hook.end_connection;
		if (end_connection) {
			end_connection->process(this,KF_NOTIFY_END_CONNECT,NULL);
		}
	}
	if (conf.gvm->globalVh.hfm) {
		KHttpFilterHookCollect *end_connection = conf.gvm->globalVh.hfm->hook.end_connection;
		if (end_connection) {
			end_connection->process(this,KF_NOTIFY_END_CONNECT,NULL);
		}
	}
#endif
	if (
#ifdef ENABLE_HTTP2
		http2_ctx == NULL && 
#endif
		c) {
		assert(c->queue.next==NULL);
		c->app_data.rq = NULL;
	}
	ctx->clean_obj(this);
	clean(false);
	xfree(readBuf);
	if (auth) {
		delete auth;
	}
	delete ctx;
	if (client_ip) {
		free(client_ip);
		client_ip = NULL;
	}
	freeUrl();
	releaseVirtualHost();
#ifdef ENABLE_KSAPI_FILTER
	if (http_filter_ctx) {
		delete http_filter_ctx;
	}
#endif
}
void KHttpRequest::endSubRequest()
{
	stackSize--;
	ctx->clean_obj(this);
	if (conf.log_sub_request) {
		log_access(this);
	}
	kassert(sr!=NULL);
	//pop fetchObj
	if (fetchObj) {
		delete fetchObj;
	}
	fetchObj = sr->fetchObj;
	//pop file
	if (file) {
		delete file;
	}
	file = sr->file;
	//pop ctx
	ctx->clean();
	delete ctx;
	ctx = sr->ctx;
	ctx->st->preventWriteEnd = false;
	//pop url,must after pop ctx
	if (url) {
		url->destroy();
		delete url;
	}
	url = sr->url;
	meth = sr->meth;
	flags = sr->flags;
	KSubRequest *nsr = sr->next;
	sub_request_call_back callBack = sr->callBack;
	void *data = sr->data;
	delete sr;
	sr = nsr;
	if (!sr) {
		CLR(workModel,WORK_MODEL_INTERNAL);
	}
	kassert(fetchObj!=NULL);
	callBack(this,data,sub_request_pop);
}
void KHttpRequest::beginSubRequest(KUrl *url,sub_request_call_back callBack,void *data)
{
	if (stackSize>32) {
		url->destroy();
		delete url;
		callBack(this,data,sub_request_free);
		stageEndRequest(this);
		return;
	}
	stackSize++;
	kassert(url!=NULL);
	KSubRequest *nsr = new KSubRequest;
	nsr->next = sr;
	nsr->fetchObj = fetchObj;
	nsr->file = file;
	nsr->flags = flags;
	CLR(flags,RQ_HAVE_RANGE|RQ_HAS_IF_MOD_SINCE|RQ_HAS_IF_NONE_MATCH|RQ_OBJ_STORED);
	nsr->url = this->url;
	nsr->ctx = ctx;
	assert(ctx);
	ctx->st->preventWriteEnd = true;
	nsr->callBack = callBack;
	nsr->data = data;
	nsr->meth = meth;
	meth = METH_GET;
	fetchObj = NULL;
	file = NULL;
	ctx = new KContext;
	SET(workModel,WORK_MODEL_INTERNAL);
	this->url = url;
	sr = nsr;
	c->selector->next(resultNextSubRequest, this);
}
bool KHttpRequest::parseMeth(const char *src) {
	meth = KHttpKeyValue::getMethod(src);
	if (meth >= 0) {
		return true;
	}
	return false;
}
void KHttpRequest::endParse() {
	if (!TEST(flags,RQ_HAS_AUTHORIZATION|RQ_HAS_PROXY_AUTHORIZATION)) {
		if (auth) {
			delete auth;
			auth = NULL;
		}
	}
}
bool KHttpRequest::parseConnectUrl(char *src) {
	char *ss;
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	*ss = 0;
	raw_url.host = strdup(src);
	raw_url.port = atoi(ss + 1);
	return true;
}
int KHttpRequest::parseHost(char *val)
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
	return PARSE_HEADER_SUCCESS;
}
void KHttpRequest::startParse() {
	if (client_ip) {
		free(client_ip);
		client_ip = NULL;
	}
	freeUrl();
}
int KHttpRequest::parseHeader(const char *attr, char *val,int &val_len, bool isFirst) {
	//printf("attr=[%s] val=[%s]\n", attr, val);
#ifdef ENABLE_HTTP2
	if (http2_ctx) {
		if (strcmp(attr,":method")==0) {
			if (!parseMeth(val)) {
				klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
				return PARSE_HEADER_FAILED;
			}
			return PARSE_HEADER_NO_INSERT;
		}
		if (strcmp(attr,":version")==0) {
			parseHttpVersion(val);
			return PARSE_HEADER_NO_INSERT;
		}
		if (strcmp(attr,":path")==0) {
			parse_url(val, &raw_url);
			return PARSE_HEADER_NO_INSERT;
		}
		if (strcmp(attr,":authority")==0) {
			return parseHost(val);
		}
	} else 
#endif
	if (isFirst) {
		if (!parseMeth(attr)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse meth=[%s]\n", attr);
			return 0;
		}
		char *space = strchr(val, ' ');
		if (space == NULL) {
			klog(
					KLOG_DEBUG,
					"httpparse:cann't get space seperator to parse HTTP/1.1 [%s]\n",
					val);
			return 0;
		}
		*space = 0;
		space++;

		while (*space && IS_SPACE((unsigned char)*space))
			space++;
		bool result;
		if (meth == METH_CONNECT) {
			result = parseConnectUrl(val);
		} else {
			result = parse_url(val, &raw_url);
		}
		if (!result) {
			klog(KLOG_DEBUG, "httpparse:cann't parse url [%s]\n", val);
			return 0;
		}
		if (!parseHttpVersion(space)) {
			klog(KLOG_DEBUG, "httpparse:cann't parse http version [%s]\n",
					space);
			return 0;
		}
		return 2;
	}
	if (!strcasecmp(attr, "Host")) {
		return parseHost(val);
	}
	if (!strcasecmp(attr, "Connection")

	)
	{
		KHttpFieldValue field(val);
		do {
			if (field.is("keep-alive")) {
				if (conf.keep_alive_count>=0) {
					flags |= RQ_HAS_KEEP_CONNECTION;
				}
			} else if (field.is("upgrade")) {
				flags |= RQ_HAS_CONNECTION_UPGRADE;
			}
		} while(field.next());
		return PARSE_HEADER_SUCCESS;
	}
	if (!strcasecmp(attr, "Accept-Encoding")) {
		if (!*val) {
			return PARSE_HEADER_NO_INSERT;
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
		return PARSE_HEADER_SUCCESS;
	}
	if (!strcasecmp(attr,"If-Range")) {
		time_t try_time = parse1123time(val);
		if (try_time==-1) {
			flags |= RQ_IF_RANGE_ETAG;
			if (this->ctx->if_none_match == NULL) {
				ctx->set_if_none_match(val, val_len);
			}
		} else {
			if_modified_since = try_time;
			flags |= RQ_IF_RANGE_DATE;
		}
		return PARSE_HEADER_NO_INSERT;
	}
	if (!strcasecmp(attr, "If-Modified-Since")) {
		if_modified_since = parse1123time(val);
		flags |= RQ_HAS_IF_MOD_SINCE;
		return PARSE_HEADER_NO_INSERT;
	}
	if (!strcasecmp(attr,"If-None-Match")) {
		flags |= RQ_HAS_IF_NONE_MATCH;
		if (this->ctx->if_none_match==NULL) {
			ctx->set_if_none_match(val,val_len);
		}
		return PARSE_HEADER_NO_INSERT;
	}

	//	printf("attr=[%s],val=[%s]\n",attr,val);
	if (!strcasecmp(attr, "Content-length")) {
		content_length = string2int(val);
		left_read = content_length;
		flags |= RQ_HAS_CONTENT_LEN;
		return PARSE_HEADER_NO_INSERT;
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			SET(flags,RQ_INPUT_CHUNKED|RQ_CONNECTION_CLOSE);
			getDechunkEngine();
		}
		return PARSE_HEADER_SUCCESS;
	}
	if (!strcasecmp(attr, "Expect")) {
		if (strstr(val, "100-continue") != NULL) {
			flags |= RQ_HAVE_EXPECT;
		}
		return PARSE_HEADER_NO_INSERT;
	}
	if (!strcasecmp(attr, "X-Forwarded-Proto")) {
		if (strcasecmp(val,"https")==0) {
			SET(raw_url.flags,KGL_URL_ORIG_SSL);
		} else {
			CLR(raw_url.flags,KGL_URL_ORIG_SSL);
		}
		return PARSE_HEADER_NO_INSERT;
	}
	if (!strcasecmp(attr, "Pragma")) {
		if (strstr(val, "no-cache"))
			flags |= RQ_HAS_NO_CACHE;
		return PARSE_HEADER_SUCCESS;
	}
	if (

	!strcasecmp(attr, AUTH_REQUEST_HEADER)) {

		flags |= RQ_HAS_AUTHORIZATION;

#ifdef ENABLE_TPROXY
		if (!TEST(workModel,WORK_MODEL_TPROXY)) {
#endif
			char *p = val;
			while (*p && !IS_SPACE((unsigned char)*p))
				p++;
			char *p2 = p;
			while (*p2 && IS_SPACE((unsigned char)*p2))
				p2++;

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
		return PARSE_HEADER_SUCCESS;
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
		return 1;
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
			if (p && *(p+1)) {
				range_to = string2int(p + 1);
			}
			char *next_range = strchr(val,',');
			if (next_range) {
				//we do not support multi range
				klog(KLOG_INFO,"cut multi_range %s\n",val);
				//SET(filter_flags,RF_NO_CACHE);
				*next_range = '\0';
			}
		}
		flags |= RQ_HAVE_RANGE;
		return 1;
	}
	if (!strcasecmp(attr,"Content-Type")) {
#ifdef ENABLE_INPUT_FILTER
		if (if_ctx==NULL) {
			if_ctx = new KInputFilterContext(this);
		}
#endif
		if(strncasecmp(val,"multipart/form-data",19)==0){
			SET(flags,RQ_POST_UPLOAD);
#ifdef ENABLE_INPUT_FILTER
			if_ctx->parseBoundary(val+19);
#endif
		}
		return 1;
	}
	return 1;
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
		buff *bodys = obj->data->bodys;
		buff *head = bodys;
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
#ifdef ENABLE_KSAPI_FILTER
void KHttpRequest::init_http_filter()
{
	if (http_filter_ctx==NULL) {
		http_filter_ctx = new KHttpFilterContext;
		http_filter_ctx->init(this);
	}
}
#endif
bool KHttpRequest::responseHeader(const char *name,hlen_t name_len,const char *val,hlen_t val_len)
{
#ifdef ENABLE_HTTP2
	if (http2_ctx) {
		return c->http2->add_header(http2_ctx,name,name_len,val,val_len);
	}
#endif
	int len = name_len + val_len + 4;
	char *buf = (char *)malloc(len);
	char *hot = buf;
	memcpy(hot,name,name_len);
	hot += name_len;
	memcpy(hot,": ",2);
	hot += 2;
	memcpy(hot,val,val_len);
	hot += val_len;
	memcpy(hot,"\r\n",2);
	send_ctx.head_append(buf,len);
	return true;
}
void KHttpRequest::startResponseBody(INT64 body_len)
{
	assert(!TEST(flags,RQ_HAS_SEND_HEADER));
	if (TEST(flags,RQ_HAS_SEND_HEADER)) {
		return;
	}
	SET(flags,RQ_HAS_SEND_HEADER);
#ifdef ENABLE_HTTP2
	if (http2_ctx) {
		int got = c->start_response(this,body_len);
		addFlow(got, ctx->cache_hit);
		send_size += got;
		return;
	}
#endif
	kgl_str_t request_line;
	getRequestLine(status_code,&request_line);
	send_ctx.head_insert_const(request_line.data,(uint16_t)request_line.len);
	send_ctx.head_append_const("\r\n",2);
	c->start_response(this, body_len);
	return;
}
bool KHttpRequest::sync_send_buffer()
{
	for (;;) {
		iovec iov[16];
		int bufCount = 16;
		buffer.getReadBuffer(iov, bufCount);
		int got = c->write(this, iov, bufCount);
		if (got <= 0) {
			buffer.clean();
			return false;
		}
		if (!buffer.readSuccess(got)) {
			break;
		}
	}
	buffer.clean();
	return true;
}
bool KHttpRequest::sync_send_header()
{
	for (;;) {
		iovec iov[16];
		int bufCount = 16;
		send_ctx.getReadBuffer(iov, bufCount);
		int got = c->write(this, iov, bufCount);
		if (got <= 0) {
			send_ctx.clean();
			return false;
		}
		if (!send_ctx.readSuccess(got)) {
			break;
		}
	}
	send_ctx.clean();
	return true;
}
