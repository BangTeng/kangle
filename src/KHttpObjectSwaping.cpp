#include "KHttpObjectSwaping.h"
#include "KHttpObject.h"
#include "KCache.h"

#ifdef ENABLE_DISK_CACHE
struct swap_in_next_call_param
{
	KHttpObjectSwapTask *queue;
	KHttpObject *obj;
	swap_in_result result;
};
void swap_in_next_call(void *arg, int got)
{
	swap_in_next_call_param *cp = (swap_in_next_call_param *)arg;
	KHttpObjectSwapTask *queue = cp->queue;
	queue->cb(queue->rq, cp->obj, cp->result);
	delete queue;
	delete cp;
}
void swap_obj_result(KHttpRequest *rq, KHttpObject *obj, KHttpObjectBody *data, swap_in_result result)
{
	assert(obj->data->type == SWAPING_OBJECT);
	KHttpObjectBody *osData = obj->data;
	KMutex *lock = obj->getLock();
	lock->Lock();
	obj->data = data;
	data = NULL;
	if (result == swap_in_success) {
		SET(obj->index.flags, FLAG_IN_MEM);
	} else if (result == swap_in_failed) {
		SET(obj->index.flags, FLAG_DEAD | OBJ_INDEX_UPDATE);
	}
	lock->Unlock();
	if (result == swap_in_success) {
		assert(obj->data->status_code > 0);
		if (TEST(obj->index.flags, FLAG_BIG_OBJECT)) {
			cache.getHash(obj->h)->incSize(obj->index.head_size);
		} else {
			cache.getHash(obj->h)->incSize(obj->index.content_length);
		}
	}
	osData->os->swapResult(rq,obj, result);
	delete osData;
}
KTHREAD_FUNCTION handle_sync_swapin(void *param, int msec)
{
	KHttpRequest *rq = (KHttpRequest *)param;
	KHttpObject *obj = rq->ctx->obj;
	assert(obj);
	assert(obj->data->type == SWAPING_OBJECT);
	if (msec < 0 || msec > conf.io_timeout * 1000) {
		swap_obj_result(rq, obj, NULL, swap_in_busy);
		KTHREAD_RETURN;
	}
	KHttpObjectBody *data = new KHttpObjectBody();
	if (obj->swapin(data)) {
		swap_obj_result(rq, obj, data, swap_in_success);
		KTHREAD_RETURN;
	}
	swap_obj_result(rq, obj, data, swap_in_failed);
	KTHREAD_RETURN;
}
void swapin_head_body_callback(KAsyncFile *fp, void *arg, char *buf, int got)
{
	KAsyncHttpObjectSwaping *os = (KAsyncHttpObjectSwaping *)arg;
	os->swapin_head_body_result(buf, got);
}

KAsyncHttpObjectSwaping::~KAsyncHttpObjectSwaping()
{
	if (aio_file) {
		delete aio_file;
	}
	clean_buffer();
	if (data) {
		delete data;
	}
}
void KAsyncHttpObjectSwaping::swapin_head_body_result(char *buf, int got)
{
	if (got <= 0) {
		swapin_result(swap_in_failed);
		return;
	}
	offset += got;
	total_left -= got;
	hot = buf + got;
	int buffer_used = hot - buffer;
	buffer_left = buffer_size - buffer_used;
	if (data->status_code == 0) {
		if (buffer_used < (int)obj->index.head_size) {
			if (!rq->c->selector->aio_read(aio_file, hot, offset, buffer_left, swapin_head_body_callback, this)) {
				swapin_result(swap_in_failed);
			}
			return;
		}
		if (!data->restore_header(obj, buffer, obj->index.head_size)) {
			swapin_result(swap_in_failed);
			return;
		}
		buf = buffer + obj->index.head_size;
		got = buffer_used - obj->index.head_size;
	}
	if (got > 0) {
		buff *tmp = (buff *)malloc(sizeof(buff));
		tmp->used = got;
		tmp->data = (char *)malloc(got);
		memcpy(tmp->data, buf, got);
		tmp->flags = 0;
		tmp->next = NULL;
		if (last == NULL) {
			assert(data->bodys == NULL);
			data->bodys = tmp;
		} else {
			last->next = tmp;
		}
		last = tmp;
	}
	if (total_left <= 0) {
		swapin_result(swap_in_success);
		return;
	}
	if (!rq->c->selector->aio_read(aio_file, buffer, offset, buffer_size, swapin_head_body_callback, this)) {
		swapin_result(swap_in_failed);
	}
}

void KAsyncHttpObjectSwaping::swapin_result(swap_in_result result)
{
	KHttpObjectBody *data = this->data;
	this->data = NULL;
	swap_obj_result(rq, obj, data, result);
}
void KAsyncHttpObjectSwaping::swapin_head_body()
{
	this->total_left = obj->index.content_length + obj->index.head_size;
	if (this->total_left <= 0) {
		swapin_result(swap_in_success);
		return;
	}
	INT64 alloc_size = MIN((INT64)conf.io_buffer, this->total_left);
	alloc_size = MAX(alloc_size, obj->index.head_size);
	alloc_size = kgl_align(alloc_size, kgl_aio_align_size);
	this->alloc_buffer((int)alloc_size);
	if (!rq->c->selector->aio_read(aio_file, buffer, offset, buffer_size, swapin_head_body_callback, this)) {
		swapin_result(swap_in_failed);
	}
}
void KAsyncHttpObjectSwaping::swapin()
{
	if (!is_valide_dc_head_size(obj->index.head_size)) {
		swapin_result(swap_in_failed);
		return;
	}
	offset = 0;
	assert(data == NULL);
	data = new KHttpObjectBody();
	data->create_type(&obj->index);
	
	swapin_head_body();
}
void KHttpObjectSwaping::swapResult(KHttpRequest *rq, KHttpObject *obj, swap_in_result result)
{
	KHttpObjectSwapTask *next;
	while (queue) {
		next = queue->next;
		KHttpRequest *trq = queue->rq;
		if (trq->c->selector == rq->c->selector){
			queue->cb(trq, obj, result);
			delete queue;
		} else {
			swap_in_next_call_param *cp = new swap_in_next_call_param;
			cp->queue = queue;
			cp->obj = obj;
			cp->result = result;
			trq->c->selector->next(swap_in_next_call, cp);
		}
		queue = next;
	}
}
void KHttpObjectSwaping::swapin(KHttpRequest *rq,KHttpObject *obj)
{
	//*
	//if (conf.async_io) {
	aio_swap = new KAsyncHttpObjectSwaping;
	aio_swap->rq = rq;
	aio_swap->obj = obj;
	char *filename = obj->getFileName();
	if (!aio_swap->fp.open(filename, fileRead, KFILE_ASYNC)) {
		free(filename);
		aio_swap->swapin_result(swap_in_failed);
		return;
	}
	free(filename);
	aio_swap->aio_file = rq->c->selector->aio_open(&aio_swap->fp);
	if (aio_swap->aio_file) {
		aio_swap->swapin();
		return;
	}
	delete aio_swap;
	aio_swap = NULL;
	//�˻�Ϊ��ͨswap
	//}
	//*/
	if (!conf.ioWorker->tryStart(rq, handle_sync_swapin)) {
		handle_sync_swapin(rq, -1);
	}
}
#endif
