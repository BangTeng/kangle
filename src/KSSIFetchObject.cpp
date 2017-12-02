/*
 * KSSIFetchObject.cpp
 *
 *  Created on: 2010-8-3
 *      Author: keengo
 */
#include "http.h"
#include "KSSIFetchObject.h"
#include "KSSIProcess.h"
#include "malloc_debug.h"
KSSIFetchObject::KSSIFetchObject() {

}
KSSIFetchObject::~KSSIFetchObject() {

}
void KSSIFetchObject::open(KHttpRequest *rq)
{
	KFetchObject::open(rq);
	KHttpObject *obj = rq->ctx->obj;
	//处理content-type
	if (!stageContentType(rq,obj)) {
		//默认的content-type
		obj->insertHttpHeader(kgl_expand_string("Content-Type"), kgl_expand_string("text/html"));
	}
	if (obj->index.max_age==0) {
		SET(obj->index.flags,ANSW_NO_CACHE);
	}
	ssiProcess.context.setRequest(rq);
	if (!ssiProcess.context.pushFileContext(rq->file, rq->url->path)) {
		handleError(rq,STATUS_SERVER_ERROR,"cann't open file");
		return;
	}
	CLR(obj->index.flags,ANSW_HAS_CONTENT_LENGTH);
	SET(obj->index.flags,ANSW_LOCAL_SERVER);
	handleUpstreamRecvedHead(rq);
}
void KSSIFetchObject::readBody(KHttpRequest *rq)
{
	ssiProcess.readBody(rq);
}
