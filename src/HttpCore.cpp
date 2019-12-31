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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <map>
#include <string.h>
#include <stdlib.h>
#include <vector>
#include <time.h>
#include <string>
#include <sstream>
#include "global.h"
#include "log.h"
#include "cache.h"
#include "http.h"
#include "utils.h"
#include "lib.h"

#include "KBuffer.h"
#include "KHttpRequest.h"
#include "KHttpResponseParser.h"
#include "KHttpObjectHash.h"
#include "KHttpKeyValue.h"
#include "KObjectList.h"
#include "KHttpTransfer.h"
#include "KSendable.h"
#include "KUpstream.h"
#include "kthread.h"
#include "lang.h"
#include "KRequestQueue.h"
#include "time_utils.h"
#include "KVirtualHostManage.h"
#include "kselector.h"
#include "KDirectoryFetchObject.h"
#include "KStaticFetchObject.h"
#include "KSyncFetchObject.h"
#include "KCacheFetchObject.h"
#include "KRewriteMarkEx.h"
#include "KUrlParser.h"
#include "KHttpFilterManage.h"
#include "KHttpProxyFetchObject.h"
#include "KSimulateRequest.h"
#include "KHttpObjectSwaping.h"
#include "ssl_utils.h"

kgl_str_t know_http_headers[] = {
	{ kgl_expand_string("Server") },
	{ kgl_expand_string("Date") }
};
using namespace std;
bool adjust_range(KHttpRequest *rq,INT64 &len)
{
        //printf("before from=%lld,to=%lld,len=%lld\n",rq->range_from,rq->range_to,len);
	if (rq->range_from >= 0){
		if(rq->range_from >= len) {
			klog(KLOG_ERR,"[%s] request [%s%s] range error,request range_from=" INT64_FORMAT ",range_to=" INT64_FORMAT ",len=" INT64_FORMAT "\n",rq->getClientIp(),rq->raw_url.host,rq->raw_url.path,rq->range_from,rq->range_to,len);
			return false;
		}
		len-=rq->range_from;
		if (rq->range_to >= 0) {
			len = MIN(rq->range_to - rq->range_from + 1,len);
			if(len<=0){
				klog(KLOG_ERR,"[%s] request [%s%s] range error,request range_from=" INT64_FORMAT ",range_to=" INT64_FORMAT ",len=" INT64_FORMAT "\n",rq->getClientIp(),rq->raw_url.host,rq->raw_url.path,rq->range_from,rq->range_to,len);
				return false;
			}
		}
	}else if(rq->range_from < 0){
		rq->range_from += len;	
		if(rq->range_from<0){
			rq->range_from = 0;
		}
		len-=rq->range_from;
	}
	rq->range_to = rq->range_from + len - 1;
       	//printf("after from=%lld,to=%lld,len=%lld\n",rq->range_from,rq->range_to,len);
	return true;
}
kbuf *inflate_buff(kbuf *in_buf, INT64 &len, bool fast) {
	KBuffer buffer;
	KGzipDecompress gzip(false,&buffer,false);
	gzip.setFast(fast);	
	while (in_buf && in_buf->used > 0) {
		if (gzip.write_all(in_buf->data, in_buf->used)
				!= STREAM_WRITE_SUCCESS) {
			break;
		}
		in_buf = in_buf->next;
	}
	gzip.write_end();
	len = buffer.getLen();
	return buffer.stealBuffFast();
}
//compress kbuf
kbuf *deflate_buff(kbuf *in_buf, int level, INT64 &len, bool fast) {
	KBuffer buffer;
	KGzipCompress gzip(false,&buffer,false);
	gzip.setFast(fast);
	while (in_buf && in_buf->used > 0) {
		if (gzip.write_all(in_buf->data, in_buf->used)!=STREAM_WRITE_SUCCESS) {
			return NULL;
		}
		in_buf = in_buf->next;
	}
	if (gzip.write_end()!=STREAM_WRITE_SUCCESS) {
		return NULL;
	}
	len = buffer.getLen();
	return buffer.stealBuffFast();
}
char * skip_next_line(char *str, int &str_len) {
	int line_pos;
	if (str_len == 0) {
		return NULL;
	}
	char *next_line = (char *) memchr(str, '\n', str_len);
	if (next_line == NULL) {
		//	printf("next line is NULL\n");
		return NULL;
	}
	line_pos = next_line - str + 1;
	str += line_pos;
	str_len -= line_pos;
	return str;
}
/**
* 创建写入流，根据情况，前面可能要加解压缩和解chunked数据。
*/
KWStream *makeWriteStream(KHttpRequest *rq, KHttpObject *obj, KWStream *st,
		bool &autoDelete) {
#if 0
	if (TEST(obj->index.flags,OBJ_GZIPED)) {
		rq->ctx->stream_gziped = true;
		if (rq->needFilter() || !TEST(rq->flags,RQ_HAS_GZIP) || TEST(rq->workModel,WORK_MODEL_SIMULATE)) {
			//三种情况需要解压内容
			//1.需要内容过滤。
			//2.即使没有发送gzip(下游并不支持gzip)到upstream中,upstream也会假定我们支持gzip，比如spdy协议。
			//3.模拟
			//解压了就把压缩志清掉
			//printf("加载dgzip层\n");
			if (!obj->in_cache) {
				//如果obj不在缓存中，清除gziped标识
				CLR(obj->index.flags,OBJ_GZIPED);
			}
			rq->ctx->stream_gziped = false;
			KHttpStream *st2 = new KGzipDecompress(false,st, autoDelete);
			st = st2;
			autoDelete = true;
		}
	} else {
		rq->ctx->stream_gziped = false;
	}
#endif
	if (TEST(obj->index.flags,ANSW_CHUNKED)) {
		CLR(obj->index.flags,ANSW_CHUNKED);
		rq->ctx->upstream_chunked = true;
		KHttpStream *st2 = new KDeChunked(st, autoDelete);
		st = st2;
		autoDelete = true;
	}
	return st;
}
kev_result send_http(KHttpRequest *rq, KHttpObject *obj, uint16_t status_code, KAutoBuffer *body) {
	//printf("send_http status=[%d],rq=[%p].\n", status_code, rq);
	rq->closeFetchObject();
	if (obj) {
		obj->data->status_code = status_code;
	}	
#ifdef ENABLE_TF_EXCHANGE
	rq->closeTempFile();
#endif
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return kev_err;
	}
	rq->responseStatus(status_code);
	if (rq->auth) {
		rq->auth->insertHeader(rq);
	}	
	timeLock.Lock();
	rq->responseHeader(kgl_response_server, conf.serverName, conf.serverNameLength);
	rq->responseHeader(kgl_response_date,(char *)cachedDateTime,29);
	timeLock.Unlock();
	if (body) {
		rq->responseHeader(kgl_expand_string("Content-Type"),kgl_expand_string("text/html; charset=utf-8"));
	}
	if (TEST(rq->filter_flags, RF_X_CACHE)) {
		KStringBuf b;
		if (rq->ctx->cache_hit_part) {
			b.WSTR("HIT-PART from ");
		}
		else if (rq->ctx->cache_hit) {
			b.WSTR("HIT from ");
		}
		else {
			b.WSTR("MISS from ");
		}
		b << conf.hostname;
		rq->responseHeader(kgl_expand_string("X-Cache"), b.getBuf(), b.getSize());
	}
	INT64 send_len = 0;
	if (obj) {
		if (!obj->checkNobody()) {
			SET(obj->index.flags,ANSW_HAS_CONTENT_LENGTH);
			obj->index.content_length = (body ? body->getLen() : 0);
			rq->responseHeader(kgl_expand_string("Content-Length"),(int) obj->index.content_length);
			send_len = obj->index.content_length;
		}
	} else if (!is_status_code_no_body(status_code)) {
		send_len = (body ? body->getLen() : 0);
		rq->responseHeader(kgl_expand_string("Content-Length"), (int)send_len);
	}
	rq->responseConnection();
	rq->startResponseBody(send_len);
	if (body) {
		return stageWriteRequest(rq, body);
	}
	return stageEndRequest(rq);
}
kev_result send_auth(KHttpRequest *rq,KAutoBuffer *body) {
	uint16_t status_code = AUTH_STATUS_CODE;
	if (rq->auth) {
		status_code = rq->auth->GetStatusCode();
	}
	return send_http(rq, NULL, status_code, body);
}
/**
* 发送错误信息
*/
kev_result send_error(KHttpRequest *rq, KHttpObject *obj, int code,const char* reason)
{
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER) || rq->ctx->read_huped) {
		if (!TEST(rq->flags,RQ_SYNC)) {
			return stageEndRequest(rq);
		}
		return kev_err;
	}
	SET(rq->flags, RQ_IS_ERROR_PAGE);

	if (rq->meth==METH_HEAD) {
		return send_http(rq, obj, code, NULL);
	}
	KAutoBuffer s(rq->pool);
	assert(rq);
	std::string errorPage;
	if (conf.gvm->globalVh.getErrorPage(code, errorPage)) {
		if (strncasecmp(errorPage.c_str(), "file://", 7) == 0) {
			errorPage = errorPage.substr(7,errorPage.size()-7);
		}
		if (!isAbsolutePath(errorPage.c_str())) {
			errorPage = conf.path + errorPage;
		}
		KFile fp;
		if (fp.open(errorPage.c_str(), fileRead)) {
			INT64 len = fp.getFileSize();
			len = MIN(len, 32768);
			kbuf *buf = new_pool_kbuf(rq->pool,int(len));
			int used = fp.read(buf->data, (int)len);
			buf->used = used;
			if (used > 2) {
				char *p = (char *)memchr(buf->data, '~', used - 2);
				if (p != NULL) {
					char tmp[5];
					snprintf(tmp, 4, "%03d", code);
					kgl_memcpy(p, tmp, 3);
				}
			}
			s.Append(buf);
			return send_http(rq, obj, code, &s);
		}
	}
	const char *status = KHttpKeyValue::getStatus(code);
	s << "<html>\n<head>\n";
	s << "	<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n";
	s << "	<title>" << code << " " << status << "</title>\n";
	s << "</head>\n<body>\n<div id='main'";

	s << ">\n<i><h2>Something error:</h2></i>\n<p><h3>" << code << " ";
	s << status;
	s << "</h3><h3><font color='red'>";
	s << reason;
	s << "</font></h3></p>\n<p>Please check or <a href='javascript:location.reload()'>try again</a> later.</p>\n";
	if (*conf.hostname) {
		s << "<div>hostname: " << conf.hostname << "</div>";
	}
	s << "<hr>\n";
	s << "<div id='pb'>";
	
	s << "Generated by <a href='https://www.cdnbest.com/?code=" << code << "' target=_blank>" << PROGRAM_NAME << "/" << VERSION << "</a>.\n";
	s << "</div>\n";
	s << "</div>\n";

	s << "<!-- padding for ie -->";
	s << "<!-- padding for ie -->";
	s << "<!-- padding for ie -->";
	s << "<!-- padding for ie -->\n";
	s << "</body></html>";
	return send_http(rq, obj, code, &s);
}
/**
* 插入via头
*/
void insert_via(KHttpRequest *rq, KWStream &s, char *old_via) {
	s << "Via: ";
	if (old_via) {
		s << old_via << ",";
	}
	s << (int)rq->http_major << "." << (int)rq->http_minor << " ";
	if (*conf.hostname) {
		s << conf.hostname;
	} else {
		sockaddr_i addr;
		rq->sink->GetSelfAddr(&addr);
		char ip[MAXIPLEN];
		ksocket_sockaddr_ip(&addr,ip,sizeof(ip));
		s << ip << ":" << ksocket_addr_port(&addr);
	}
	s << "(";
	timeLock.Lock();
	s.write_all(conf.serverName,conf.serverNameLength);
	timeLock.Unlock();
	s << ")\r\n";
}
/*************************
* 创建回应http头信息
*************************/
bool build_obj_header(KHttpRequest *rq, KHttpObject *obj,INT64 content_len, INT64 &start, INT64 &send_len) {
	start = 0;
	send_len = content_len;
	assert(!TEST(rq->flags,RQ_HAS_SEND_HEADER));
	//SET(rq->flags,RQ_HAS_SEND_HEADER);
	if (obj->data->status_code == 0) {
		obj->data->status_code = STATUS_OK;
	}
	bool build_first = true;
	if (TEST(rq->flags, RQ_HAVE_RANGE) 
		&& !TEST(obj->index.flags,ANSW_CHUNKED)
		&& obj->data->status_code == STATUS_OK 
		&& content_len>0) {
		send_len = content_len;
		if (!adjust_range(rq,send_len)) {			
			build_first = false;
			rq->responseStatus(416);
			rq->range_from = -1;
			start = -1;
		} else {
			if (!TEST(rq->raw_url.flags,KGL_URL_RANGED)) {				
				build_first = false;
				rq->responseStatus(STATUS_CONTENT_PARTIAL);
				KStringBuf s;
				s.WSTR("bytes ");
				s.add(rq->range_from,INT64_FORMAT);
				s.WSTR("-");
				s.add(rq->range_to, INT64_FORMAT);
				s.WSTR("/");
				s.add(content_len, INT64_FORMAT);
				rq->responseHeader(kgl_expand_string("Content-Range"),s.getBuf(),s.getSize());
			}
			start = rq->range_from;
			content_len = send_len;
		}
	}
	if (build_first) {
		uint16_t status_code = obj->data->status_code;
		if (TEST(rq->raw_url.flags,KGL_URL_RANGED) && rq->status_code==STATUS_CONTENT_PARTIAL) {
			//如果请求是url模拟range，则强制转换206的回应为200
			status_code = STATUS_OK;
		}
		rq->responseStatus(status_code);
	}
	if (TEST(obj->index.flags,ANSW_LOCAL_SERVER)) {		
		timeLock.Lock();
		rq->responseHeader(kgl_response_server, conf.serverName, conf.serverNameLength);
		rq->responseHeader(kgl_response_date,(char *)cachedDateTime,29);
		timeLock.Unlock();
	}
	//bool via_inserted = false;
	//发送附加的头
	KHttpHeader *header = obj->data->headers;       
	while (header) {
		rq->responseHeader(header->attr,header->attr_len,header->val,header->val_len);
		header = header->next;
	}
	//发送Age头
	if (TEST(rq->filter_flags,RF_AGE) && !TEST(obj->index.flags,FLAG_DEAD|ANSW_NO_CACHE)) {
		int current_age = (int)obj->getCurrentAge(kgl_current_sec);
		if (current_age > 0) {
			rq->responseHeader(kgl_expand_string("Age"),current_age);
		}
	}
	if (TEST(rq->filter_flags,RF_X_CACHE)) {
		KStringBuf b;
		if (rq->ctx->cache_hit_part) {
			b.WSTR("HIT-PART from ");
		} else if (rq->ctx->cache_hit) {
			b.WSTR("HIT from ");
		} else {
			b.WSTR("MISS from ");
		}	
		b << conf.hostname;
		rq->responseHeader(kgl_expand_string("X-Cache"),b.getBuf(),b.getSize());
	}
	if (!TEST(obj->index.flags,FLAG_NO_BODY)) {
		/*
		* no body的不发送content-length
		* head method要发送content-length,但不发送内容
		*/
		if (content_len >= 0) {
			//有content-length时
			char len_str[INT2STRING_LEN];
			int len = snprintf(len_str, INT2STRING_LEN - 1, INT64_FORMAT, content_len);
			rq->responseHeader(kgl_expand_string("Content-Length"),len_str,len);
		} else {
			//无content-length时
			if (rq->http_minor == 0) {
				//A HTTP/1.0 client no support TE head.
				//The connection MUST close
				SET(rq->flags,RQ_CONNECTION_CLOSE);
			} else if (!rq->ctx->connection_upgrade	&& rq->sink->SetTransferChunked()) {				
				SET(rq->flags,RQ_TE_CHUNKED);
			}
		}
	}
	rq->responseConnection();
	rq->startResponseBody(send_len);
	return true;
}
//试图发送，如果rq->buffer有数据就发送,并返回true，否则返回false
kev_result try_send_request(KHttpRequest *rq)
{
	
	return rq->tr->TryWrite();
}
kev_result stage_rdata_end(KHttpRequest *rq,StreamState result)
{
	
	rq->closeFetchObject();
	

	KHttpObject *obj = rq->ctx->obj;
	if (rq->ctx->upstream_chunked && result != STREAM_WRITE_END) {
		//上游是chunked数据，又未正常结束
		assert(!rq->ctx->upstream_expected_done);
		result = STREAM_WRITE_FAILED;
	} else if (rq->ctx->know_length && rq->ctx->left_read != 0) {
		//有content-length，又未读完
		result = STREAM_WRITE_FAILED;
	}
	if (result == STREAM_WRITE_FAILED) {
		if (obj->data->type==MEMORY_OBJECT) {
			obj->Dead();
			SET(rq->flags,RQ_CONNECTION_CLOSE);
		}
		if(TEST(rq->filter_flags,RQ_RESPONSE_DENY) && !TEST(rq->flags,RQ_HAS_SEND_HEADER)){
			return send_error(rq,NULL,STATUS_FORBIDEN,"access denied by response control");
		}
		//如果是http2的情况下，此处要向下游传递错误，调用terminal stream
		//避免下游会误缓存
		rq->ctx->body_not_complete = 1;
		return stageEndRequest(rq);
	}
	kassert(rq->ctx->st);
	result = rq->ctx->st->write_end();	
	if (result == STREAM_WRITE_FAILED) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
	}
	if (TEST(rq->filter_flags,RQ_RESPONSE_DENY) && !TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return send_error(rq,NULL,STATUS_FORBIDEN,"access denied by response control");
	}
	kev_result ret = try_send_request(rq);
	if (KEV_HANDLED(ret)) {
		return ret;
	}
#ifdef  ENABLE_BIG_OBJECT_206
	if (rq->bo_ctx) {
		return rq->bo_ctx->write_request_end(rq);
	}
#endif
	return stageEndRequest(rq);
	
}
void prepare_write_stream(KHttpRequest *rq)
{
	kassert(rq->tr == NULL);
	kassert(rq->ctx->st == NULL);
	bool autoDelete = true;
	rq->tr = new KHttpTransfer(rq, rq->ctx->obj);
	rq->ctx->st = makeWriteStream(rq, rq->ctx->obj, rq->tr, autoDelete);
	kassert(rq->fetchObj);
}
/**
异步load body准备
*/
kev_result prepare_load_body(KHttpRequest *rq)
{
	prepare_write_stream(rq);
	if (rq->ctx->connection_upgrade
#ifdef WORK_MODEL_TCP
		&& !TEST(rq->sink->GetBindServer()->flags,WORK_MODEL_TCP)
#endif
		) {
		rq->tr->sendHead(false);
		kev_result ret = rq->tr->TryWrite();
		if (KEV_HANDLED(ret)) {
			return ret;
		}
	}
	return rq->fetchObj->readBody(rq);
}

bool push_redirect_header(KHttpRequest *rq, const char *url,int url_len,int code) {
	if (code==0) {
		code = STATUS_FOUND;
	}
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return false;
	}
	rq->responseStatus(code);
	timeLock.Lock();
	rq->responseHeader(kgl_response_server, conf.serverName, conf.serverNameLength);
	rq->responseHeader(kgl_response_date, (char *)cachedDateTime, 29);
	timeLock.Unlock();
	rq->responseHeader(kgl_expand_string("Location"),url,url_len);
	rq->responseHeader(kgl_expand_string("Content-Length"), 0);
	rq->responseConnection();
	return true;
}
kev_result send_not_modify_from_mem(KHttpRequest *rq) {
	return send_http(rq, NULL, STATUS_NOT_MODIFIED);
}
kev_result processCacheReadyRequest(KHttpRequest *rq,KHttpObject *obj, swap_in_result result) {
	switch (result) {
	case swap_in_busy:
		klog(KLOG_ERR,"obj swap in busy drop request.\n");
		return send_error(rq, NULL, STATUS_SERVER_ERROR, "swap in busy");
	case swap_in_success:
	{
		KHttpObject *obj = rq->ctx->obj;
		rq->ctx->cache_hit = true;
		
		return processCacheWithCheckExpire(rq, obj);
	}
	default:
	{
#ifdef ENABLE_DISK_CACHE
		//不能swap in就从源上去取
		char *filename = obj->getFileName();
		klog(KLOG_ERR, "obj swap in failed cache file [%s] error=[%d].\n", filename,result);
		free(filename);
#endif
		rq->ctx->clean_obj(rq);
		rq->ctx->new_object = true;
		rq->ctx->lastModified = 0;
		rq->ctx->obj = new KHttpObject(rq);
		if (swap_in_failed_clean_blocked == result) {
			SET(rq->ctx->obj->index.flags, FLAG_DEAD);
		}
		if (rq->file) {
			delete rq->file;
			rq->file = NULL;
		}
		assert(rq->fetchObj || TEST(rq->sink->GetBindServer()->flags, WORK_MODEL_MANAGE) || rq->svh);
		rq->resetFetchObject();
		return asyncLoadHttpObject(rq);
	}
	}
}
kev_result processCacheRequest(KHttpRequest *rq) {
	//printf("processCacheRequest rq=[%p].\n", rq);
	KHttpObject *obj = rq->ctx->obj;
	if (!TEST(obj->index.flags,FLAG_IN_MEM)) {
		KMutex *lock = obj->getLock();
		//rq->c->removeRequest(rq,true);
		lock->Lock();
		if (obj->data==NULL) {
			if (cache.IsCleanBlocked()) {
				lock->Unlock();
				return processCacheReadyRequest(rq, obj, swap_in_failed_clean_blocked);
			}
#ifdef ENABLE_DISK_CACHE
			KHttpObjectSwaping *obj_swap = new KHttpObjectSwaping;
			obj->data = new KHttpObjectBody();
			obj->data->type = SWAPING_OBJECT;
			obj->data->os = obj_swap;
			obj->data->os->addTask(rq,processCacheReadyRequest);
			lock->Unlock();
			return obj_swap->swapin(rq,obj);
#else
			lock->Unlock();
			SET(obj->index.flags,FLAG_DEAD);
			klog(KLOG_ERR,"BUG!! obj is not in memory.");
			assert(false);
			return stageEndRequest(rq);
#endif
#ifdef ENABLE_DISK_CACHE
		} else if(obj->data->type == SWAPING_OBJECT) {
			//已经有其它线程在swap
			KHttpObjectSwaping *os = obj->data->os;
			assert(os);
			os->addTask(rq,processCacheReadyRequest);
			lock->Unlock();
			return kev_ok;
#endif
		}
		lock->Unlock();		
	}
	return processCacheReadyRequest(rq,obj,swap_in_success);
}
/**
* 发送在内存中的object.
*/
kev_result sendMemoryObject(KHttpRequest *rq,KHttpObject *obj)
{

	rq->closeFetchObject();
	if (TEST(rq->flags, RQ_HAVE_RANGE) && obj->url->IsContentEncoding()) {
		//有压缩的内容，不回应206
		CLR(rq->flags, RQ_HAVE_RANGE);
	}
#if 0
	if (TEST(obj->index.flags, OBJ_CACHE_RESPONSE)) {		
		if (!TEST(rq->workModel, WORK_MODEL_MANAGE | WORK_MODEL_INTERNAL | WORK_MODEL_SKIP_ACCESS)
			&& checkResponse(rq, obj) == JUMP_DENY) {
			handleError(rq, STATUS_FORBIDEN, "access denied by response control");
			return;
		}
	}
#endif
	if (obj->data->type==MEMORY_OBJECT && !obj->isNoBody(rq)) {
		if (rq->needFilter()) {
			rq->fetchObj = new KCacheFetchObject(obj);
			return rq->fetchObj->open(rq);
		}
		//assert(!TEST(obj->index.flags,OBJ_IS_DELTA));
		assert(!TEST(obj->index.flags,ANSW_CHUNKED));
	}
	kbuf *send_buffer = obj->data->bodys;
	INT64 content_len = obj->index.content_length;
	if (!obj->in_cache && !TEST(obj->index.flags,ANSW_HAS_CONTENT_LENGTH)) {
		content_len = -1;
	}
	INT64 send_len = content_len;
	INT64 start = 0;
	build_obj_header(rq, obj, content_len, start, send_len);
	if (TEST(obj->index.flags,FLAG_NO_BODY) || rq->meth == METH_HEAD || start == -1) {
			send_buffer = NULL;
			send_len = -1;
		/*
		 * do not need send body
		 */
	}
	
	return stageWriteRequest(rq,send_buffer,(int)start,(int)send_len);
}


void send_memory_object_swap_call_back(KHttpRequest *rq,bool result) {
	if (!result) {
		//不能swap in就从源上去取
		rq->ctx->clean_obj(rq);
		rq->ctx->new_object = true;
        rq->ctx->lastModified = 0;
        rq->ctx->obj = new KHttpObject(rq);
		if (rq->file) {
			delete rq->file;
			rq->file = NULL;
		}		
		assert(rq->fetchObj || TEST(rq->sink->GetBindServer()->flags,WORK_MODEL_MANAGE) || rq->svh);
		rq->resetFetchObject();
		asyncLoadHttpObject(rq);
		return;
	}
	sendMemoryObject(rq,rq->ctx->obj);
}
kev_result asyncSendHttpObject(KHttpRequest *rq)
{
	
	kassert(!TEST(rq->flags,RQ_SYNC));
	if (TEST(rq->filter_flags,RQ_SWAP_OLD_OBJ)) {
		rq->ctx->popObj();
		CLR(rq->filter_flags,RQ_SWAP_OLD_OBJ);
	}
	return sendMemoryObject(rq,rq->ctx->obj);
}
bool sync_send_http_object(KHttpRequest *rq, KHttpObject *obj) {
	kbuf *send_buffer = NULL;
	INT64 send_len = 0;
	INT64 start = 0;
	unsigned this_send_len = 0;
	bool result = true;
	INT64 content_len = 0;
	assert(!TEST(obj->index.flags,ANSW_CHUNKED));
	KMutex *lock = obj->getLock();
	lock->Lock();
#ifdef ENABLE_DISK_CACHE
	if (obj->data==NULL) {
		assert(TEST(obj->index.flags,FLAG_IN_DISK));
		KHttpObjectBody *data = new KHttpObjectBody();
		obj->data = data;
		if (!obj->swapin(data)) {
			obj->Dead();
			lock->Unlock();
			cache.dead(obj, __FILE__, __LINE__);
			//objList[obj->list_state].dead(obj);
			return send_error(rq, NULL, STATUS_SERVER_ERROR, "Cann't swap in object!") == kev_ok;
		}
		SET(obj->index.flags,FLAG_IN_MEM);
		cache.getHash(obj->h)->incSize(obj->index.content_length + obj->index.head_size);
	}
#endif	
	send_buffer = obj->data->bodys;
	content_len = obj->index.content_length;
	lock->Unlock();
	send_len = content_len;
	result = build_obj_header(rq, obj, content_len,start, send_len);
	if (TEST(obj->index.flags,FLAG_NO_BODY) || rq->meth == METH_HEAD || !result || start == -1) {
		/*
		 * do not need send body
		 */
		return result;
	}
	while (send_buffer != NULL) {
		if ((INT64) send_buffer->used <= start) {
			start -= send_buffer->used;
			goto next_buffer;
		}
		if (send_len <= 0){
				break;
		}
		if (send_len > (INT64) send_buffer->used - start) {
			this_send_len = send_buffer->used - (unsigned) start;
		} else {
			this_send_len = (int) send_len;
		}
		if (!rq->WriteAll(send_buffer->data + start, this_send_len)) {
			result = false;
			break;
		}
		start = 0;
		send_len -= this_send_len;
		next_buffer: send_buffer = send_buffer->next;
	}
	if (result) {
		assert(send_len==0);
	}
	return result;
}
kev_result stageError(KHttpRequest *rq,int code,const char *msg)
{
	bool sync = rq->IsSync();
	kev_result ret = send_error(rq,NULL,STATUS_FORBIDEN,"access denied by response control");
	if (sync) {
		return stageEndRequest(rq);
	}
	return ret;
}
kev_result handleXSendfile(KHttpRequest *rq)
{
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return send_error(rq,NULL,STATUS_SERVER_ERROR,"X-Accel-Redirect cann't send body");
	}
	rq->closeFetchObject();
	
	char *xurl = NULL;
	bool x_proxy_redirect = false;
	KHttpHeader *header = rq->ctx->obj->data->headers;
	while (header) {
		if (strcasecmp(header->attr,"X-Accel-Redirect")==0) {
			xurl = header->val;
			x_proxy_redirect = false;
		} else if (strcasecmp(header->attr, "X-Proxy-Redirect") == 0) {
			xurl = header->val;
			x_proxy_redirect = true;
		} else {
			rq->responseHeader(header->attr,header->attr_len,header->val,header->val_len);
		}
		header = header->next;
	}
	kassert(xurl!=NULL);
	if (xurl==NULL) {
		return stageEndRequest(rq);
	}
	if (!rq->rewriteUrl(xurl,0)) {
		return send_error(rq,NULL,STATUS_SERVER_ERROR,"X-Accel-Redirect value is not right");
	}
	rq->ctx->clean_obj(rq);
	if (rq->file) {
		delete rq->file;
		rq->file = NULL;
	}
	if (x_proxy_redirect) {
		rq->closeFetchObject();
		rq->fetchObj = new KHttpProxyFetchObject();
	} else {
		rq->ctx->internal = 1;
		rq->ctx->replace = 1;
	}
	return async_http_start(rq);
}
//上游无http body的处理
kev_result handleUpstreamNoBody(KHttpRequest *rq)
{
	rq->fetchObj->readBodyEnd(rq);
#ifdef ENABLE_TF_EXCHANGE
	rq->closeTempFile();
#endif	
	
	if (!TEST(rq->filter_flags,RQ_SWAP_OLD_OBJ) && TEST(rq->ctx->obj->index.flags,ANSW_XSENDFILE)) {
		return handleXSendfile(rq);
	}
	rq->closeFetchObject(false);
	if (TEST(rq->flags,RQ_SYNC)) {
		//同步发送
		if(!sync_send_http_object(rq, TEST(rq->filter_flags,RQ_SWAP_OLD_OBJ)?rq->ctx->old_obj:rq->ctx->obj)){
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return kev_err;
		}
		return kev_ok;
	}
	//异步发送
	return asyncSendHttpObject(rq);	
}
kev_result handleUpstreamRecvedHead(KHttpRequest *rq)
{
#ifdef ENABLE_TF_EXCHANGE
	if (!rq->ctx->internal) {
		if (!TEST(rq->filter_flags,RF_NO_BUFFER) &&
			rq->fetchObj->NeedTempFile(false,rq)) {
			//在需要temp file的上游，并且没有指定no_buffer
			if (rq->tf == NULL) {
				rq->tf = new KTempFile;
			}
			//切换temp file为收文件
			rq->tf->init(-1);
		} else {
			rq->closeTempFile();
		}
	}
#endif
	KContext *context = rq->ctx;
	//KHttpRequest *rq = context->rq;
	KHttpObject *obj = rq->ctx->obj;
	if (obj->data->status_code==0) {
		//如果status没有设置，设置为200
		obj->data->status_code = STATUS_OK;
	}
	int status_code = obj->data->status_code;
	//context->us_code = obj->data->status_code;
	if (status_code != STATUS_OK && status_code != STATUS_CONTENT_PARTIAL) {
		SET(obj->index.flags,ANSW_NO_CACHE|OBJ_NOT_OK);
	}
	if (checkResponse(rq,obj) == JUMP_DENY) {
		if (rq->sink->HasHeaderDataToSend()) {
			
			rq->closeFetchObject();
			rq->startResponseBody(-1);
			return stageEndRequest(rq);
		}
		return	stageError(rq, STATUS_FORBIDEN, "access denied by response control");
	}
	obj->checkNobody();
	//printf("load head status=%d\n", obj->data->status_code);
	switch (status_code) {
	case STATUS_NOT_MODIFIED:
		SET(obj->index.flags,FLAG_DEAD);
		CLR(obj->index.flags,ANSW_CHUNKED);
		if (context->old_obj) {
			if(!TEST(rq->flags,RQ_HAS_IF_MOD_SINCE|RQ_HAS_IF_NONE_MATCH)) {
				//直接发送old_obj给客户
				SET(rq->filter_flags,RQ_SWAP_OLD_OBJ);
			}
			SET(rq->flags,RQ_OBJ_VERIFIED);
			context->old_obj->index.last_verified = kgl_current_sec;
			//if (context->old_obj->data->type==BIG_OBJECT) {
			//	context->old_obj->data->bo->saveLastVerified(context->old_obj);
			//}
		} else {
			rq->ctx->no_body = true;
		}
		return handleUpstreamNoBody(rq);
	default:
		rq->ctx->cache_hit = false;
		if (rq->meth == METH_HEAD || TEST(context->obj->index.flags,FLAG_NO_BODY)) {
			//没有http body的情况
			SET(obj->index.flags,FLAG_DEAD);
			CLR(obj->index.flags,ANSW_CHUNKED);
			rq->ctx->no_body = true;
			return handleUpstreamNoBody(rq);
		}
		if (TEST(rq->ctx->obj->index.flags,ANSW_HAS_CONTENT_LENGTH) && !rq->ctx->connection_upgrade) {
			rq->ctx->know_length = true;
			rq->ctx->left_read = rq->ctx->obj->index.content_length;
		} else {
			rq->ctx->know_length = false;
			rq->ctx->left_read = -1;
		}
		
		if (status_code==STATUS_CONTENT_PARTIAL && !obj->IsContentRangeComplete(rq)) {
			//强行设置206不缓存
			SET(obj->index.flags,ANSW_NO_CACHE|OBJ_NOT_OK);
		} else {
			CLR(rq->flags,RQ_HAVE_RANGE);
		}
		kassert(!TEST(rq->flags, RQ_SYNC));
		return prepare_load_body(rq);
	
	}
}

kev_result handleError(KHttpRequest *rq,int code,const char *msg) {
	
	if (TEST(rq->filter_flags,RF_ALWAYS_ONLINE)) {
		//always on
		if (rq->ctx->old_obj) {
			//have cache
			rq->ctx->always_on_model = true;
			rq->ctx->popObj();
			if (JUMP_DENY == checkResponse(rq,rq->ctx->obj)) {
				return send_error(rq,NULL,STATUS_FORBIDEN,"denied by response access");
			}
			return async_send_valide_object(rq,rq->ctx->obj);
		} else if (TEST(rq->flags,RQ_HAS_IF_MOD_SINCE|RQ_HAS_IF_NONE_MATCH)) {
			//treat as not-modified
			return send_not_modify_from_mem(rq);
		}
	}
	if (rq->svh==NULL || code<403 || code>499) {
		return send_error(rq,NULL,code,msg);
	}
	KHttpObject *obj = rq->ctx->obj;
	obj->data->status_code = code;
	if(TEST(rq->flags,RQ_IS_ERROR_PAGE)){
		//如果本身是错误页面，又产生错误
		return send_error(rq,NULL,code,msg);
	}
	//设置为错误页面
	SET(rq->flags,RQ_IS_ERROR_PAGE);
	//清除range请求
	CLR(rq->flags,RQ_HAVE_RANGE);
	assert(rq->svh);
	string errorPage2;
	if (!rq->svh->vh->getErrorPage(code, errorPage2)) {
		return send_error(rq,NULL,code,msg);
	}
	const char *errorPage = errorPage2.c_str();
	if (strncasecmp(errorPage, "http://", 7) == 0 || strncasecmp(errorPage, "https://", 8) == 0) {
		stringstream s;
		if (rq->svh->vh->status!=0) {
			s << errorPage << "?name=" << rq->svh->vh->name << "&status=" << rq->svh->vh->status << "&url=" << rq->getInfo();
		} else {
			s << errorPage << "?" << obj->data->status_code << "," << rq->getInfo();
		}
		push_redirect_header(rq,s.str().c_str(),s.str().size(),STATUS_FOUND);
		rq->startResponseBody(0);
		return stageWriteRequest(rq,NULL);
	}
	//string path;
	bool result = false;
	if (rq->file) {
		delete rq->file;
	}
	rq->file = new KFileName;
	/*
	skip_redirect 如果是本地文件file:// 则不用查找扩展映射，当普通文件即可.
	*/
	//bool skip_redirect = true;
	if (strncasecmp(errorPage, "file://", 7) == 0) {
		errorPage += 7;
		if(isAbsolutePath(errorPage)){
			result = rq->file->setName(errorPage);
		}else{
			result = rq->file->setName(conf.path.c_str(),errorPage,rq->getFollowLink());
		}
	} else {
		//skip_redirect = false;
	//	path = rq->svh->doc_root;
		KStringBuf errorUrl;
		if (errorPage[0] != '/') {
			char *url = (char *) xstrdup(rq->url->path);
			char *p = strrchr(url, '/');
			if (p) {
				*(p + 1) = '\0';
			}
			errorUrl << url << errorPage;
			xfree(url);
		} else {
			errorUrl << errorPage;
		}
		result = rq->file->setName(rq->svh->doc_root,errorPage,rq->getFollowLink());
		if(result){
			result = rq->rewriteUrl(errorUrl.getString(),code);
		}
	}
	if (result && !rq->file->isDirectory()) {
		bool redirect_result;
		KFetchObject *fo = rq->svh->vh->findFileExtRedirect(rq, rq->file, true,redirect_result);
		if (fo ==NULL) {
			fo = new KStaticFetchObject;
		}
		rq->appendFetchObject(fo);
		return processRequest(rq);
	}
	return send_error(rq,NULL,code,msg);
	//*/
}
KFetchObject *bindVirtualHost(KHttpRequest *rq,RequestError *error,KAccess **htresponse,bool &handled) {
	assert(rq->file==NULL);
	//file = new KFileName;
	assert(!rq->hasFinalFetchObject());
	KFetchObject *redirect = NULL;
	bool result = false;
	bool redirect_result = false;
	char *indexPath = NULL;
	bool indexFileFindedResult = false;
	if(rq->svh->vh->status!=0){
		error->set(STATUS_SERVICE_UNAVAILABLE,"virtual host is closed");
		return NULL;
	}
	if (!rq->svh->bindFile(rq,rq->ctx->obj,result,htresponse,handled)) {
		//bind错误.如非法url.
		result = false;
		error->set(STATUS_SERVER_ERROR, "cann't bind file.");
		return NULL;
	}
	if (handled || rq->hasFinalFetchObject()) {
		//请求已经处理,或者数据源已确定.
		return NULL;
	}
	if (rq->file==NULL) {
		error->set(STATUS_SERVER_ERROR,"cann't bind local file. file is NULL.");
		return NULL;
	}
	if (result && rq->file->isPrevDirectory()) {
		//查找默认首页
		KFileName *newFile = NULL;
		indexFileFindedResult = rq->svh->vh->getIndexFile(rq,rq->file,&newFile,&indexPath);
		if (indexFileFindedResult) {
			delete rq->file;
			rq->file = newFile;
		}
	}
	redirect = rq->svh->vh->findPathRedirect(rq, rq->file,(indexPath?indexPath:rq->url->path), result,redirect_result);
	if (indexPath) {
		free(indexPath);
	}
	if (redirect) {
		//路径映射源确定
		return redirect;
	}
	if (redirect_result) {
		//路径映射源为空,但映射成功,意思就是用默认处理
		goto done;
	}
	if (result && rq->file->isDirectory()) {
		//文件为目录处理
		if (!rq->file->isPrevDirectory()) {
			//url后面不是以/结尾,重定向处理
			if(rq->meth == METH_GET){
				return new KPrevDirectoryFetchObject;
			}else{
				error->set(STATUS_METH_NOT_ALLOWED,"method not allowed");
				return NULL;
			}
			result = false;
			goto done;
		}
		//默认首页处理
		if (!indexFileFindedResult) {
			//没有查到默认首页
			if (rq->svh->vh->browse) {
				//如果允许浏览
				return new KDirectoryFetchObject;
			}
			error->set(STATUS_FORBIDEN,"You don't have permission to browse.");
			return NULL;
		}
	}
	//按文件扩展名查找扩展映射
	redirect = rq->svh->vh->findFileExtRedirect(rq, rq->file, result,redirect_result);
	if (redirect) {
		//映射源确定
		return redirect;
	}
	if(redirect_result){
		//映射源为空,但映射成功,意思就是用默认处理
		goto done;
	}
	//查找默认扩展
	redirect = rq->svh->vh->findDefaultRedirect(rq,rq->file,result);
	if(redirect){
		return redirect;
	}else if(result){
		if(rq->file->getPathInfoLength()>0){
			//静态文件不支持path_info
			result = false;
		}
	}
done:
	if (!result) {
		if(rq->ctx->obj->data->status_code==0){
			error->set(STATUS_NOT_FOUND, "No such file or directory.");
		}
		return NULL;
	} else if(redirect==NULL) {		
		redirect = new KStaticFetchObject;
	}
	return redirect;
}
/*
准备读文件，分捡请求
*/
kev_result stage_prepare(KHttpRequest *rq)
{
	assert(rq->ctx->obj);
	RequestError error;
	error.code = STATUS_SERVER_ERROR;
	error.msg = "internal error";
	if(!rq->hasFinalFetchObject()){
		if (rq->svh==NULL) {
#ifdef ENABLE_VH_RS_LIMIT
			return send_error(rq,NULL,STATUS_SERVER_ERROR,"access action error");
#else
			if (query_vh_success!=conf.gvm->queryVirtualHost(rq->c->ls,&rq->svh,rq->url->host)) {
				return send_error(rq,NULL,STATUS_BAD_REQUEST,"host not found.");
			}
#endif
		}
		KAccess *htresponse = NULL;
		bool handled = false;
		KFetchObject *fo = bindVirtualHost(rq,&error,&htresponse,handled);
		if (handled) {
			//已经处理了
			assert(fo==NULL);
			return kev_ok;
		}
		if (fo) {
			rq->appendFetchObject(fo);
		}
		//postmap
		if (htresponse) {
			if (!rq->ctx->internal && !rq->ctx->replace	&& htresponse->checkPostMap(rq,rq->ctx->obj)==JUMP_DENY) {
				delete htresponse;
				if (TEST(rq->filter_flags,RQ_SEND_AUTH)) {
					return send_auth(rq);
				}
				return handleError(rq,STATUS_FORBIDEN,"Deny by htaccess file");
			}
			delete htresponse;
		}
	}	
	if (!rq->ctx->internal && !rq->ctx->replace) {
		if (rq->svh) {
			if (rq->svh->vh->checkPostMap(rq)==JUMP_DENY) {
				if (TEST(rq->filter_flags,RQ_SEND_AUTH)) {
					return send_auth(rq);
				}
				return handleError(rq,STATUS_FORBIDEN,"Deny by vh postmap access");
			}
		}
		if (kaccess[RESPONSE].checkPostMap(rq,rq->ctx->obj)==JUMP_DENY) {
			if (TEST(rq->filter_flags,RQ_SEND_AUTH)) {
				return send_auth(rq);
			}
			return send_error(rq,NULL,STATUS_FORBIDEN,"Deny by global postmap access");
		}
	}
	if(!rq->hasFinalFetchObject()){
		return handleError(rq,error.code,error.msg);
	}
	return processRequest(rq);
}
kev_result asyncLoadHttpObject(KHttpRequest *rq) {
	
	KContext *context = rq->ctx;
	context->lastModified = 0;
	context->mt = modified_if_modified;
	if (rq->if_modified_since > 0) {
		context->lastModified = rq->if_modified_since;
		if (TEST(rq->flags,RQ_IF_RANGE_DATE)) {
			context->mt = modified_if_range_date;
		}
	} else if (context->if_none_match) {
		context->mt = modified_if_none_match;
		if (TEST(rq->flags, RQ_IF_RANGE_ETAG)) {
			context->mt = modified_if_range_etag;
		}
	} else if (context->old_obj	&& !TEST(context->old_obj->index.flags,OBJ_NOT_OK)) {
		if (context->old_obj->index.last_modified>0) {
			context->lastModified = context->old_obj->index.last_modified;
		} else if (TEST(context->old_obj->index.flags,OBJ_HAS_ETAG)) {
			KHttpHeader *h = context->old_obj->findHeader("Etag",sizeof("Etag")-1);
			if (h) {
				context->mt = modified_if_none_match;
				context->set_if_none_match(h->val,h->val_len);
			}
		} else {
			context->lastModified = context->obj->index.last_verified;
		}
	}
	return stage_prepare(rq);
}
inline int attr_tolower(const char p) {
	if (p=='-') {
		return '_';
	}
	return tolower(p);
}
int attr_casecmp(const char *s1,const char *s2)
{
	const unsigned char *p1 = (const unsigned char *) s1;
	const unsigned char *p2 = (const unsigned char *) s2;
	int result;
	if (p1 == p2)
		return 0;

	while ((result = attr_tolower (*p1) - attr_tolower(*p2++)) == 0)
		if (*p1++ == '\0')
			break;
	return result;
}
bool is_val(KHttpHeader *av, const char *val, int val_len)
{
	if (av->val_len != val_len) {
		return false;
	}
	return strncasecmp(av->val, val, val_len) == 0;
}
bool is_attr(KHttpHeader *av, const char *attr) {
	if (!av || !av->attr || !attr)
		return false;
	return attr_casecmp(av->attr, attr) == 0;
}
bool is_attr(KHttpHeader *av, const char *attr,int attr_len)
{
	assert(av && av->attr && attr);
	return attr_casecmp(av->attr, attr) == 0;
}
bool parse_url(const char *src, KUrl *url) {
	const char *ss, *se, *sx;
	//memset(url, 0, sizeof(KUrl));
	int p_len;
	if (*src == '/') {/* this is 'GET /path HTTP/1.x' request */
		sx = src;
		goto only_path;
	}
	ss = strchr(src, ':');
	if (!ss) {
		return false;
	}
	if (memcmp(ss, "://", 3)) {
		return false;
	}
	p_len = ss - src;
	if (p_len == 4 && strncasecmp(src, "http", p_len) == 0) {
		CLR(url->flags,KGL_URL_ORIG_SSL);
		url->port = 80;
	} else if (p_len == 5 && strncasecmp(src, "https", p_len) == 0) {
		SET(url->flags, KGL_URL_ORIG_SSL);
		url->port = 443;
	}
	//host start
	ss += 3;
	sx = strchr(ss, '/');
	if (sx == NULL) {
		return false;
	}
	p_len = 0;
	if(*ss == '['){
		ss++;
		se = strchr(ss,']');
		SET(url->flags, KGL_URL_IPV6);
		if(se && se < sx) {
			p_len = se - ss;
			se = strchr(se+1,':');
			if(se && se<sx){
				url->port = atoi(se + 1);
			}
		}
	}else{
		se = strchr(ss, ':');
		if(se && se<sx){
			p_len = se - ss;
			url->port = atoi(se + 1);
		}
	}
	if(p_len == 0){
		p_len = sx - ss;
	}
	url->host = (char *) malloc(p_len + 1);
	kgl_memcpy(url->host, ss, p_len);
	url->host[p_len] = 0;
	only_path: const char *sp = strchr(sx, '?');
	int path_len;
	if (sp) {
		//清理外部的varied url
		//varied url不能由外部传送,只能调用url->vary_url
		char *param = strdup(sp+1);
		char *varied_char = strchr(param,VARY_URL_KEY);
		if (varied_char) {
			*varied_char = '\0';
		}
		if (*param) {
			url->param = param;
		} else {
			free(param);
		}
		path_len = sp - sx;
	} else {
		path_len = strlen(sx);
	}
	url->path = (char *) xmalloc(path_len+1);
	url->path[path_len] = '\0';
	kgl_memcpy(url->path, sx, path_len);
	return true;
}
bool stageContentType(KHttpRequest *rq,KHttpObject *obj)
{
	//处理content-type
	char *content_type = rq->svh->vh->getMimeType(obj,rq->file->getExt());
	if (content_type==NULL) {
		content_type = conf.gvm->globalVh.getMimeType(obj,rq->file->getExt());
	}
	if (content_type==NULL) {
		return false;
	}
	obj->insertHttpHeader2(strdup("Content-Type"),sizeof("Content-Type")-1, content_type,strlen(content_type));
	return true;
}

bool make_http_env(KHttpRequest *rq, KBaseRedirect *brd,time_t lastModified,KFileName *file,KEnvInterface *env, bool chrooted) {
	size_t skip_length = 0;
	char tmpbuff[50];
	if (chrooted && rq->svh) {
		skip_length = rq->svh->vh->doc_root.size() - 1;
	}
	KHttpHeader *av = rq->GetHeader();
	while (av) {
#ifdef HTTP_PROXY
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
#endif
		if (TEST(rq->flags,RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}
		if (is_attr(av,"SCHEME")) {
			goto do_not_insert;
		}
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		env->addHttpHeader(av->attr, av->val);
		do_not_insert: av = av->next;
	}
	KStringBuf host;
	rq->url->GetHost(host);
	env->addHttpHeader("Host", host.getString());
	if (rq->content_length > 0) {
		env->addHttpHeader((char *)"Content-Length",(char *)int2string(rq->content_length,tmpbuff));
	}
	if (lastModified != 0) {
		mk1123time(lastModified, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			env->addHttpHeader((char *)"If-Range", (char *)tmpbuff);
		} else {
			env->addHttpHeader((char *)"If-Modified-Since", (char *)tmpbuff);
		}
	} else if (rq->ctx->if_none_match) {
		if (rq->ctx->mt == modified_if_range_etag) {
			env->addHttpHeader((char *)"If-Range",rq->ctx->if_none_match->data);
		} else {
			env->addHttpHeader((char *)"If-None-Match",rq->ctx->if_none_match->data);
		}
	}
	timeLock.Lock();
	env->addEnv("SERVER_SOFTWARE", conf.serverName);
	timeLock.Unlock();
	env->addEnv("GATEWAY_INTERFACE", "CGI/1.1");
	env->addEnv("SERVER_NAME", rq->url->host);
	env->addEnv("SERVER_PROTOCOL", "HTTP/1.1");
	env->addEnv("REQUEST_METHOD", rq->getMethod());
	char *param_buf = NULL;
	const char *param = rq->raw_url.getParam(&param_buf);
	if (param==NULL) {
		env->addEnv("REQUEST_URI",rq->raw_url.path);
		if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
			env->addEnv("HTTP_X_REWRITE_URL",rq->raw_url.path);
		}
	} else {
		KStringBuf request_uri;
		request_uri << rq->raw_url.path << "?" << param;
		env->addEnv("REQUEST_URI",request_uri.getString());
		if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
			env->addEnv("HTTP_X_REWRITE_URL",request_uri.getString());
		}
	}
	char *param2_buf = NULL;
	if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
		param = rq->url->getParam(&param2_buf);
	}
	if (param) {
		env->addEnv("QUERY_STRING", param);
	}
	if (param_buf) {
		free(param_buf);
	}
	if (param2_buf) {
		free(param2_buf);
	}
	/*
	SCRIPT_NAME和PATH_INFO区别
	/test.php/a

	SCIPRT_NAME = /test.php
	全PATH_INFO(isapi默认)
	PATH_INFO = /test.php/a
	部分PATH_INFO
	PATH_INFO = /a
	*/
	if (file) {
		unsigned pathInfoLength = file->getPathInfoLength();
		if(file->getIndex()) {
			//有index文件情况下。
			KStringBuf s;
			s << rq->url->path << file->getIndex();
			env->addEnv("SCRIPT_NAME", s.getString());
			if(TEST(rq->filter_flags,RQ_FULL_PATH_INFO)){
				env->addEnv("PATH_INFO",s.getString());
			}
		} else {		
			if(pathInfoLength>0){
				//有path info的情况下
				char *scriptName = (char *)xmalloc(pathInfoLength+1);
				kgl_memcpy(scriptName,rq->url->path,pathInfoLength);
				scriptName[pathInfoLength] = '\0';
				env->addEnv("SCRIPT_NAME",scriptName);
				xfree(scriptName);
				if(!TEST(rq->filter_flags,RQ_FULL_PATH_INFO)){
					env->addEnv("PATH_INFO",rq->url->path + pathInfoLength);
				}
			}else{
				env->addEnv("SCRIPT_NAME", rq->url->path);
			}
			if(TEST(rq->filter_flags,RQ_FULL_PATH_INFO)){
				env->addEnv("PATH_INFO",rq->url->path);
			}
		}
		if (skip_length < file->getNameLen()) {
			if(pathInfoLength>0){
				KStringBuf s;
				s << file->getName() + skip_length << rq->url->path + pathInfoLength;
				env->addEnv("PATH_TRANSLATED",s.getString());
			}else{
				env->addEnv("PATH_TRANSLATED", file->getName() + skip_length);
			}
			env->addEnv("SCRIPT_FILENAME", file->getName() + skip_length);
		}
	}
	sockaddr_i self_addr;
	rq->sink->GetSelfAddr(&self_addr);
	char ips[MAXIPLEN];
	ksocket_sockaddr_ip(&self_addr, ips, sizeof(ips));
	env->addEnv("SERVER_ADDR", ips);
	env->addEnv("SERVER_PORT", rq->raw_url.port);
	env->addEnv("REMOTE_ADDR", rq->getClientIp());	
	env->addEnv("REMOTE_PORT", ksocket_addr_port(rq->sink->GetAddr()));
	if (rq->svh) {
		env->addEnv("DOCUMENT_ROOT", rq->svh->doc_root + skip_length);
		env->addEnv("VH_NAME", rq->svh->vh->name.c_str());
	}
#ifdef KSOCKET_SSL
	if (TEST(rq->raw_url.flags,KGL_URL_SSL)) {
		env->addEnv("HTTPS", "ON");
		kssl_session *ssl = rq->sink->GetSSL();
		if (ssl) {
#ifdef SSL_READ_EARLY_DATA_SUCCESS
			if (ssl->in_early) {
				env->addEnv("SSL_EARLY_DATA", "1");
			}
#endif
			make_ssl_env(env, ssl->ssl);
		}
	}
#endif
	
	return env->addEnvEnd();

}
/*
同步处理
*/
KTHREAD_FUNCTION stage_sync(void *param)
{	
	KHttpRequest *rq = (KHttpRequest *)param;
	kassert(rq->fetchObj->isSync());
	//请求时间重新计时
	rq->fetchObj->open(rq);
	KTHREAD_RETURN;	
}
int checkResponse(KHttpRequest *rq,KHttpObject *obj)
{
	if (TEST(rq->GetWorkModel(), WORK_MODEL_MANAGE) || rq->ctx->response_checked || rq->ctx->skip_access) {
		return JUMP_ALLOW;
	}
	rq->ctx->response_checked = 1;
	int action = kaccess[RESPONSE].check(rq,obj);
#ifndef HTTP_PROXY
#ifdef ENABLE_USER_ACCESS
	if (action == JUMP_ALLOW && rq->svh) {
		action = rq->svh->vh->checkResponse(rq);
	}
#endif
#endif
	if (action == JUMP_DENY) {
		KMutex *lock = obj->getLock();
		lock->Lock();
		if (obj->refs == 1) {
			destroy_kbuf(obj->data->bodys);
			obj->data->bodys = NULL;
			set_obj_size(obj, 0);
		}
		lock->Unlock();
	}
	return action;
}
kev_result afterPostHandleForUpstream(KHttpRequest *rq,void *arg)
{
	return processQueueRequest(rq);
}
