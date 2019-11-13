/*
 * KFetchObject.cpp
 *
 *  Created on: 2010-4-19
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
#include "KFetchObject.h"
#include "kmalloc.h"
#include "KAsyncFetchObject.h"
#include "http.h"
#include "KHttpTransfer.h"
StreamState KFetchObject::PushBody(KHttpRequest *rq, const char *buf, int len)
{
	if (!rq->ctx->connection_upgrade && rq->ctx->know_length) {
		len = (int)MIN(rq->ctx->left_read, (INT64)len);
		rq->ctx->left_read -= len;
	}
	kassert(rq->ctx->st);

	return rq->ctx->st->write_all(buf, len);
}
/*
处理已经从upstream读到的数据，返回true,继续读取，false则不继续读，表示已经有数据可以发到rq
*/
kev_result KFetchObject::pushHttpBody(KHttpRequest *rq,const char *buf,int len)
{
	switch (PushBody(rq,buf,len)) {
	case STREAM_WRITE_END:
		//正确读完了chunked数据
		assert(rq->ctx->connection_upgrade == false);
		readBodyEnd(rq);
		return stage_rdata_end(rq, STREAM_WRITE_END);
	case STREAM_WRITE_FAILED:
		if (rq->ctx->connection_upgrade) {
			return (static_cast<KAsyncFetchObject *>(this))->shutdown(rq);
		}
		return stage_rdata_end(rq,STREAM_WRITE_FAILED);
	default:
		return try_send_request(rq);
	}
}
KFetchObject *KFetchObject::clone(KHttpRequest *rq)
{
	if (brd) {
		KFetchObject *fetchObj = brd->rd->makeFetchObject(rq,rq->file);
		fetchObj->bindBaseRedirect(brd);
		return fetchObj;
	}
	return NULL;
}
kev_result KFetchObject::open(KHttpRequest *rq)
{
	this->closed = 0;
	return kev_err;
}
