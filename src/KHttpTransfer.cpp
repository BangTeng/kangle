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
		//����http�������Ĺ���
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
		 ����Ƿ���Ҫ�������ݱ任�㣬���Ҫ���򳤶�δ֪
		 */
		content_len = -1;
		cache_layer = cache_memory;
	}
	gzip_layer = false;
	if (TEST(workModel,WORK_MODEL_INTERNAL) && !TEST(workModel,WORK_MODEL_REPLACE)) {
		/* ������,�ڲ��������滻ģʽ */
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
			//ͬ��ģʽ����header
			if (!rq->sync_send_header()) {
				return STREAM_WRITE_FAILED;
			}
		}
	}
	if (obj->in_cache) {
		cache_layer = cache_none;
	} else if (TEST(rq->filter_flags,RF_ALWAYS_ONLINE) && status_code_can_cache(obj->data->status_code)) {
		//��������ģʽ
		if (TEST(obj->index.flags, ANSW_NO_CACHE)) {
			//����Ƕ�̬��ҳ��������Cookie(�ο�ģʽ)
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
	 ���µ��Ͽ�ʼ����
	 �����������ٷ���.
	 */
	autoDelete = false;
	/*
	 ����Ƿ���chunk��
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
	//����Ƿ����cache��
	
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
	//����Ƿ�Ҫ����gzipѹ����
	if (gzip_layer) {
		KGzipCompress *st_gzip = new KGzipCompress(false,st, autoDelete);
		//debug("����gzipѹ����=%p,up=%p\n",st_gzip,st);
		if (st_gzip) {
			st = st_gzip;
			autoDelete = true;
		} else {
			return false;
		}
	}
	//���ݱ任��
	if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && rq->needFilter()) {
		//debug("�������ݱ任��\n");
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
				//st ��rq->of_ctx��������autoDeleteΪfalse
				autoDelete = false;
				st = filter_st_head;
			}
		}		
	}
	//�������
	return true;
}
