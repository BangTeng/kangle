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
#include "malloc_debug.h"
#include "KAsyncFetchObject.h"
#include "http.h"
/*
处理已经从upstream读到的数据，返回true,继续读取，false则不继续读，表示已经有数据可以发到rq
*/
bool KFetchObject::pushHttpBody(KHttpRequest *rq,char *buf,int len)
{
	if (!rq->ctx->connection_upgrade && rq->ctx->know_length) {
		len = (int)MIN(rq->ctx->left_read, (INT64)len);
		rq->ctx->left_read -= len;
	}
	assert(rq->ctx->st);
	StreamState result  = rq->ctx->st->write_all(buf, len);
	switch (result) {
	case STREAM_WRITE_END:
		//正确读完了chunked数据
		assert(rq->ctx->connection_upgrade == false);
		readBodyEnd(rq);
		stage_rdata_end(rq,result);
		return false;
	case STREAM_WRITE_FAILED:
		if (rq->ctx->connection_upgrade) {
			(static_cast<KAsyncFetchObject *>(this))->shutdown(rq);
			return false;
		}
		stage_rdata_end(rq,STREAM_WRITE_FAILED);
		return false;
	default:
		if(try_send_request(rq)){
			//如果已经发送了数据就不要继续读了。
			return false;
		}
		return true;
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
void KFetchObject::open(KHttpRequest *rq)
{
	this->closed = false;
}
