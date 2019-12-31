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
#include "kmalloc.h"
#include "KCacheStream.h"
#include "KGzip.h"
#include "KContentTransfer.h"
#include "kselector.h"
#include "KFilterContext.h"
#include "KHttpFilterManage.h"

#include "cache.h"
static kev_result stage_request_write_clean(KHttpRequest *rq)
{
	if (rq->fetchObj && !rq->fetchObj->isClosed()) {
		
		//if (rq->ctx->connection_upgrade) {
		///	rq->sink->Flush();
		//}
		return rq->fetchObj->readBody(rq);
	}
	return stageEndRequest(rq);
}
int buffer_http_transfer(void *arg, LPWSABUF buf, int bc)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	return rq->tr->buffer.getReadBuffer(buf, bc);
}
kev_result result_http_transfer(void *arg, int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got < 0) {
		return stageEndRequest(rq);
	}
	if (got == 0) {
		return stage_request_write_clean(rq);
	}
	rq->AddDownFlow(got);
	if (rq->tr->buffer.readSuccess(got)) {
		return rq->Write(rq, result_http_transfer, buffer_http_transfer);
	}
	return stage_request_write_clean(rq);
}
bool KHttpTransfer::TrySyncWrite()
{
	kbuf *buf = buffer.stealBuff();
	if (buf) {
		bool result = rq->WriteBuff(buf);
		destroy_kbuf(buf);
		return result;
	}
	return true;
}
kev_result KHttpTransfer::TryWrite()
{
	kassert(rq->tr == this);
	if (buffer.getLen() > 0) {
		return rq->Write(rq, result_http_transfer, buffer_http_transfer);
	}
	if (rq->sink->HasHeaderDataToSend()) {
		return rq->Write(rq, result_http_transfer, NULL);
	}
	if (rq->HasWriteHook()) {
		rq->write_stack->arg = rq;
		rq->write_stack->result = result_http_transfer;
		return kgl_call_write_hook(rq, 0);
	}
	return kev_err;
}
KHttpTransfer::KHttpTransfer(KHttpRequest *rq, KHttpObject *obj) {
	init(rq, obj);
}
KHttpTransfer::KHttpTransfer() {
	init(NULL, NULL);
}
void KHttpTransfer::init(KHttpRequest *rq, KHttpObject *obj) {
	this->rq = rq;
	this->obj = obj;
}
KHttpTransfer::~KHttpTransfer() {
	
}

StreamState KHttpTransfer::write_all(const char *str, int len) {
	if (len<=0) {
		return STREAM_WRITE_SUCCESS;
	}
	if (st==NULL) {
		if (sendHead(false) != STREAM_WRITE_SUCCESS) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	if (st == NULL) {
		return STREAM_WRITE_FAILED;
	}
	return st->write_all(str, len);
}
StreamState KHttpTransfer::write_end() {
	if (st == NULL) {
		if (sendHead(true) != STREAM_WRITE_SUCCESS) {
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			return STREAM_WRITE_FAILED;
		}
	}
	return KHttpStream::write_end();
}
bool KHttpTransfer::support_sendfile()
{
	return false;
}
StreamState KHttpTransfer::sendHead(bool isEnd) {
	INT64 start = 0;
	INT64 send_len = 0;
	StreamState result = STREAM_WRITE_SUCCESS;
	INT64 content_len = (isEnd?0:-1);
	cache_model cache_layer = cache_memory;
	kassert(!TEST(rq->flags, RQ_HAVE_RANGE) || rq->ctx->obj->data->status_code==STATUS_CONTENT_PARTIAL);
	if (TEST(obj->index.flags,ANSW_HAS_CONTENT_LENGTH)) {
		content_len = obj->index.content_length;
#ifdef ENABLE_DISK_CACHE
		if (content_len > conf.max_cache_size) {
			if (rq->IsSync() || content_len > conf.max_bigobj_size) {
				cache_layer = cache_none;
			} else {
				cache_layer = cache_disk;
			}
		}
#else
		cache_layer = cache_none;
#endif
	}
	if (rq->needFilter()) {
		/*
		 ����Ƿ���Ҫ�������ݱ任�㣬���Ҫ���򳤶�δ֪
		 */
		content_len = -1;
		cache_layer = cache_memory;
	}
	bool gzip_layer = false;
	if (unlikely(rq->ctx->internal && !rq->ctx->replace)) {
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
#ifdef WORK_MODEL_TCP
		//�˿�ӳ�䲻����httpͷ
		if (!TEST(rq->GetWorkModel(), WORK_MODEL_TCP))
#endif
		build_obj_header(rq, obj, content_len, start, send_len);
#ifndef NDEBUG
		if (TEST(rq->flags,RQ_TE_GZIP)) {
			assert(gzip_layer);
		} else {
			assert(gzip_layer == false);
		}
#endif
	}
	if (obj->in_cache) {
		cache_layer = cache_none;
	}
#if 0
	else if (TEST(rq->filter_flags,RF_ALWAYS_ONLINE) && status_code_can_cache(obj->data->status_code)) {
		//��������ģʽ
#if 0
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
#endif
	}
#endif
	loadStream(gzip_layer,cache_layer);
	return result;
}
bool KHttpTransfer::loadStream(bool gzip_layer, cache_model cache_layer) {

	/*
	 ���µ��Ͽ�ʼ����
	 �����������ٷ���.
	 */
	autoDelete = false;

	assert(st==NULL && rq);
#ifdef ENABLE_TF_EXCHANGE
	if (rq->tf) {
		st = rq->tf;
	} else
#endif
	st = &buffer;

	/*
	 ����Ƿ���chunk��
	 */
	if ((!rq->ctx->internal || rq->ctx->replace) && TEST(rq->flags, RQ_TE_CHUNKED)) {
	//if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && TEST(rq->flags,RQ_TE_CHUNKED)) {
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
		if (cache_layer != cache_none) {
			KCacheStream *st_cache = new KCacheStream(st, autoDelete);
			if (st_cache) {
				st_cache->init(rq,obj,cache_layer);
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
	if ((!rq->ctx->internal || rq->ctx->replace) && rq->needFilter()) {
	//if (!(TEST(workModel,WORK_MODEL_INTERNAL|WORK_MODEL_REPLACE)==WORK_MODEL_INTERNAL) && rq->needFilter()) {
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
