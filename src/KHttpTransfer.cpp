/*
 * KHttpTransfer.cpp
 *
 *  Created on: 2010-5-4
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

#include "http.h"
#include "KHttpTransfer.h"
#include "malloc_debug.h"
#include "KCacheStream.h"
#include "KGzip.h"
#include "KContentTransfer.h"
#include "KSelector.h"
#include "KFilterContext.h"
#include "KSubRequest.h"
#include "KHttpFilterManage.h"
#include "cache.h"

KHttpTransfer::KHttpTransfer(KHttpRequest *rq, KHttpObject *obj) {
	init(rq, obj);
}
KHttpTransfer::KHttpTransfer() {
	init(NULL, NULL);
}
void KHttpTransfer::init(KHttpRequest *rq, KHttpObject *obj) {
	isHeadSend = false;
	this->rq = rq;
	this->obj = obj;
	wst = NULL;
	
	wstDelete = false;
	responseChecked = false;
	if (rq) {
		sr = rq->sr;
		workModel = rq->workModel;
	} else {
		sr = NULL;
		workModel = 0;
	}
}
KHttpTransfer::~KHttpTransfer() {
	if (wstDelete && wst) {
		delete wst;
	}
}
KWStream *KHttpTransfer::getWStream()
{
	if(wst){
		return wst;
	}
	wst = makeWriteStream(rq,obj,this,wstDelete);
	assert(wst);
	return wst;
}
StreamState KHttpTransfer::write_all(const char *str, int len) {
	if (len<=0) {
		return STREAM_WRITE_SUCCESS;
	}
	if (!isHeadSend) {
		if (sendHead(false) != STREAM_WRITE_SUCCESS) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	if (st == NULL) {
		return STREAM_WRITE_FAILED;
	}
	return st->write_all(str,len);
}
StreamState KHttpTransfer::write_end() {
	if (preventWriteEnd) {
		return STREAM_WRITE_SUCCESS;
	}
	if (!isHeadSend) {
		if (sendHead(true) != STREAM_WRITE_SUCCESS) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	return KHttpStream::write_end();
}
bool KHttpTransfer::support_sendfile()
{
	if (st != static_cast<KWStream *>(rq)) {
		return false;
	}
	if (rq->c->isSSL()) {
		return false;
	}

	if (!isHeadSend) {
		if (sendHead(false) != STREAM_WRITE_SUCCESS) {
			SET(rq->flags, RQ_CONNECTION_CLOSE);
			return false;
		}
	}
	return true;
}
StreamState KHttpTransfer::sendHead(bool isEnd) {
	INT64 start = 0;
	INT64 send_len = 0;
	StreamState result = STREAM_WRITE_SUCCESS;
	isHeadSend = true;
	INT64 content_len = (isEnd?0:-1);
	cache_layer = cache_memory;
	CLR(rq->flags,RQ_HAVE_RANGE);
	if (TEST(obj->index.flags,ANSW_HAS_CONTENT_LENGTH)) {
		content_len = obj->index.content_length;
		if (content_len > conf.max_cache_size) {
			cache_layer = cache_none;
		}
	}
#ifdef ENABLE_KSAPI_FILTER
	if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL)) {
		//处理http过滤器的过滤
		KHttpStream *raw_send_head = NULL;
		KHttpStream *raw_send_end = NULL;
		KHttpFilterManage::buildSendStream(rq,&raw_send_head,&raw_send_end);
		if (raw_send_head) {
			rq->getOutputFilterContext()->registerFilterStreamEx(raw_send_head,raw_send_end,true);
		}
	}
#endif
	if (rq->needFilter()) {
		/*
		 检查是否需要加载内容变换层，如果要，则长度未知
		 */
		content_len = -1;
		cache_layer = cache_memory;
	}
	gzip_layer = false;
	if (TEST(workModel,WORK_MODEL_INTERNAL) && !TEST(workModel,WORK_MODEL_REPLACE)) {
		/* 子请求,内部但不是替换模式 */
	} else {
		if (check_need_gzip(rq, obj)) {
			if (content_len==-1 || content_len>=conf.min_gzip_length) {	
				SET(rq->flags,RQ_TE_GZIP);
				content_len = -1;
				gzip_layer = true;
				cache_layer = cache_memory;
				obj->insertHttpHeader(kgl_expand_string("Content-Encoding"), kgl_expand_string("gzip"));
				obj->url->set_content_encoding(KGL_ENCODING_GZIP);
			}
		}
		
		build_obj_header(rq, obj, content_len, start, send_len);
#ifndef NDEBUG
		if (TEST(rq->flags,RQ_TE_GZIP)) {
			assert(gzip_layer);
		} else {
			assert(gzip_layer == false);		
		}
#endif
		if (TEST(rq->flags,RQ_SYNC) && rq->send_ctx.getBufferSize()>0) {
			//同步模式发送header
			if (!rq->sync_send_header()) {
				return STREAM_WRITE_FAILED;
			}
		}
	}
	if (obj->in_cache) {
		cache_layer = cache_none;
	} else if (TEST(rq->filter_flags,RF_ALWAYS_ONLINE) && status_code_can_cache(obj->data->status_code)) {
		//永久在线模式
		if (TEST(obj->index.flags, ANSW_NO_CACHE)) {
			//如果是动态网页，并且无Cookie(游客模式)
			if (rq->parser.findHttpHeader(kgl_expand_string("Cookie")) == NULL
				&& rq->parser.findHttpHeader(kgl_expand_string("Cookie2")) == NULL
				&& obj->findHeader(kgl_expand_string("Set-Cookie")) == NULL
				&& obj->findHeader(kgl_expand_string("Set-Cookie2")) == NULL) {
				SET(obj->index.flags, OBJ_MUST_REVALIDATE);
				CLR(obj->index.flags, ANSW_NO_CACHE | FLAG_DEAD);
			}
		}
	}
	loadStream();
	return result;
}
bool KHttpTransfer::loadStream() {
	assert(st==NULL && rq);
	if (sr) {
		st = sr->ctx->st;
	} else {
		st = rq;
	}
	/*
	 从下到上开始串流
	 最下面是限速发送.
	 */
	autoDelete = false;
	/*
	 检查是否有chunk层
	 */
	if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && TEST(rq->flags,RQ_TE_CHUNKED)) {
		KWStream *st2 = new KChunked(st, autoDelete);
		if (st2) {
			st = st2;
			autoDelete = true;
		} else {
			return false;
		}
	}
	//检测是否加载cache层
	
	if (TEST(obj->index.flags,ANSW_NO_CACHE)==0) {
		if (cache_layer == cache_memory) {
			KCacheStream *st_cache = new KCacheStream(st, autoDelete);
			if (st_cache) {
				st_cache->init(obj);
				st = st_cache;
				autoDelete = true;
			}
		}
	}
	//检测是否要加载gzip压缩层
	if (gzip_layer) {
		KGzipCompress *st_gzip = new KGzipCompress(false,st, autoDelete);
		//debug("加载gzip压缩层=%p,up=%p\n",st_gzip,st);
		if (st_gzip) {
			st = st_gzip;
			autoDelete = true;
		} else {
			return false;
		}
	}
	//内容变换层
	if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && rq->needFilter()) {
		//debug("加载内容变换层\n");
		if (rq->of_ctx->head) {
			KContentTransfer *st_content = new KContentTransfer(st, autoDelete);
			if (st_content) {
				st_content->init(rq);
				st = st_content;
				autoDelete = true;
			} else {
				return false;
			}
		}
		KWStream *filter_st_head = rq->of_ctx->getFilterStreamHead();
		if (filter_st_head) {				
			KHttpStream *filter_st_end = rq->of_ctx->getFilterStreamEnd();
			assert(filter_st_end);
			if (filter_st_end) {
				filter_st_end->connect(st,autoDelete);
				//st 由rq->of_ctx管理，所以autoDelete为false
				autoDelete = false;
				st = filter_st_head;
			}
		}		
	}
	//串流完毕
	return true;
}
