/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */

#include "global.h"

#include<time.h>

#include <sstream>

#include<map>
#include<string>
#include<list>
#include<stdarg.h>

//#include	"modules.h"
#include	"utils.h"
#include	"http.h"
#include	"cache.h"
#include	"log.h"
#include 	"KHttpManage.h"
#include 	"KSelectorManager.h"
#include 	"lib.h"
#include	"malloc_debug.h"
#include "KHttpObjectHash.h"
#include "KThreadPool.h"
#include "KVirtualHostManage.h"
#include "KSequence.h"
#include	"md5.h"
#include "KFastcgiUtils.h"
#include "KFastcgiFetchObject.h"
#include "KApiFetchObject.h"
#include "KObjectList.h"
#include "KHttpProxyFetchObject.h"
#include "KPoolableSocketContainer.h"
#include "lang.h"
#include "time_utils.h"
#include "KHttpDigestAuth.h"
#include "KHttpBasicAuth.h"
#include "KSubRequest.h"

#include "KHttpFilterManage.h"
#include "ksapi.h"
#include "KAddr.h"
#include "KLogDrill.h"
using namespace std;

void free_url(KUrl *url) {
	url->destroy();
}
inline bool in_stop_cache(KHttpRequest *rq) {
	if (TEST(rq->filter_flags,RF_NO_CACHE))
		return true;
	if (rq->meth == METH_GET || rq->meth == METH_HEAD) {
		return false;
	}
	return true;
}
/*
 check if the keep-alive client has next request data.
 return 0=close 1=yes 2=continue 3=no next request but continue
 */
inline int checkHaveNextRequest(KHttpRequest *rq) {
	if (rq->pre_post_length>0) {
		rq->parser.bodyLen -= rq->pre_post_length;
		rq->parser.body += rq->pre_post_length;
		rq->pre_post_length = 0;
	}
	int bodyLen = rq->parser.bodyLen;
	if (bodyLen > 0) {
		memcpy(rq->readBuf, rq->parser.body , bodyLen);
		rq->clean();
		rq->init(NULL);
		rq->hot = rq->readBuf + bodyLen;
		return rq->parser.parse(rq->readBuf, bodyLen, rq);
	}
	return HTTP_PARSE_NO_NEXT_BUT_CONTINUE;
}
void nextRequest(void *arg,int got)
{
	handleStartRequest((KHttpRequest *)arg,0);
}
void resultEndSubRequest(void *arg ,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	rq->endSubRequest();
}
void stageUnlockedEndRequest(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	CLR(rq->workModel, WORK_MODEL_INTERNAL | WORK_MODEL_REPLACE);
	int status = HTTP_PARSE_FAILED;
	log_access(rq);
	rq->ctx->clean_obj(rq);
#ifdef ENABLE_KSAPI_FILTER
	/* http filter end request hook */
	if (rq->svh && rq->svh->vh->hfm) {
		KHttpFilterHookCollect *end_request = rq->svh->vh->hfm->hook.end_request;
		if (end_request) {
			end_request->process(rq, KF_NOTIFY_END_REQUEST, NULL);
		}
	}
	if (conf.gvm->globalVh.hfm) {
		KHttpFilterHookCollect *end_request = conf.gvm->globalVh.hfm->hook.end_request;
		if (end_request) {
			end_request->process(rq, KF_NOTIFY_END_REQUEST, NULL);
		}
	}
#endif
	if (!TEST(rq->flags, RQ_CONNECTION_CLOSE) && TEST(rq->flags, RQ_HAS_KEEP_CONNECTION)) {
		status = checkHaveNextRequest(rq);
		if (status == HTTP_PARSE_SUCCESS) {
			rq->c->selector->next(nextRequest, rq);
			return;
		}
	}
	if (status == HTTP_PARSE_FAILED) {
		delete rq;
		return;
	}
	if (status != HTTP_PARSE_CONTINUE) {
		rq->clean();
		rq->init(NULL);
	}
	rq->c->read(rq, resultRequestRead, bufferRequestRead);
}
void stageEndRequest(KHttpRequest *rq)
{
#ifndef _WIN32	
#ifndef NDEBUG
	assert(!rq->c->socket->isBlock());
#endif
#endif
	if (rq->fetchObj && rq->ctx->connection_upgrade) {
		if (rq->c->is_locked(rq)) {
			static_cast<KAsyncFetchObject *>(rq->fetchObj)->shutdown(rq);
			return;
		}
		KUpstreamSelectable *st = rq->fetchObj->getSelectable();
		if (st && st->is_upstream_locked()) {
			static_cast<KAsyncFetchObject *>(rq->fetchObj)->shutdown(rq);
			return;
		}
	}

	if (!TEST(rq->flags,RQ_CONNECTION_CLOSE) && rq->sr) {
		//
		rq->c->selector->next(resultEndSubRequest,rq);
		return;
	}
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf && rq->tf->switchRead()) {
#ifndef NDEBUG
		int len = 0;
		char *buf = rq->tf->readBuffer(len);
		assert(len>0 && buf);
#endif
		kassert(!rq->tf->isWrite());
		startTempFileWriteRequest(rq);
		return;
	}
#endif
	rq->c->remove_read_hup(rq);
	assert(!rq->c->is_event(rq, STF_EVENT));
	stageUnlockedEndRequest(rq, 0);
}

void log_access(KHttpRequest *rq) {
	if (rq->isBad()) {
		klog(KLOG_ERR,"BAD REQUEST FROM [%s] len=[%d]\n", rq->getClientIp(),(int)(rq->hot - rq->readBuf));
		return;
	}
	INT64 sended_length = rq->send_size;
	KStringBuf l(512);
	KLogElement *s = &accessLogger;
#ifndef HTTP_PROXY
	if (rq->svh) {

#ifdef ENABLE_VH_LOG_FILE
		if (rq->svh->vh->logger) {
			s = rq->svh->vh->logger;
		}
#endif
	}
#endif
	if (s->place == LOG_NONE
#ifdef ENABLE_LOG_DRILL
		&& conf.log_drill<=0
#endif
		) {
		return;
	}
	char *referer = NULL;
	const char *user_agent = NULL;
	char tmp[64];
	int default_port = 80;
	if (TEST(rq->raw_url.flags,KGL_URL_SSL)) {
		default_port = 443;
	}
	l << rq->getClientIp();	
	//l.WSTR(":");
	//l << rq->c->socket->get_remote_port();
	l.WSTR(" - ");
	if (rq->auth) {
		const char *user = rq->auth->getUser();
		if (user && *user) {
			l << user;
		} else {
			l.WSTR("-");
		}
	} else {
		l.WSTR("-");
	}
	l.WSTR(" ");
	timeLock.Lock();
	l.write_all((char *)cachedLogTime,28);
	timeLock.Unlock();
	l.WSTR(" \"");
	l << rq->getMethod();
	l.WSTR(" ");
	l << (TEST(rq->raw_url.flags,KGL_URL_SSL) ? "https://" : "http://");
	KUrl *url;
	if (rq->sr) {
		url = rq->url;
		referer = rq->sr->url->getUrl();
		user_agent = "-sub request-";
	} else {
		url = &rq->raw_url;
		referer = (char *)rq->parser.getHttpValue("Referer");
		user_agent = rq->parser.getHttpValue("User-Agent");
	}
	l << url->host;
	if (url->port != default_port) {
		l << ":" << url->port;
	}
	l << url->path;
	if (url->param) {
		l << "?" << url->param;
	}
#ifdef ENABLE_HTTP2
	if (rq->http2_ctx) {
		l.WSTR(" HTTP/2\" ");
	} else 
#endif	
		l.WSTR(" HTTP/1.1\" ");
	l << rq->status_code << " " ;
#ifdef _WIN32
    const char *formatString="%I64d";
#else
    const char *formatString = "%lld";
#endif
	sprintf(tmp,formatString,sended_length);
	l << tmp;
#ifndef NDEBUG
	const char *range = rq->parser.getHttpValue("Range");
	if (range) {
	       l << " \"" << range << "\"" ;
	}
#endif
	//s->log(formatString,rq->send_ctx.send_size);
	l.WSTR(" \"");
	if(referer){
		l << referer;
	} else {
		l.WSTR("-");
	}
	l.WSTR("\" \"");
	if(user_agent){
		l << user_agent;
	} else {
		l.WSTR("-");
	}
	//*
	l.WSTR("\"[");
#ifndef NDEBUG
	l << "F" << (unsigned)rq->flags << "f" << (unsigned)rq->filter_flags;
	l << "u" << (int)rq->ctx->upstream_socket;
	if (!rq->ctx->upstream_expected_done) {
		l.WSTR("e");
	}
#endif
	if (TEST(rq->flags, RQ_UPSTREAM_ERROR)) {
		l.WSTR("E");
	}
	if(rq->ctx->cache_hit){
		l.WSTR("C");
	}
	if(TEST(rq->flags,RQ_OBJ_STORED)){
		l.WSTR("S");
	}
	if (rq->ctx->upstream_sign) {
		l.WSTR("U");
	}
	if (rq->ctx->parent_signed) {
		l.WSTR("P");
	}
	if(TEST(rq->flags,RQ_OBJ_VERIFIED)){
		l.WSTR("V");
	}
	if(TEST(rq->flags,RQ_TE_GZIP)){
		l.WSTR("Z");
	}
	if(TEST(rq->flags,RQ_TE_CHUNKED)){
		l.WSTR("K");
	}

	if (TEST(rq->flags,RQ_HAS_KEEP_CONNECTION|RQ_CONNECTION_CLOSE) == RQ_HAS_KEEP_CONNECTION) {
		l.WSTR("L");
	}
	if (rq->ctx->upstream_connection_keep_alive) {
		l.WSTR("l");
	}
	if (rq->sr) {
		free(referer);
	}
	
	l.WSTR("t");
	INT64 t = kgl_current_msec - rq->begin_time_msec;
	l << t;
	if (rq->first_response_time_msec > 0) {
		l.WSTR("T");
		t = rq->first_response_time_msec - rq->begin_time_msec ;
		l << t;
	}
	if (rq->mark!=0) {
		l.WSTR("m");
		l << (int)rq->mark;
	}
	l.WSTR("a");
	l << rq->ctx->us_code;
	l.WSTR("]");
	if (conf.log_event_id) {
		l.WSTR(" ");
		l.add(rq->begin_time_msec, INT64_FORMAT_HEX);
		l.WSTR("-");
		l.add((INT64)rq, INT64_FORMAT_HEX);
	}
#if 0
	l.WSTR(" ");
	l.addHex(rq->if_modified_since);
	l.WSTR(" ");
	l.addHex(rq->ctx->lastModified);
	l.WSTR(" ");
	if (TEST(rq->flags, RQ_HAS_IF_NONE_MATCH)) {
		l.WSTR("inm");
	}
	if (rq->ctx->if_none_match) {
		l.write_all(rq->ctx->if_none_match->data, rq->ctx->if_none_match->len);
	}
#endif
	l.WSTR("\n");
	//*/
	if (s->place != LOG_NONE) {
		s->startLog();
		s->write(l.getString(), l.getSize());
		s->endLog(false);
	}
#ifdef ENABLE_LOG_DRILL
	if (conf.log_drill > 0) {
		add_log_drill(rq,l);
	}
#endif
}
inline int checkRequest(KHttpRequest *rq)
{
#ifdef ENABLE_KSAPI_FILTER
	if (conf.gvm->globalVh.hfm &&
		!conf.gvm->globalVh.hfm->check_request(rq)) {
		return JUMP_DENY;
	}
#endif
	return kaccess[REQUEST].check(rq, NULL);
}
void handleConnectMethod(KHttpRequest *rq)
{

	send_error(rq,NULL,STATUS_METH_NOT_ALLOWED,"The requested method CONNECT is not allowed");

}
void stageHttpManageLogin(KHttpRequest *rq)
{
	if (rq->auth) {
		delete rq->auth;
		rq->auth = NULL;
	}
#ifdef ENABLE_DIGEST_AUTH
	if (conf.auth_type == AUTH_DIGEST) {
		KHttpDigestAuth *auth = new KHttpDigestAuth();
		auth->init(rq, PROGRAM_NAME);
		rq->auth = auth;
	} else
#endif
		rq->auth = new KHttpBasicAuth(PROGRAM_NAME);
	rq->auth->set_auth_header(AUTH_HEADER_WEB);
	const char path_split_str[2]={PATH_SPLIT_CHAR,0};
	rq->buffer << "<html><body>Please set the admin user and password in the file: <font color='red'>"
		<< "kangle_installed_path" << path_split_str << "etc" << path_split_str 
		<< "config.xml</font> like this:"
		"<font color=red><pre>"
		"&lt;admin user='admin' password='kangle' admin_ips='127.0.0.1|*'/&gt;"
		"</pre></font>\n"
		"The default admin user is admin, password is kangle</body></html>";

	send_auth(rq,&rq->buffer);
}
void stageHttpManage(KHttpRequest *rq)
{
	rq->releaseVirtualHost();
	conf.admin_lock.Lock();
	if (!checkManageLogin(rq)) {
		conf.admin_lock.Unlock();
		char ips[MAXIPLEN];
		rq->c->socket->get_remote_ip(ips,sizeof(ips));
		klog(KLOG_WARNING, "[ADMIN_FAILED]%s:%d %s\n",
				ips,
				rq->c->socket->get_remote_port(), rq->raw_url.path);
		stageHttpManageLogin(rq);
		return;
	}
	conf.admin_lock.Unlock();
	/*
	if(getUrlValue("url_password")=="1" && rq->raw_url.param){
		xfree(rq->raw_url.param);
		rq->raw_url.param = xstrdup("***url_password***");
	}
	*/
	char ips[MAXIPLEN];
	rq->c->socket->get_remote_ip(ips,sizeof(ips));
	klog(KLOG_NOTICE, "[ADMIN_SUCCESS]%s:%d %s%s%s\n",
			ips,
			rq->c->socket->get_remote_port(), rq->raw_url.path,
			(rq->raw_url.param?"?":""),(rq->raw_url.param?rq->raw_url.param:""));
	if(strstr(rq->url->path,".whm") 
		|| strcmp(rq->url->path,"/logo.gif")==0
		|| strcmp(rq->url->path,"/main.css")==0){
		CLR(rq->flags,RQ_HAS_AUTHORIZATION);
		assert(rq->fetchObj==NULL && rq->svh==NULL);
		rq->svh = conf.sysHost->getFirstSubVirtualHost();
		if(rq->svh){
			rq->svh->vh->addRef();
			async_http_start(rq);
			return;
		}
		assert(false);
	}
	rq->fetchObj = new KHttpManage;
	processRequest(rq);
}
void stageDeniedRequest(KHttpRequest *rq) {
	if (rq->send_ctx.getBufferSize() > 0 || rq->buffer.getLen() > 0
		|| rq->status_code>0
		) {
#ifdef ENABLE_TF_EXCHANGE
		if (rq->tf) {
			
			delete rq->tf;
			rq->tf = NULL;
		}
#endif
		rq->startResponseBody(-1);
		stageWriteRequest(rq);
		return;
	}
	if (TEST(rq->filter_flags, RQ_SEND_AUTH)) {
		send_auth(rq);
		return;
	}
	if (!TEST(rq->flags, RQ_HAS_SEND_HEADER)) {
		send_error(rq, NULL, STATUS_FORBIDEN, "denied by request access control");
		return;
	}
}
bool check_virtual_host_request(KHttpRequest *rq) {
	if (rq->svh==NULL) {
		return true;
	}
	if (rq->svh->vh->status != 0) {
		handleError(rq, STATUS_SERVICE_UNAVAILABLE, "virtual host is closed");
		return false;
	}
#ifdef ENABLE_VH_RS_LIMIT
	KSpeedLimit *sl= rq->svh->vh->refsSpeedLimit();
	if (sl) {
		rq->pushSpeedLimit(sl);
	}
#endif
#ifdef ENABLE_VH_FLOW
	KFlowInfo *flow = rq->svh->vh->refsFlowInfo();
	if (flow) {
		rq->pushFlowInfo(flow);
	}
#endif
#ifndef HTTP_PROXY
#ifdef ENABLE_USER_ACCESS
	switch(rq->svh->vh->checkRequest(rq)) {
	case JUMP_DROP:
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		stageEndRequest(rq);
		return false;
	case JUMP_DENY:
		stageDeniedRequest(rq);
		return false;
	default:
		break;
	}
#endif
#endif
	return true;
}
bool bind_virtual_host(KHttpRequest *rq,const char *hostname,int len) {
	query_vh_result vh_result;
#ifdef KSOCKET_SSL
	if (rq->c->sni) {
		vh_result = rq->c->useSniVirtualHost(rq);
	} else
#endif
		vh_result = conf.gvm->queryVirtualHost(rq->c->ls,&rq->svh,hostname,len);
	switch (vh_result) {
	case query_vh_connect_limit:
		send_error(rq, NULL, STATUS_SERVER_ERROR, "max connect limit.");
		return false;
	case query_vh_host_not_found:
		send_error(rq,NULL,STATUS_BAD_REQUEST,"host not found.");
		return false;
	default:
		break;
	}
	u_short flags = rq->raw_url.flags;
	rq->raw_url.flags = 0;
	if (!check_virtual_host_request(rq)) {
		SET(rq->raw_url.flags, flags);
		return false;
	}
	if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
		//rewrite host
		KSubVirtualHost *new_svh = NULL;
		conf.gvm->queryVirtualHost(rq->c->ls, &new_svh, rq->url->host, 0);
		if (new_svh) {
			if (new_svh->vh == rq->svh->vh) {
				rq->svh->release();
				rq->svh = new_svh;
			} else {
				new_svh->release();
			}
		}
	}
	SET(rq->raw_url.flags, flags);
	return true;
}
bool bind_virtual_host(KHttpRequest *rq)
{
	return bind_virtual_host(rq,rq->url->host,0);
}
void handleStartRequest(KHttpRequest *rq,int got)
{
	
	rq->beginRequest();
#ifdef HTTP_PROXY
	if (rq->meth == METH_CONNECT) {
		handleConnectMethod(rq);
		return;
	}
#endif
	if (rq->ctx->read_huped) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		send_error(rq,NULL,STATUS_BAD_REQUEST,"Client close connection");
		return;
	}
	if (TEST(rq->raw_url.flags,KGL_URL_BAD)) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		send_error(rq, NULL, STATUS_BAD_REQUEST, "Bad url");
		return;
	}

	if (rq->isBad()) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		send_error(rq, NULL, STATUS_BAD_REQUEST, "Bad request format.");
		return;
	}
	if (TEST(rq->workModel,WORK_MODEL_MANAGE)) {
		stageHttpManage(rq);
		return;
	}
#ifdef ENABLE_STAT_STUB
	if (strcmp(rq->url->path, "/kangle.status") == 0) {
		if (rq->meth!=METH_HEAD) {
			rq->buffer << "OK\n";
			/*
			rq->buffer << (INT64)katom_get64((void *)&kgl_total_accepts) << "\t";
			rq->buffer << (INT64)katom_get64((void *)&kgl_total_requests) << "\n";
			*/
		}
		send_http(rq, NULL, STATUS_OK, &rq->buffer);
		return;
	}
#endif
	if (!TEST(rq->workModel, WORK_MODEL_MANAGE | WORK_MODEL_INTERNAL|WORK_MODEL_SKIP_ACCESS)) {
		switch(checkRequest(rq)) {
		case JUMP_DROP:
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			stageEndRequest(rq);
			return;
		case JUMP_DENY:
			stageDeniedRequest(rq);
			return;
		case JUMP_VHS:
			if (!bind_virtual_host(rq)) {
				return;
			}
		default:
			break;
		}
	}
	async_http_start(rq);
}
void async_http_start(KHttpRequest *rq)
{
	KContext *context = rq->ctx;
	
	if (TEST(rq->flags,RQ_HAVE_EXPECT)) {
		rq->c->socket->write_all("HTTP/1.1 100 Continue\r\n\r\n");
		CLR(rq->flags,RQ_HAVE_EXPECT);
	}
	if (conf.max_io>0 && katom_get((void *)&kgl_aio_count) > (uint32_t)conf.max_io) {
		//async io limit
		SET(rq->filter_flags, RF_NO_DISK_CACHE);
	}
	//only if cached
	if (TEST(rq->flags, RQ_HAS_ONLY_IF_CACHED)) {
		context->obj = findHttpObject(rq, false, &context->new_object);
		if (!context->obj) {
			send_error(rq, context->obj, 404, "Not in cache");
			goto done;
		}
		processCacheRequest(rq);
		goto done;
	}
	//end purge or only if cached
	if (in_stop_cache(rq)) {
		context->obj = new KHttpObject(rq);
		context->new_object = 1;
		if (!context->obj) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			//context->last_status = false;
			goto done;
		}
		SET(context->obj->index.flags,FLAG_DEAD);
	} else {
		context->obj = findHttpObject(rq, true, &context->new_object);
		//		msg = "net";
		if (!context->obj) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			goto done;
		}
	}
	if (context->new_object) { //It is a new object
		if (rq->meth != METH_GET) {
			SET(context->obj->index.flags,FLAG_DEAD);
		}
		processNotCacheRequest(rq);
	} else {
		//useCache = true;
		processCacheRequest(rq);
	}
	done:
	return;
}

void processHttpRequest(KHttpRequest *rq) {
	//KContext context;
	//memset(&context,0,sizeof(KContext));
	//context.rq = rq;
	 async_http_start(rq);
}


