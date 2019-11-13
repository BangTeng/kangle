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
#include 	"kselector_manager.h"
#include 	"lib.h"
#include	"kmalloc.h"
#include "KHttpObjectHash.h"
#include "kthread.h"
#include "KVirtualHostManage.h"
#include "KSequence.h"
#include "kmd5.h"
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
#include "KSSLSniContext.h"

#include "KHttpFilterManage.h"
#include "ksapi.h"
#include "kaddr.h"
#include "KLogDrill.h"
using namespace std;

void free_url(KUrl *url) {
	url->destroy();
}
inline bool in_stop_cache(KHttpRequest *rq) {
	if (TEST(rq->filter_flags, RF_NO_CACHE)) {
		return true;
	}
	if (rq->meth == METH_GET || rq->meth == METH_HEAD) {
		return false;
	}
	return true;
}
static kev_result stageUnlockedEndRequest(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	log_access(rq);
	rq->ctx->clean_obj(rq);
	rq->endRequest();
	return kev_destroy;
}
bool is_request_locked(KHttpRequest *rq)
{
	//client/upstream must be non-lock
	if (rq->sink->IsLocked()) {
		return true;
	}
	if (rq->fetchObj) {
		KUpstream *st = rq->fetchObj->GetUpstream();
		if (st) {
			return st->IsLocked();
		}
	}
	return false;
}
kev_result stageEndRequest(KHttpRequest *rq, bool expected)
{
	//printf("stageEndRequest=[%p]\n", rq);
	if (rq->IsSync()) {
		rq->RemoveSync();
	}
	if (rq->fetchObj && rq->ctx->connection_upgrade) {
		if (rq->sink->IsLocked()) {
			rq->sink->Shutdown();
			return kev_ok;
		}
		KUpstream *st = rq->fetchObj->GetUpstream();
		if (st && st->IsLocked()) {
			st->Shutdown();
			return kev_ok;
		}
	}
	assert(!is_request_locked(rq));

#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf && rq->tf->switchRead()) {
#ifndef NDEBUG
		int len = 0;
		char *buf = rq->tf->readBuffer(len);
		assert(len>0 && buf);
#endif
		return startTempFileWriteRequest(rq);
	}
#endif
	rq->sink->RemoveReadHup();
	if (rq->sink->HasHeaderDataToSend()) {
		return rq->Write(rq, stageUnlockedEndRequest, NULL);
	}
	return stageUnlockedEndRequest(rq, 0);
}

void log_access(KHttpRequest *rq) {
	//printf("log_access=[%p]\n", rq);
	if (rq->isBad()) {
		klog(KLOG_ERR,"BAD REQUEST FROM [%s].\n", rq->getClientIp());
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
	//l << rq->sink->get_remote_port();
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
#ifdef HTTP_PROXY
	if (rq->meth!=METH_CONNECT)
#endif
	l << (TEST(rq->raw_url.flags,KGL_URL_SSL) ? "https://" : "http://");
	KUrl *url = &rq->raw_url;
	referer = (char *)rq->GetHttpValue("Referer");
	user_agent = rq->GetHttpValue("User-Agent");
	l << url->host;
	if (url->port != default_port) {
		l << ":" << url->port;
	}
#ifdef HTTP_PROXY
	if (rq->meth != METH_CONNECT)
#endif
	l << url->path;
	if (url->param) {
		l << "?" << url->param;
	}
#ifdef ENABLE_HTTP2
	if (rq->http_major>1) {
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
	const char *range = rq->GetHttpValue("Range");
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
	//l << "u" << (int)rq->ctx->upstream_socket;
	if (!rq->ctx->upstream_expected_done) {
		l.WSTR("e");
	}
#endif
	if (rq->ctx->read_huped) {
		l.WSTR("h");
	}
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
	//l.WSTR("a");
	//l << rq->ctx->us_code;
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
	return kaccess[REQUEST].check(rq, NULL);
}
void handleConnectMethod(KHttpRequest *rq)
{

	send_error(rq,NULL,STATUS_METH_NOT_ALLOWED,"The requested method CONNECT is not allowed");

}
kev_result stageHttpManageLogin(KHttpRequest *rq)
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
	KAutoBuffer buffer(rq->pool);
	buffer << "<html><body>Please set the admin user and password in the file: <font color='red'>"
		<< "kangle_installed_path" << path_split_str << "etc" << path_split_str 
		<< "config.xml</font> like this:"
		"<font color=red><pre>"
		"&lt;admin user='admin' password='kangle' admin_ips='127.0.0.1|*'/&gt;"
		"</pre></font>\n"
		"The default admin user is admin, password is kangle</body></html>";
	return send_auth(rq,&buffer);
}
kev_result stageHttpManage(KHttpRequest *rq)
{
	rq->releaseVirtualHost();
	conf.admin_lock.Lock();
	if (!checkManageLogin(rq)) {
		conf.admin_lock.Unlock();
		char ips[MAXIPLEN];
		rq->sink->GetRemoteIp(ips,sizeof(ips));
		klog(KLOG_WARNING, "[ADMIN_FAILED] %s %s\n",	ips, rq->raw_url.path);
		return stageHttpManageLogin(rq);
	}
	conf.admin_lock.Unlock();
	char ips[MAXIPLEN];
	rq->sink->GetRemoteIp(ips, sizeof(ips));
	klog(KLOG_NOTICE, "[ADMIN_SUCCESS]%s %s%s%s\n",
			ips, rq->raw_url.path,
			(rq->raw_url.param?"?":""),(rq->raw_url.param?rq->raw_url.param:""));
	if(strstr(rq->url->path,".whm") 
		|| strcmp(rq->url->path,"/logo.gif")==0
		|| strcmp(rq->url->path,"/main.css")==0){
		CLR(rq->flags,RQ_HAS_AUTHORIZATION);
		assert(rq->fetchObj==NULL && rq->svh==NULL);
		rq->svh = conf.sysHost->getFirstSubVirtualHost();
		if(rq->svh){
			rq->svh->vh->addRef();
			return async_http_start(rq);
		}
		assert(false);
	}
	rq->fetchObj = new KHttpManage;
	return processRequest(rq);
}
kev_result stageDeniedRequest(KHttpRequest *rq) {
	if (rq->fetchObj != NULL) {
		if (rq->status_code == 0) {
			rq->responseConnection();
			rq->responseStatus(STATUS_OK);
		}
		rq->startResponseBody(-1);
		return rq->fetchObj->open(rq);
	}
	if (rq->status_code > 0) {
		rq->startResponseBody(0);
		return stageEndRequest(rq,true);
	}
	if (TEST(rq->filter_flags, RQ_SEND_AUTH)) {
		return send_auth(rq);
	}
	if (!TEST(rq->flags, RQ_HAS_SEND_HEADER)) {
		return send_error(rq, NULL, STATUS_FORBIDEN, "denied by request access control");
	}
	return kev_err;
}
kev_result check_virtual_host_request(KHttpRequest *rq) {
	if (rq->svh==NULL) {
		return kev_err;
	}
	if (rq->svh->vh->status != 0) {
		return handleError(rq, STATUS_SERVICE_UNAVAILABLE, "virtual host is closed");
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
		return stageEndRequest(rq);
	case JUMP_DENY:
		return stageDeniedRequest(rq);
	default:
		break;
	}
#endif
#endif
	return kev_err;
}
kev_result bind_virtual_host(KHttpRequest *rq,const char *hostname,int len) {
	query_vh_result vh_result;
#ifdef KSOCKET_SSL
	kconnection *c = rq->sink->GetConnection();
	if (c && c->sni) {
		vh_result = kgl_use_ssl_sni(c, rq);
	} else
#endif
		vh_result = conf.gvm->queryVirtualHost(rq->sink->GetBindServer(),&rq->svh,hostname,len);
#ifdef ENABLE_VH_RS_LIMIT
	if (query_vh_success == vh_result && rq->svh) {
		if (!rq->svh->vh->addConnection(rq)) {
			vh_result = query_vh_connect_limit;
		}
	}
#endif
	switch (vh_result) {
	case query_vh_connect_limit:
		return send_error(rq, NULL, STATUS_SERVER_ERROR, "max connect limit.");
	case query_vh_host_not_found:
		return send_error(rq,NULL,STATUS_BAD_REQUEST,"host not found.");
	default:
		break;
	}
	u_short flags = rq->raw_url.flags;
	rq->raw_url.flags = 0;
	kev_result ret = check_virtual_host_request(rq);
	if (KEV_HANDLED(ret)) {
		if (KEV_AVAILABLE(ret)) {
			SET(rq->raw_url.flags, flags);
		}
		return ret;
	}
	if (TEST(rq->raw_url.flags,KGL_URL_REWRITED)) {
		//rewrite host
		KSubVirtualHost *new_svh = NULL;
		conf.gvm->queryVirtualHost(rq->sink->GetBindServer(), &new_svh, rq->url->host, 0);
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
	return kev_err;
}
kev_result bind_virtual_host(KHttpRequest *rq)
{
	return bind_virtual_host(rq,rq->url->host,0);
}
kev_result handleStartRequest(KHttpRequest *rq,int got)
{
	kev_result ret;
	rq->beginRequest();
#ifdef HTTP_PROXY
	if (rq->meth == METH_CONNECT) {
		return handleConnectMethod(rq);
	}
#endif
	if (rq->ctx->read_huped) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return send_error(rq,NULL,STATUS_BAD_REQUEST,"Client close connection");
	}
	if (TEST(rq->raw_url.flags,KGL_URL_BAD)) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return send_error(rq, NULL, STATUS_BAD_REQUEST, "Bad url");
	}

	if (rq->isBad()) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return send_error(rq, NULL, STATUS_BAD_REQUEST, "Bad request format.");
	}
	if (rq->IsWorkModel(WORK_MODEL_MANAGE)) {
		return stageHttpManage(rq);
	}
#ifdef ENABLE_STAT_STUB
	if (strcmp(rq->url->path, "/kangle.status") == 0) {
		KAutoBuffer s(rq->pool);
		if (rq->meth!=METH_HEAD) {			
			s << "OK\n";
		}
		return send_http(rq, NULL, STATUS_OK, &s);
	}
#endif
	if (!rq->ctx->skip_access) {
		switch(checkRequest(rq)) {
		case JUMP_DROP:
			return stageEndRequest(rq);
		case JUMP_DENY:
			return stageDeniedRequest(rq);
		case JUMP_VHS:
			ret = bind_virtual_host(rq);
			if (KEV_HANDLED(ret)) {
				return ret;
			}
		}
	}
	return async_http_start(rq);
}
kev_result async_http_start(KHttpRequest *rq)
{
	//printf("async_http_start. [%p].\n",rq);
	KContext *context = rq->ctx;
	
#ifdef ENABLE_STAT_STUB
	if (conf.max_io>0 && katom_get((void *)&kgl_aio_count) > (uint32_t)conf.max_io) {
		//async io limit
		SET(rq->filter_flags, RF_NO_DISK_CACHE);
	}
#endif
	//only if cached
	if (TEST(rq->flags, RQ_HAS_ONLY_IF_CACHED)) {
		context->obj = findHttpObject(rq, false, context);
		if (!context->obj) {
			return send_error(rq, context->obj, 404, "Not in cache");
		}
		return processCacheRequest(rq);
	}
	//end purge or only if cached
	if (in_stop_cache(rq)) {
		context->obj = new KHttpObject(rq);
		context->new_object = 1;
		if (!context->obj) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return send_error(rq, context->obj, STATUS_SERVER_ERROR, "cann't malloc memory.");
		}
		SET(context->obj->index.flags,FLAG_DEAD);
	} else {
		context->obj = findHttpObject(rq, true, context);
		if (!context->obj) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return send_error(rq, context->obj, STATUS_SERVER_ERROR, "cann't malloc memory.");
		}
	}
	if (context->new_object) { //It is a new object
		if (rq->meth != METH_GET) {
			SET(context->obj->index.flags,FLAG_DEAD);
		}
		return processNotCacheRequest(rq);
	}
	return processCacheRequest(rq);	
}

