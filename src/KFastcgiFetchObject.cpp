/*
 * KFastcgiFetchObject.cpp
 *
 *  Created on: 2010-4-21
 *      Author: keengo
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
#include "do_config.h"

#include "KFastcgiFetchObject.h"
#include "KFastcgiUtils.h"
#include "http.h"
#include "lib.h"
#include "lang.h"
#include "KHttpObjectParserHook.h"
#include "KHttpHeadPull.h"
#include "KHttpTransfer.h"
#include "KHttpBasicAuth.h"
#include "KApiRedirect.h"
#include "malloc_debug.h"

KFastcgiFetchObject::KFastcgiFetchObject()
{
	buf_len = 0;
	body_len = -1;
	end = NULL;
	bodyEnd = false;
}
KFastcgiFetchObject::~KFastcgiFetchObject() {

}
void KFastcgiFetchObject::buildHead(KHttpRequest *rq)
{
	buffer = new KSocketBuffer(NBUFF_SIZE);
	KHttpObject *obj = rq->ctx->obj;
	SET(obj->index.flags,ANSW_LOCAL_SERVER);
	hook.init(obj,rq);
	hook.setProto(Proto_fcgi);
	KFastcgiStream<KSocketBuffer> fbuf;
	fbuf.setStream(buffer);
	fbuf.extend = isExtend();
	if (fbuf.extend) {
		FCGI_BeginRequestRecord package;
		memset(&package, 0, sizeof(package));
		package.header.type = FCGI_BEGIN_REQUEST;
		package.header.contentLength = htons(sizeof(FCGI_BeginRequestBody));
		KApiRedirect *ard = static_cast<KApiRedirect *>(brd->rd);
		package.body.id = ard->id;
		buffer->write_all((char *)&package,sizeof(FCGI_BeginRequestRecord));
	}else{
		fbuf.beginRequest(client->getLifeTime()>0);
	}
	if (isExtend() && rq->auth) {
		const char *auth_type = KHttpAuth::buildType(rq->auth->getType());
		const char *user = rq->auth->getUser();
		if (user) {
			fbuf.addEnv("AUTH_USER", user);
		}
		fbuf.addEnv("AUTH_TYPE", auth_type);
		if (rq->auth->getType() == AUTH_BASIC) {
			KHttpBasicAuth *auth = (KHttpBasicAuth *) rq->auth;
			const char *password = auth->getPassword();
			if (password) {
				fbuf.addEnv("AUTH_PASSWORD", password);
			}
		}
	}
	bool chrooted = false;
#ifndef _WIN32
	chrooted = rq->svh->vh->chroot;
#endif
	bool sendResult = make_http_env(rq,brd, rq->ctx->lastModified, rq->file, &fbuf, chrooted);
	if (!sendResult) {//send error
		buffer->destroy();
		return;
	}
	if (rq->pre_post_length>0) {
		//处理pre loaded post数据
		//printf("pre_post_len=%d\n",pre_post_len);
		fbuf.write_data(rq->parser.body,rq->pre_post_length);
		rq->parser.body += rq->pre_post_length;
		rq->parser.bodyLen -= rq->pre_post_length;
		rq->left_read -= rq->pre_post_length;
		rq->pre_post_length = 0;
	}
	if (!rq->has_post_data()) {
		appendPostEnd();
	}

}
void KFastcgiFetchObject::appendPostEnd()
{
	if(isExtend()){
		//api使用不用发结束post标记
		return;
	}
	//最后的post数据
	buff *fcgibuff = (buff *)malloc(sizeof(buff));
	fcgibuff->data = (char *)malloc(sizeof(FCGI_Header));
	fcgibuff->used = sizeof(FCGI_Header);
	FCGI_Header *fcgiheader = (FCGI_Header *)fcgibuff->data;
	memset(fcgiheader, 0, sizeof(FCGI_Header));
	fcgiheader->version = 1;
	fcgiheader->type = FCGI_STDIN;
	fcgiheader->contentLength = 0;
	fcgiheader->requestIdB0 = 1;
	buffer->appendBuffer(fcgibuff);
}
void KFastcgiFetchObject::buildPost(KHttpRequest *rq)
{

	unsigned postLen = buffer->getLen();
	assert(postLen>0);
	buff *fcgibuff = (buff *)malloc(sizeof(buff));
	fcgibuff->data = (char *)malloc(sizeof(FCGI_Header));
	//nbuff *fcgibuff = (nbuff *)malloc(sizeof(nbuff) + sizeof(FCGI_Header));
	fcgibuff->used = sizeof(FCGI_Header);
	FCGI_Header *fcgiheader = (FCGI_Header *)fcgibuff->data;
	memset(fcgiheader, 0, sizeof(FCGI_Header));
	fcgiheader->version = 1;
	fcgiheader->type = FCGI_STDIN;
	fcgiheader->contentLength = htons(postLen);
	fcgiheader->requestIdB0 = 1;
	buffer->insertBuffer(fcgibuff);
	if(!rq->has_post_data()){
		appendPostEnd();
	}
}
Parse_Result KFastcgiFetchObject::parseBody(KHttpRequest *rq,char *data,int len)
{
	hot = data;
	end = data + len;
	return Parse_Continue;
}
Parse_Result KFastcgiFetchObject::parseHead(KHttpRequest *rq,char *buf,int len) 
{
	char *parse_hot = buf;
	end = buf + len;
	while (len>0) {
		char *str = parse(rq,&parse_hot,len);
		if(str && len>0) {
			if(this->buf.type==FCGI_STDERR){
				fwrite(str,len,1,stderr);
				fwrite("\n",1,1,stderr);
			} else {
				kassert(str!=NULL);
				switch(parser.pull(str,len,&hook)){
				case HTTP_PARSE_SUCCESS:
					hot = parse_hot;
					rq->ctx->obj->data->headers = parser.stealHeaders(rq->ctx->obj->data->headers);
					return Parse_Success;
				case HTTP_PARSE_FAILED:
					return Parse_Failed;
				default:
					break;
				}
			}
		}
		if (bodyEnd) {
			return Parse_Failed;
		}
		len = end - parse_hot;
	}
	//assert(false);
	return Parse_Continue;
}
char *KFastcgiFetchObject::nextBody(KHttpRequest *rq,int &len)
{
	if (parser.bodyLen>0) {
		len = parser.bodyLen;
		parser.bodyLen = 0;
		//printf("parser len=%d,buf=%p\n",len,buf);
		return parser.body;
	}
	if(end==NULL){
		return NULL;
	}
	len = end - hot;
	char *buf = parse(rq,&hot,len);
	if (buf==NULL) {
		readBodyEnd(rq);
	}
	return buf;
}
char *KFastcgiFetchObject::parse(KHttpRequest *rq,char **str,int &len)
{
	if (body_len==0) {
		if (buf.paddingLength>0) {
			//skip padding
			int padlen = MIN(len,(int)buf.paddingLength);
			buf.paddingLength -= padlen;
			*str += padlen;
			len -= padlen;
			if(buf.paddingLength>0){
				return NULL;
			}
		} 
		body_len = -1;
		buf_len = 0;
	}
	if (body_len==-1) {
		//head
		if (len<=0) {
			return NULL;
		}
		assert(buf_len<(int)sizeof(FCGI_Header));
		int left_read = sizeof(FCGI_Header) - buf_len;
		left_read = MIN(left_read,len);
		memcpy(((char *)&buf)+buf_len,*str,left_read);
		buf_len += left_read;
		*str += left_read;
		len -= left_read;
		//add_buf(str,len);
		if (buf_len<(int)sizeof(FCGI_Header)) {
			//continue;
			return NULL;
		}
		body_len = ntohs(buf.contentLength);
	}
	//printf("type=%d,body_len=%d,len=%d\n",buf.type,body_len,len);
	if (buf.type == FCGI_END_REQUEST) {
		expectDone(rq);
		bodyEnd = true;
		return NULL;
	}
	if (buf.type == FCGI_ABORT_REQUEST) {
		bodyEnd = true;
		return NULL;
	}
	int this_body_len = MIN(len,body_len);
	body_len -= this_body_len;
	char *body = *str;
	*str += this_body_len;	
	len = this_body_len;
	if (this_body_len==0) {
		return NULL;
	}
	return body;
}

