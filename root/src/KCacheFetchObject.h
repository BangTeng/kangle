#ifndef KCACHEFETCHOBJECT_H
#define KCACHEFETCHOBJECT_H
#include "KFetchObject.h"
#include "KBuffer.h"
#include "http.h"
#include "KSubRequest.h"
/**
* 缓存物件数据源，仅用于内部请求命中
*/
class KCacheFetchObject : public KFetchObject
{
public:
	KCacheFetchObject(KHttpObject *obj)
	{
		header = NULL;
		this->obj = obj;
		obj->addRef();
	}
	~KCacheFetchObject()
	{
		if (header) {
			free(header);
		}
		if (obj) {
			obj->release();
		}
	}
	void open(KHttpRequest *rq)
	{
		KFetchObject::open(rq);		
		if (obj->data->bodys==NULL) {
			stage_rdata_end(rq, STREAM_WRITE_END);	
			return;
		}
		//buff *send_buffer = obj->data->bodys;
		INT64 content_len = obj->index.content_length;
		INT64 send_len = content_len;
		INT64 start = 0;
		hot_buffer = obj->data->bodys;
		if (TEST(rq->flags,RQ_HAVE_RANGE)) {
			if(!adjust_range(rq,send_len)){
				handleError(rq,416,"range error");
				return;
			}
			start = rq->range_from;
			//seek
			while (hot_buffer) {
				if (start==0) {
					break;
				}
				if (start < hot_buffer->used) {
					assert(header==NULL);
					header = (buff *)xmalloc(sizeof(buff));
					memset(header,0,sizeof(buff));
					header->skip_data_free = 1;
					header->data = hot_buffer->data + start;
					header->used = hot_buffer->used - (int)start;
					header->next = hot_buffer->next;
					hot_buffer = header;
					break;
				}
				start -= hot_buffer->used;
				hot_buffer = hot_buffer->next;
			}
		}
		left = (int)send_len;
		readBody(rq);
	}
	void readBody(KHttpRequest *rq)
	{
		if (hot_buffer == NULL) {
			stage_rdata_end(rq, STREAM_WRITE_END);
			return;
		}
		KWStream *st = NULL;
		if (rq->sr) {
			st = rq->sr->ctx->st;
		} else {
			st = rq->ctx->st;
		}
		assert(st);
		while (hot_buffer && hot_buffer->used>0 && left>0) {
			buff *buf = hot_buffer;
			hot_buffer = hot_buffer->next;
			int send_len = MIN(left,buf->used);
			StreamState result = st->write_all(buf->data,send_len);
			left -= send_len;
			if (result==STREAM_WRITE_FAILED) {
				SET(rq->flags,RQ_CONNECTION_CLOSE);
				break;
			}
			if (try_send_request(rq)) {
				return;
			}
		}
		stage_rdata_end(rq, STREAM_WRITE_END);
	}
private:
	buff *header;
	int left;
	buff *hot_buffer;
	KHttpObject *obj;
};
#endif
