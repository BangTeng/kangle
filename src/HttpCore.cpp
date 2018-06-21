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
#include "KHttpObjectParserHook.h"
#include "KHttpProtocolParser.h"
#include "KHttpObjectHash.h"
#include "KHttpKeyValue.h"
#include "KObjectList.h"
#include "KHttpTransfer.h"
#include "KSendable.h"
#include "KUpstreamSelectable.h"
#include "KThreadPool.h"
#include "lang.h"
#include "ssl_utils.h"
#include "KRequestQueue.h"
#include "malloc_debug.h"
#include "time_utils.h"
#include "KVirtualHostManage.h"
#include "KSelector.h"
#include "KDirectoryFetchObject.h"
#include "KStaticFetchObject.h"
#include "KSyncFetchObject.h"
#include "KCacheFetchObject.h"
#include "KConcatFetchObject.h"
#include "KRewriteMarkEx.h"
#include "KUrlParser.h"
#include "KHttpFilterManage.h"
#include "KHttpProxyFetchObject.h"
#include "KSimulateRequest.h"
#include "KHttpObjectSwaping.h"

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
buff *inflate_buff(buff *in_buf, INT64 &len, bool fast) {
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
//compress buff
buff *deflate_buff(buff *in_buf, int level, INT64 &len, bool fast) {
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
/**
注意返回意义，true为切换了thread.
false没有切换
所以现阶段，总是返回true.
*/
bool send_http(KHttpRequest *rq, KHttpObject *obj, int code, KReadWriteBuffer *body) {
	rq->closeFetchObject();
	//rq->status_code = code;
	if (obj) {
		obj->data->status_code = code;
	}
	
#ifdef ENABLE_TF_EXCHANGE
	rq->closeTempFile();
#endif
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return true;
	}
	rq->responseStatus(code);
	if (rq->auth) {
		rq->auth->insertHeader(rq);
	}
	rq->responseHeader(kgl_response_server,conf.serverName,conf.serverNameLength);
	timeLock.Lock();
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
	} else if (!is_status_code_no_body(code)) {
		send_len = (body ? body->getLen() : 0);
		rq->responseHeader(kgl_expand_string("Content-Length"), (int)send_len);
	}
	rq->responseConnection();
	rq->startResponseBody(send_len);
	stageWriteRequest(rq);
	return true;	
}
bool send_auth(KHttpRequest *rq,KReadWriteBuffer *body) {
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return true;
	}
	rq->status_code = STATUS_UNAUTH;
	return send_http(rq, NULL, rq->status_code, body);
}
/**
* 发送错误信息
*/
bool send_error(KHttpRequest *rq, KHttpObject *obj, int code,const char* reason) 
{
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER) || rq->ctx->read_huped) {
		if (!TEST(rq->flags,RQ_SYNC)) {
			stageEndRequest(rq);
		}
		return true;
	}
#ifdef ENABLE_SIMULATE_HTTP
	if (TEST(rq->workModel,WORK_MODEL_SIMULATE)) {
		KSimulateSocket *ss = static_cast<KSimulateSocket *>(rq->c->socket);
		ss->sendError(code,reason);
		stageEndRequest(rq);
		return true;
	}
#endif

	rq->buffer.clean();
	KReadWriteBuffer &s = rq->buffer;
	if (rq->meth==METH_HEAD) {
		return send_http(rq, obj, code, &s);
	}
	assert(rq);
	assert(rq->c->socket);
	KStringBuf event_id(32);
	if (conf.log_event_id) {
		event_id.add(rq->begin_time_msec, INT64_FORMAT_HEX);
		event_id.WSTR("-");
		event_id.add((INT64)rq, INT64_FORMAT_HEX);
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
		rq->c->socket->get_self_addr(&addr);
		char ip[MAXIPLEN];
		KSocket::make_ip(&addr,ip,sizeof(ip));
		s << ip << ":" << addr.get_port();
	}
	s << "(";
	s.write_all(conf.serverName,conf.serverNameLength);
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
#ifdef ENABLE_SIMULATE_HTTP
	if (TEST(rq->workModel,WORK_MODEL_SIMULATE)) {
		KSimulateSocket *ss = static_cast<KSimulateSocket *>(rq->c->socket);
		if (build_first) {
			rq->status_code = obj->data->status_code;
		}
		ss->sendHeader(rq->status_code,obj->data->headers);
		SET(rq->flags, RQ_HAS_SEND_HEADER);
		return true;
	}
#endif
	if (build_first) {
		uint16_t status_code = obj->data->status_code;
		if (TEST(rq->raw_url.flags,KGL_URL_RANGED) && rq->status_code==STATUS_CONTENT_PARTIAL) {
			//如果请求是url模拟range，则强制转换206的回应为200
			status_code = STATUS_OK;
		}
		rq->responseStatus(status_code);
	}
	if (TEST(obj->index.flags,ANSW_LOCAL_SERVER)) {
		rq->responseHeader(kgl_response_server,conf.serverName,conf.serverNameLength);
		timeLock.Lock();
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
	if (rq->cookie_stick>0) {
		//设置cookie粘住cookie
		KStringBuf b;
		if (*conf.cookie_stick_name) {
			b << conf.cookie_stick_name;
		} else {
			b.WSTR(DEFAULT_COOKIE_STICK_NAME);
		}
		b.WSTR("=");
		b << rq->cookie_stick;
		b.WSTR("; path=/");
		rq->responseHeader(kgl_expand_string("Set-Cookie"),b.getBuf(),b.getSize());
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
			} else if (!rq->ctx->connection_upgrade						
	
			) {
				rq->responseHeader(kgl_expand_string("Transfer-Encoding"),kgl_expand_string("chunked"));
				SET(rq->flags,RQ_TE_CHUNKED);
			}
		}
	}
	rq->responseConnection();
	rq->startResponseBody(send_len);
	return true;
}
//试图发送，如果rq->buffer有数据就发送,并返回true，否则返回false
bool try_send_request(KHttpRequest *rq)
{
	
	if (rq->buffer.getLen() > 0) {
		stageWriteRequest(rq);
		return true;
	}
	return false;
}
void stage_rdata_end(KHttpRequest *rq,StreamState result)
{
	
	rq->closeFetchObject();
	
	KHttpObject *obj = rq->ctx->obj;
	if (rq->ctx->upstream_chunked && result != STREAM_WRITE_END) {
		//上游是chunked数据，又未正常结束
		assert(!rq->ctx->upstream_expected_done);
		SET(obj->index.flags, FLAG_DEAD | OBJ_INDEX_UPDATE);
		result = STREAM_WRITE_FAILED;
	}
	if (result == STREAM_WRITE_FAILED) {
		if (obj->data->type==MEMORY_OBJECT) {
			SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
			SET(rq->flags,RQ_CONNECTION_CLOSE);
		}
		delete rq->ctx->st;
		rq->ctx->st = NULL;
		if(TEST(rq->filter_flags,RQ_RESPONSE_DENY) && !TEST(rq->flags,RQ_HAS_SEND_HEADER)){
			send_error(rq,NULL,STATUS_FORBIDEN,"access denied by response control");
			return;
		}
		stageEndRequest(rq);
		return;
	}
	if (rq->ctx->know_length && rq->ctx->left_read != 0 && obj->data->type==MEMORY_OBJECT) {
		//	printf("content 没读完[%s],总共有[%d],剩下[%d]\n",rq->getInfo().c_str(),obj->index.content_length,left_read);
		SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);	
	}
	assert(rq->ctx->st);
	result = rq->ctx->st->write_end();
	delete rq->ctx->st;
	rq->ctx->st = NULL;
	if (result == STREAM_WRITE_FAILED) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
	}
	if (TEST(rq->filter_flags,RQ_RESPONSE_DENY) && !TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		send_error(rq,NULL,STATUS_FORBIDEN,"access denied by response control");
		return;
	}
	if (!try_send_request(rq)) {
#ifdef  ENABLE_BIG_OBJECT
		if (rq->bo_ctx) {
			rq->bo_ctx->write_request_end(rq);
			return;
		}
#endif
		if (rq->send_ctx.getBufferSize()>0) {
				stageWriteRequest(rq);
				return;
		}
		stageEndRequest(rq);
	}
}
/**
异步load body准备
*/
void prepare_load_body(KHttpRequest *rq)
{
	assert(rq->ctx->st == NULL);
	bool autoDelete = true;	
	KHttpTransfer *tr = new KHttpTransfer(rq,rq->ctx->obj);
	rq->ctx->st = makeWriteStream(rq, rq->ctx->obj, tr, autoDelete);
	assert(rq->buffer.getLen()==0);
	assert(rq->fetchObj);
	assert(rq->ctx->st);
	if (rq->ctx->connection_upgrade
#ifdef WORK_MODEL_TCP
		&& !TEST(rq->workModel,WORK_MODEL_TCP)
#endif
		) {
		tr->sendHead(false);
		stageWriteRequest(rq);
		return;
	}
	rq->fetchObj->readBody(rq);
}
/**
* 同步load body
*/
bool sync_load_body(KHttpRequest *rq, KHttpObject *obj) {
	kassert(TEST(rq->flags,RQ_SYNC));
	//assert(rq->op == STAGE_OP_SYNC);
	KSyncFetchObject *fo = static_cast<KSyncFetchObject *>(rq->fetchObj);
	assert(fo);
	StreamState result = STREAM_WRITE_FAILED;
	bool autoDelete = false;
	KHttpTransfer tr(rq, obj);
	KWStream *st = makeWriteStream(rq, obj, &tr, autoDelete);
	int r = READ_PROTOCOL_ERROR;
	char answer[1024];
	int read_len = sizeof(answer);
	assert(rq->ctx->left_read == 0);
	assert(rq->ctx->know_length==false);
	if (TEST(obj->index.flags,ANSW_HAS_CONTENT_LENGTH) && !rq->ctx->connection_upgrade) {
		rq->ctx->know_length = true;
		rq->ctx->left_read = obj->index.content_length;
		rq->ctx->left_read -= rq->buffer.getLen();
		if (rq->ctx->left_read < (INT64) sizeof(answer)) {
			read_len = (int) rq->ctx->left_read;
		}
	}
	
	if (rq->buffer.getLen() > 0) {
		buff *buf = rq->buffer.stealBuff();
		assert(buf);
		result = send_buff<KWStream>(st,buf);
		KBuffer::destroy(buf);
		if (result == STREAM_WRITE_FAILED) {
			goto error;
		}
		if (result == STREAM_WRITE_END) {
			goto done;
		}
	}
	for (;;) {
		if (read_len <= 0) {
			//	printf("读完数据了\n");
			goto done;
		}
		r = fo->read(answer, read_len);
		if (r <= 0) {
			goto done;
		}
		result = st->write_all(answer, r);
		switch (result) {
		case STREAM_WRITE_END:
			goto done;
		case STREAM_WRITE_FAILED:
			goto error;
		default:
			break;
		}
		if (rq->ctx->know_length) {
			rq->ctx->left_read -= r;
			read_len = (int) MIN((INT64)sizeof(answer),rq->ctx->left_read);
		}
	}
	done: 
	if (TEST(obj->index.flags,ANSW_CHUNKED) && result != STREAM_WRITE_END) {
		SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
	}
	if (rq->ctx->know_length && rq->ctx->left_read!=0 && obj->data->type==MEMORY_OBJECT) {
		//	printf("content 没读完[%s],总共有[%d],剩下[%d]\n",rq->getInfo().c_str(),obj->index.content_length,left_read);
		SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
	}
	result = st->write_end();
	if (autoDelete) {
		delete st;
	}
	return result != STREAM_WRITE_FAILED;
	error: if (autoDelete) {
		delete st;
	}
	SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
	return false;
}

bool push_redirect_header(KHttpRequest *rq, const char *url,int url_len,int code) {
	if (code==0) {
		code = STATUS_FOUND;
	}
	if (TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		return false;
	}
	rq->responseStatus(code);
	rq->responseHeader(kgl_response_server, conf.serverName, conf.serverNameLength);
	timeLock.Lock();
	rq->responseHeader(kgl_response_date, (char *)cachedDateTime, 29);
	timeLock.Unlock();
	rq->responseHeader(kgl_expand_string("Location"),url,url_len);
	rq->responseHeader(kgl_expand_string("Content-Length"), 0);
	rq->responseConnection();
	return true;
}
bool send_not_modify_from_mem(KHttpRequest *rq) {
	return send_http(rq, NULL, STATUS_NOT_MODIFIED);
}
void processCacheReadyRequest(KHttpRequest *rq,KHttpObject *obj, swap_in_result result) {
	switch (result) {
	case swap_in_busy:
		klog(KLOG_ERR,"obj swap in busy drop request.\n");
		send_error(rq, NULL, STATUS_SERVER_ERROR, "swap in busy");
		//stageEndRequest(rq);
		break;
	case swap_in_failed:
	{
#ifdef ENABLE_DISK_CACHE
		//不能swap in就从源上去取
		char *filename = obj->getFileName();
		klog(KLOG_ERR, "obj swap in failed cache file [%s].\n", filename);
		free(filename);
#endif
		rq->ctx->clean_obj(rq);
		rq->ctx->new_object = true;
		rq->ctx->lastModified = 0;
		rq->ctx->obj = new KHttpObject(rq);
		if (rq->file) {
			delete rq->file;
			rq->file = NULL;
		}
		assert(rq->fetchObj || TEST(rq->workModel, WORK_MODEL_MANAGE) || rq->svh);
		rq->resetFetchObject();
		asyncLoadHttpObject(rq);
		break;
	}
	case swap_in_success:
	{
		KHttpObject *obj = rq->ctx->obj;
		rq->ctx->cache_hit = true;
		
		processCacheWithCheckExpire(rq, obj);
		break;
	}
	}
}
void processCacheRequest(KHttpRequest *rq) {
	KHttpObject *obj = rq->ctx->obj;
	if (!TEST(obj->index.flags,FLAG_IN_MEM)) {
		KMutex *lock = obj->getLock();
		//rq->c->removeRequest(rq,true);
		lock->Lock();
		if (obj->data==NULL) {
#ifdef ENABLE_DISK_CACHE
			KHttpObjectSwaping *obj_swap = new KHttpObjectSwaping;
			obj->data = new KHttpObjectBody();
			obj->data->type = SWAPING_OBJECT;
			obj->data->os = obj_swap;
			obj->data->os->addTask(rq,processCacheReadyRequest);
			lock->Unlock();
			obj_swap->swapin(rq,obj);
			return;
#else
			lock->Unlock();
			SET(obj->index.flags,FLAG_DEAD);
			klog(KLOG_ERR,"BUG!! obj is not in memory.");
			assert(false);
			stageEndRequest(rq);
			return;
#endif
#ifdef ENABLE_DISK_CACHE
		} else if(obj->data->type == SWAPING_OBJECT) {
			//已经有其它线程在swap
			KHttpObjectSwaping *os = obj->data->os;
			assert(os);
			os->addTask(rq,processCacheReadyRequest);
			lock->Unlock();
			return;
#endif
		}
		lock->Unlock();		
	}
	processCacheReadyRequest(rq,obj,swap_in_success);
}
/**
* 发送在内存中的object.
*/
void sendMemoryObject(KHttpRequest *rq,KHttpObject *obj)
{

	rq->closeFetchObject();
	//内部子请求
	if (rq->sr) {
		assert(rq->fetchObj==NULL);
		rq->fetchObj = new KCacheFetchObject(obj);
		rq->fetchObj->open(rq);
		return;
	}
	if (TEST(obj->index.flags, OBJ_CACHE_RESPONSE)) {		
		if (!TEST(rq->workModel, WORK_MODEL_MANAGE | WORK_MODEL_INTERNAL | WORK_MODEL_SKIP_ACCESS)
			&& checkResponse(rq, obj) == JUMP_DENY) {
			handleError(rq, STATUS_FORBIDEN, "access denied by response control");
			return;
		}
	}
	if (obj->data->type==MEMORY_OBJECT && !obj->isNoBody(rq)) {
		if (rq->needFilter()) {
			if (rq->ctx->st == NULL) {
				bool autoDelete = true;
				KHttpTransfer *tr = new KHttpTransfer(rq,obj);
				rq->ctx->st = makeWriteStream(rq,obj,tr,autoDelete);			
			}
			rq->fetchObj = new KCacheFetchObject(obj);
			rq->fetchObj->open(rq);
			return;
		}
		//assert(!TEST(obj->index.flags,OBJ_IS_DELTA));
		assert(!TEST(obj->index.flags,ANSW_CHUNKED));
	}
	buff *send_buffer = obj->data->bodys;
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
	
	stageWriteRequest(rq,send_buffer,(int)start,(int)send_len);
	return;
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
		assert(rq->fetchObj || TEST(rq->workModel,WORK_MODEL_MANAGE) || rq->svh);
		rq->resetFetchObject();
		asyncLoadHttpObject(rq);
		return;
	}
	sendMemoryObject(rq,rq->ctx->obj);
}
/*
异步发送物件
rq->filter_flags=RQ_SWAP_OLD_OBJ则发送rq->ctx->old_obj
否则就发送rq->ctx->obj
同步函数为:sendHttpObject,完成相同的功能
*/
bool asyncSendHttpObject(KHttpRequest *rq)
{
	
	kassert(!TEST(rq->flags,RQ_SYNC));
	if (TEST(rq->filter_flags,RQ_SWAP_OLD_OBJ)) {
		rq->ctx->popObj();
		CLR(rq->filter_flags,RQ_SWAP_OLD_OBJ);
	}
	sendMemoryObject(rq,rq->ctx->obj);
	return true;
}
bool sendHttpObject(KHttpRequest *rq, KHttpObject *obj) {
	//assert(rq->op == STAGE_OP_SYNC);
	buff * send_buffer = NULL;//obj->container;
	buff *hdrs_to_send = NULL;//alloc_buff(CHUNK_SIZE); /* most headers fit this (?) */
	//bool send_unknow_length = false;
	INT64 send_len = 0;//obj->index.content_length;
	INT64 start = 0;
	unsigned this_send_len = 0;
	//INT64 body_size = 0;
	//	bool chunked_and_http_1_0=false;
	bool result = true;
	//	status_code = obj->data->status_code;
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
			SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
			lock->Unlock();
			cache.dead(obj, __FILE__, __LINE__);
			//objList[obj->list_state].dead(obj);
			return send_error(rq, NULL, STATUS_SERVER_ERROR,"Cann't swap in object!");
		}
		SET(obj->index.flags,FLAG_IN_MEM);
		
			cache.getHash(obj->h)->incSize(obj->index.content_length);		
	}
#endif	
	send_buffer = obj->data->bodys;
	content_len = obj->index.content_length;
	lock->Unlock();
	send_len = content_len;
	KBuffer headerBuffer(2048);
	build_obj_header(rq, obj, content_len,start, send_len);
	hdrs_to_send = headerBuffer.stealBuffFast();
	result = send_buff(rq, hdrs_to_send) == STREAM_WRITE_SUCCESS;
	KBuffer::destroy(hdrs_to_send);
	if (TEST(obj->index.flags,FLAG_NO_BODY) || rq->meth == METH_HEAD || !result
			|| start == -1) {
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
		//if (!send_unknow_length) {
		if (send_len <= 0){
				break;
		}
		if (send_len > (INT64) send_buffer->used - start) {
			this_send_len = send_buffer->used - (unsigned) start;
		} else {
			this_send_len = (int) send_len;
		}
		if (!rq->write_all(send_buffer->data + start, this_send_len)) {
			result = false;
			break;
		}
		//rq->send_size += this_send_len;
		start = 0;
		//if (!send_unknow_length){
		send_len -= this_send_len;
		//}
		next_buffer: send_buffer = send_buffer->next;
	}
	if (result) {
		assert(send_len==0);
	}
	return result;
}
void stageError(KHttpRequest *rq,int code,const char *msg)
{
	int sync = TEST(rq->flags,RQ_SYNC);
	send_error(rq,NULL,STATUS_FORBIDEN,"access denied by response control");
	if (sync) {
		stageEndRequest(rq);
	}
}
void handleXSendfile(KHttpRequest *rq)
{
	if (rq->ctx->st || TEST(rq->flags,RQ_HAS_SEND_HEADER)) {
		send_error(rq,NULL,STATUS_SERVER_ERROR,"X-Accel-Redirect cann't send body");
		return;
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
		stageEndRequest(rq);
		return;
	}
	if (!rq->rewriteUrl(xurl,0)) {
		send_error(rq,NULL,STATUS_SERVER_ERROR,"X-Accel-Redirect value is not right");
		return;
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
		SET(rq->workModel, WORK_MODEL_INTERNAL | WORK_MODEL_REPLACE);
	}
	async_http_start(rq);
}
//上游无http body的处理
void handleUpstreamNoBody(KHttpRequest *rq)
{
	rq->fetchObj->readBodyEnd(rq);
#ifdef ENABLE_TF_EXCHANGE
	rq->closeTempFile();
#endif	
	
	if (!TEST(rq->filter_flags,RQ_SWAP_OLD_OBJ) && TEST(rq->ctx->obj->index.flags,ANSW_XSENDFILE)) {
		handleXSendfile(rq);
		return;
	}
	rq->closeFetchObject(false);
	if (TEST(rq->flags,RQ_SYNC)) {
		//同步发送
		if(!sendHttpObject(rq, TEST(rq->filter_flags,RQ_SWAP_OLD_OBJ)?rq->ctx->old_obj:rq->ctx->obj)){
			SET(rq->flags,RQ_CONNECTION_CLOSE);
		}
	} else {
		//异步发送
		asyncSendHttpObject(rq);
	}
}
void handleUpstreamRecvedHead(KHttpRequest *rq)
{
#ifdef ENABLE_TF_EXCHANGE
	if (!TEST(rq->workModel,WORK_MODEL_INTERNAL)) {
		if (!TEST(rq->filter_flags,RF_NO_BUFFER) &&
			rq->fetchObj->needTempFile()) {
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
	context->us_code = obj->data->status_code;
	if (status_code != STATUS_OK && status_code != STATUS_CONTENT_PARTIAL) {
		SET(obj->index.flags,ANSW_NO_CACHE|OBJ_INDEX_UPDATE|OBJ_NOT_OK);
	}
	if (!TEST(rq->workModel, WORK_MODEL_MANAGE|WORK_MODEL_INTERNAL|WORK_MODEL_SKIP_ACCESS) 
		&& checkResponse(rq,obj) == JUMP_DENY) {
		if (rq->send_ctx.getBufferSize()>0 || rq->buffer.getLen()>0) {
			
			rq->closeFetchObject();
			rq->startResponseBody(-1);
			stageWriteRequest(rq);
		} else {
			stageError(rq,STATUS_FORBIDEN,"access denied by response control");
		}
		return;
	}
	obj->checkNobody();
	//printf("load head status=%d\n", obj->data->status_code);
	switch (status_code) {
	case STATUS_NOT_MODIFIED:
		SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
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
		handleUpstreamNoBody(rq);
		return;
	default:
		rq->ctx->cache_hit = false;
		if (rq->meth == METH_HEAD || TEST(context->obj->index.flags,FLAG_NO_BODY)) {
			//没有http body的情况
			SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
			CLR(obj->index.flags,ANSW_CHUNKED);
			rq->ctx->no_body = true;
			handleUpstreamNoBody(rq);
			return;
		}
		if (TEST(rq->ctx->obj->index.flags,ANSW_HAS_CONTENT_LENGTH) && !rq->ctx->connection_upgrade) {
			rq->ctx->know_length = true;
			rq->ctx->left_read = rq->ctx->obj->index.content_length;
		} else {
			rq->ctx->know_length = false;
			rq->ctx->left_read = -1;
		}
		
		if (status_code==STATUS_CONTENT_PARTIAL) {
			//强行设置206不缓存
			SET(obj->index.flags,ANSW_NO_CACHE|OBJ_INDEX_UPDATE|OBJ_NOT_OK);
		} else {
			CLR(rq->flags,RQ_HAVE_RANGE);
		}
		//load http body
		if (!TEST(rq->flags,RQ_SYNC)) {
			//异步情况
			prepare_load_body(rq);
			return;
		}
		if (!sync_load_body(rq, obj)) {
			goto error;
		}	
	}
	rq->closeFetchObject();
	return ;
error:
	rq->closeFetchObject();
	SET(rq->flags,RQ_CONNECTION_CLOSE);
	return ;
}

void handleError(KHttpRequest *rq,int code,const char *msg) {
	
	if (TEST(rq->filter_flags,RF_ALWAYS_ONLINE)) {
		//always on
		if (rq->ctx->old_obj) {
			//have cache
			rq->ctx->always_on_model = true;
			rq->ctx->popObj();
			if (JUMP_DENY == checkResponse(rq,rq->ctx->obj)) {
				send_error(rq,NULL,STATUS_FORBIDEN,"denied by response access");
				return;
			}
			async_send_valide_object(rq,rq->ctx->obj);
			return;
		} else if (TEST(rq->flags,RQ_HAS_IF_MOD_SINCE|RQ_HAS_IF_NONE_MATCH)) {
			//treat as not-modified
			send_not_modify_from_mem(rq);
			return;
		}
	}
	if (rq->svh==NULL || code<403 || code>499) {
		send_error(rq,NULL,code,msg);
		return;
	}
	KHttpObject *obj = rq->ctx->obj;
	obj->data->status_code = code;
	if(TEST(rq->flags,RQ_IS_ERROR_PAGE)){
		//如果本身是错误页面，又产生错误
		send_error(rq,NULL,code,msg);
		return;
	}
	//设置为错误页面
	SET(rq->flags,RQ_IS_ERROR_PAGE);
	//清除range请求
	CLR(rq->flags,RQ_HAVE_RANGE);
	assert(rq->svh);
	string errorPage2;
	if (!rq->svh->vh->getErrorPage(code, errorPage2)) {
		send_error(rq,NULL,code,msg);
		return;
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
		stageWriteRequest(rq);
		return;
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
	//SET(rq->flags,ANSW_IS_ERROR_PAGE);
	//assert(redirect==NULL);
	if (result && !rq->file->isDirectory()) {
		bool redirect_result;
		rq->closeFetchObject();
		rq->fetchObj = rq->svh->vh->findFileExtRedirect(rq, rq->file, true,redirect_result);
		if (rq->fetchObj==NULL) {
			if (rq->svh->vh->concat && rq->url->param && *rq->url->param=='?') {
				rq->fetchObj = new KConcatFetchObject;
			} else {
				rq->fetchObj = new KStaticFetchObject;
			}
		}
		processRequest(rq);
		return;
	}
	send_error(rq,NULL,code,msg);
	return;
	//*/
}
KFetchObject *bindVirtualHost(KHttpRequest *rq,RequestError *error,KAccess **htresponse,bool &handled) {
	assert(rq->file==NULL);
	//file = new KFileName;
	assert(rq->fetchObj==NULL);
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
		goto done;
	}
	if (handled || rq->fetchObj) {
		//请求已经处理,或者数据源已确定.
		return NULL;
	}
	if (rq->file==NULL) {
		error->set(STATUS_SERVER_ERROR,"cann't bind file");
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
		if (rq->svh->vh->concat && rq->url->param && *rq->url->param=='?') {
			redirect = new KConcatFetchObject;
		} else {
			redirect = new KStaticFetchObject;
		}
	}
	return redirect;
}
/*
准备读文件，分捡请求
*/
void stage_prepare(KHttpRequest *rq)
{
	assert(rq->ctx->obj);
	RequestError error;
	if(rq->fetchObj == NULL){
		if (rq->svh==NULL) {
#ifdef ENABLE_VH_RS_LIMIT
			send_error(rq,NULL,STATUS_SERVER_ERROR,"access action error");
			return;
#else
			if (query_vh_success!=conf.gvm->queryVirtualHost(rq->c->ls,&rq->svh,rq->url->host)) {
				send_error(rq,NULL,STATUS_BAD_REQUEST,"host not found.");
				return;
			}
#endif
		}
		KAccess *htresponse = NULL;
		bool handled = false;
		KFetchObject *fo = bindVirtualHost(rq,&error,&htresponse,handled);
		if (handled) {
			//已经处理了
			assert(fo==NULL);
			return;
		}
		if (fo) {
			assert(rq->fetchObj==NULL);
			rq->fetchObj = fo;
		}
		//postmap
		if (htresponse) {
			if (!TEST(rq->workModel, WORK_MODEL_MANAGE|WORK_MODEL_INTERNAL|WORK_MODEL_SKIP_ACCESS) 
				&& htresponse->checkPostMap(rq,rq->ctx->obj)==JUMP_DENY) {
				delete htresponse;
				if (TEST(rq->filter_flags,RQ_SEND_AUTH)) {
					send_auth(rq);
					return;
				}
				handleError(rq,STATUS_FORBIDEN,"Deny by htaccess file");
				return;
			}
			delete htresponse;
		}
	}	
	if (!TEST(rq->workModel, WORK_MODEL_MANAGE|WORK_MODEL_INTERNAL|WORK_MODEL_SKIP_ACCESS)) {
		if (rq->svh) {
#ifdef ENABLE_KSAPI_FILTER
			if (rq->svh->vh->hfm 
				&& !rq->svh->vh->hfm->check_urlmap(rq)) {
				return;
			}
	#endif
			if (rq->svh->vh->checkPostMap(rq)==JUMP_DENY) {
				if (TEST(rq->filter_flags,RQ_SEND_AUTH)) {
					send_auth(rq);
					return;
				}
				handleError(rq,STATUS_FORBIDEN,"Deny by vh postmap access");
				return;
			}
		}
#ifdef ENABLE_KSAPI_FILTER
		if (conf.gvm->globalVh.hfm 
			&& !conf.gvm->globalVh.hfm->check_urlmap(rq)) {
			return;
		}
#endif
		if (kaccess[RESPONSE].checkPostMap(rq,rq->ctx->obj)==JUMP_DENY) {
			if (TEST(rq->filter_flags,RQ_SEND_AUTH)) {
				send_auth(rq);
				return;
			}
			send_error(rq,NULL,STATUS_FORBIDEN,"Deny by global postmap access");
			return;
		}
	}
	if(rq->fetchObj==NULL){
		handleError(rq,error.code,error.msg);
		return;
	}
	if (TEST(rq->filter_flags,RQ_NO_EXTEND) && !TEST(rq->flags,RQ_IS_ERROR_PAGE)) {
		//无扩展处理
		if (rq->fetchObj->needQueue()) {
			rq->closeFetchObject();
			assert(rq->fetchObj==NULL);
			rq->fetchObj = new KStaticFetchObject();
		}
	}
	processRequest(rq);
}
void asyncLoadHttpObject(KHttpRequest *rq) {
	
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
	stage_prepare(rq);
	return;
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
	memcpy(url->host, ss, p_len);
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
	memcpy(url->path, sx, path_len);
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
/*
 这个函数是webdav的辅助函数。
 检查webdav的destination头，把destination解析，别名映射检查，并检查扩展映射。
 如果成功，则会把注册变量DAV_DESTINATION，映射到物理地址上。
 */
bool make_webdav_destination_env(KHttpRequest *rq, KRedirect *rd,
		KEnvInterface *env, bool chrooted) {
	bool make_result = false;
	size_t skip_length = 0;
	if (chrooted && rq->svh) {
		skip_length = rq->svh->vh->doc_root.size() - 1;
	}
	KHttpHeader *av = rq->parser.getHeaders();
	while (av) {
		if (is_attr(av, "Destination")) {
			KUrl url;
			memset(&url, 0, sizeof(url));
			char *val = xstrdup(av->val);
			url_decode(val, 0, &url);
			if (parse_url(val, &url)) {
				KFileName file;
				char *tripedDir = file.tripDir2(url.path, PATH_SPLIT_CHAR);
				char *new_path = rq->svh->vh->alias(TEST(rq->workModel,WORK_MODEL_INTERNAL)>0,tripedDir);
				//bool result;
				if (new_path) {
					file.giveName(new_path);
				} else {
					file.setName(rq->svh->vh->doc_root.c_str(),
							tripedDir, rq->getFollowLink());
				}
				xfree(tripedDir);
				if (rq->svh->vh->isPathRedirect(rq, &file, true, rd)) {
					//todo register env
					env->addEnv("DAV_DESTINATION", file.getName() + skip_length);
					make_result = true;
				}
			}
			url.destroy();
			xfree(val);
			break;
		}
		av = av->next;
	}
	return make_result;
}
bool make_http_env(KHttpRequest *rq, KBaseRedirect *brd,time_t lastModified,KFileName *file,KEnvInterface *env, bool chrooted) {
	size_t skip_length = 0;
	char tmpbuff[50];
	if (chrooted && rq->svh) {
		skip_length = rq->svh->vh->doc_root.size() - 1;
	}
	KHttpHeader *av = rq->parser.getHeaders();
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
	//env->addHttpHeader("Connection",
	//		(TEST(rq->flags,RQ_HAS_KEEP_CONNECTION) ? "Keep-Alive" : "close"));
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
	env->addEnv("SERVER_SOFTWARE", conf.serverName);
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
				memcpy(scriptName,rq->url->path,pathInfoLength);
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
	rq->c->socket->get_self_addr(&self_addr);
	char ips[MAXIPLEN];
	KSocket::make_ip(&self_addr,ips,sizeof(ips));	
	env->addEnv("SERVER_ADDR", ips);
	env->addEnv("SERVER_PORT", rq->raw_url.port);
	env->addEnv("REMOTE_ADDR", rq->getClientIp());	
	env->addEnv("REMOTE_PORT", rq->c->socket->get_remote_port());
	if (rq->svh) {
		env->addEnv("DOCUMENT_ROOT", rq->svh->doc_root + skip_length);
		env->addEnv("VH_NAME", rq->svh->vh->name.c_str());
	}
#ifdef KSOCKET_SSL
	if (TEST(rq->raw_url.flags,KGL_URL_SSL)) {
		env->addEnv("HTTPS", "ON");
		if (rq->c->isSSL()) {
			KSSLSocket *sslSocket = static_cast<KSSLSocket *> (rq->c->socket);
			make_ssl_env(env, sslSocket->getSSL());
		}
	}
#endif
	
	return env->addEnvEnd();

}
bool endWorkThread(KHttpRequest *rq, int nextStatus);
/*
同步处理
*/
FUNC_TYPE FUNC_CALL stage_sync(void *param)
{	
	KHttpRequest *rq = (KHttpRequest *)param;
	assert(rq->fetchObj->isSync());
	//请求时间重新计时
	rq->fetchObj->open(rq);
	KTHREAD_RETURN;	
}
int checkResponse(KHttpRequest *rq,KHttpObject *obj)
{
	if (rq->ctx->response_checked) {
		return JUMP_ALLOW;
	}
	rq->ctx->response_checked = true;
	int action = kaccess[RESPONSE].check(rq,obj);
#ifdef ENABLE_KSAPI_FILTER
	if (action == JUMP_ALLOW && conf.gvm->globalVh.hfm) {
		action = conf.gvm->globalVh.hfm->check_response(rq);
	}
#endif
#ifndef HTTP_PROXY
#ifdef ENABLE_USER_ACCESS
	if (action == JUMP_ALLOW && rq->svh) {
		action = rq->svh->vh->checkResponse(rq);
#ifdef ENABLE_KSAPI_FILTER
		if (action==JUMP_ALLOW && rq->svh->vh->hfm) {
			action = rq->svh->vh->hfm->check_response(rq);
		}
#endif
	}
#endif
#endif
	if (action == JUMP_DENY) {
		KMutex *lock = obj->getLock();
		lock->Lock();
		if (obj->refs == 1) {
			KBuffer::destroy(obj->data->bodys);
			obj->data->bodys = NULL;
			set_obj_size(obj, 0);
		}
		lock->Unlock();
	}
	return action;
}
void afterPostHandleForUpstream(KHttpRequest *rq,void *arg)
{
	processQueueRequest(rq);
}
