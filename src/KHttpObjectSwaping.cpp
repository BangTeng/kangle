#include "KHttpObjectSwaping.h"
#include "KHttpObject.h"
#include "KCache.h"
#include "kfile.h"

#ifdef ENABLE_DISK_CACHE
struct swap_in_next_call_param
{
	KHttpObjectSwapTask *queue;
	KHttpObject *obj;
	swap_in_result result;
};
kev_result swap_in_next_call(void *arg, int got)
{
	swap_in_next_call_param *cp = (swap_in_next_call_param *)arg;
	KHttpObjectSwapTask *queue = cp->queue;
	kev_result ret = queue->cb(queue->rq, cp->obj, cp->result);
	delete queue;
	delete cp;
	return ret;
}
kev_result swap_obj_result(KHttpRequest *rq, KHttpObject *obj, KHttpObjectBody *data, swap_in_result result)
{
	assert(obj->data->type == SWAPING_OBJECT);
	KHttpObjectBody *osData = obj->data;
	KMutex *lock = obj->getLock();
	lock->Lock();
	obj->data = data;
	data = NULL;
	if (result == swap_in_success) {
		SET(obj->index.flags, FLAG_IN_MEM);
	} else if (result != swap_in_busy) {
		SET(obj->index.flags, FLAG_DEAD);
	}
	lock->Unlock();
	if (result == swap_in_success) {
		assert(obj->data->status_code > 0);
		INT64 inc_size = obj->index.head_size;
		if (obj->data->type == MEMORY_OBJECT) {
			inc_size += obj->index.content_length;
		}
		cache.getHash(obj->h)->incSize(inc_size);
	}
	kev_result ret = osData->os->swapResult(rq,obj, result);
	delete osData;
	return ret;
}
kev_result handle_sync_swapin(void *param, int msec)
{
	KHttpRequest *rq = (KHttpRequest *)param;
	KHttpObject *obj = rq->ctx->obj;
	assert(obj);
	assert(obj->data->type == SWAPING_OBJECT);
	if (msec < 0 || msec > conf.io_timeout * 1000) {
		return swap_obj_result(rq, obj, NULL, swap_in_busy);
	}
	KHttpObjectBody *data = new KHttpObjectBody();
	if (obj->swapin(data)) {
		return swap_obj_result(rq, obj, data, swap_in_success);
	}
	return swap_obj_result(rq, obj, data, swap_in_failed_other);
}
kev_result swapin_head_body_callback(kasync_file *fp, void *arg, char *buf, int got)
{
	KAsyncHttpObjectSwaping *os = (KAsyncHttpObjectSwaping *)arg;
	return os->swapin_head_body_result(buf, got);
}
#ifdef ENABLE_BIG_OBJECT
kev_result swapin_head_callback(kasync_file *fp, void *arg, char *buf, int got)
{
	KAsyncHttpObjectSwaping *os = (KAsyncHttpObjectSwaping *)arg;
	return os->swapin_head_result(buf, got);
}
#endif
#ifdef ENABLE_BIG_OBJECT_206
kev_result swap_in_progress_callback(kasync_file *fp, void *arg, char *buf, int got)
{
	KAsyncHttpObjectSwaping *os = (KAsyncHttpObjectSwaping *)arg;
	return os->swapin_progress_result(buf, got);
}

kev_result KAsyncHttpObjectSwaping::swapin_progress_result(char *buf, int got)
{
	if (got <= 0) {
		return swapin_result(swap_in_failed_read);
	}
	offset += got;
	buffer_left -= got;
	hot = buf + got;
	total_left -= got;
	if (buffer_left > 0) {		
		if (!kgl_selector_module.aio_read(rq->sink->GetSelector(), aio_file, hot, offset, buffer_left, swap_in_progress_callback, this)) {
			return swapin_result(swap_in_failed_read);
		}
		return kev_ok;
	}
	if (!data->sbo->restore(buffer, buffer_size)) {
		return swapin_result(swap_in_failed_parse);
	}
	return swapin_result(swap_in_success);
}
kev_result KAsyncHttpObjectSwaping::swapin_proress()
{
	kasync_file_close(aio_file);
	aio_file = NULL;
	offset = 0;
	char *filename = obj->getFileName(true);
	if (filename == NULL) {
		return swapin_result(swap_in_failed_other);
	}
	FILE_HANDLE fp = kfopen(filename, fileRead, KFILE_ASYNC);
	free(filename);
	if (!kflike(fp)) {
		return swapin_result(swap_in_failed_open);
	}
	aio_file = kgl_selector_module.aio_open(rq->sink->GetSelector(),fp);
	if (aio_file == NULL) {
		kfclose(fp);
		return swapin_result(swap_in_failed_open);
	}
	int size = (int)kfsize(fp);
	alloc_buffer(MIN(16384, size));
	if (!kgl_selector_module.aio_read(rq->sink->GetSelector(), aio_file, buffer, 0, buffer_size, swap_in_progress_callback, this)) {
		return swapin_result(swap_in_failed_read);
	}
	return kev_ok;
}
#endif
#ifdef ENABLE_BIG_OBJECT
kev_result KAsyncHttpObjectSwaping::swapin_head_result(char *buf, int got)
{
	//printf("swap_head_result buffer=[%p],buf=[%p],got=[%d]\n",buffer,buf,got);
	if (got <= 0) {
		return swapin_result(swap_in_failed_read);
	}
	offset += got;
	buffer_left -= got;
	hot = buf + got;
	if (buffer_left > 0) {
		if (!kgl_selector_module.aio_read(rq->sink->GetSelector(),aio_file, hot, offset, buffer_left, swapin_head_callback, this)) {
			return swapin_result(swap_in_failed_read);
		}
		return kev_ok;
	}
	if (!data->restore_header(obj, buffer, buffer_size)) {
		return swapin_result(swap_in_failed_parse);
	}
#ifdef ENABLE_BIG_OBJECT_206
	if (data->type == BIG_OBJECT_PROGRESS) {
		return swapin_proress();
	}
#endif
	return swapin_result(swap_in_success);
}
kev_result KAsyncHttpObjectSwaping::swapin_head()
{
	this->alloc_buffer(obj->index.head_size);
	if (!kgl_selector_module.aio_read(rq->sink->GetSelector(), aio_file, buffer, 0, buffer_size, swapin_head_callback, this)) {
		return swapin_result(swap_in_failed_read);
	}
	return kev_ok;
}
#endif
KAsyncHttpObjectSwaping::~KAsyncHttpObjectSwaping()
{
	if (aio_file) {
		kasync_file_close(aio_file);
	}
	clean_buffer();
	if (data) {
		delete data;
	}
}
kev_result KAsyncHttpObjectSwaping::swapin_head_body_result(char *buf, int got)
{
	if (got <= 0) {
		return swapin_result(swap_in_failed_read);
	}
	offset += got;
	total_left -= got;
	hot = buf + got;
	int buffer_used = hot - buffer;
	buffer_left = buffer_size - buffer_used;
	if (data->status_code == 0) {
		if (buffer_used < (int)obj->index.head_size) {			
			if (!kgl_selector_module.aio_read(rq->sink->GetSelector(), aio_file, hot, offset, buffer_left, swapin_head_body_callback, this)) {
				return swapin_result(swap_in_failed_read);
			}
			return kev_ok;
		}
		if (!data->restore_header(obj, buffer, obj->index.head_size)) {
			return swapin_result(swap_in_failed_parse);
		}
		buf = buffer + obj->index.head_size;
		got = buffer_used - obj->index.head_size;
	}
	if (got > 0) {
		kbuf *tmp = (kbuf *)malloc(sizeof(kbuf));
		tmp->used = got;
		tmp->data = (char *)malloc(got);
		kgl_memcpy(tmp->data, buf, got);
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
		return swapin_result(swap_in_success);
	}
	if (!kgl_selector_module.aio_read(rq->sink->GetSelector(), aio_file, buffer, offset, buffer_size, swapin_head_body_callback, this)) {
		return swapin_result(swap_in_failed_read);
	}
	return kev_ok;
}

kev_result KAsyncHttpObjectSwaping::swapin_result(swap_in_result result)
{
	KHttpObjectBody *data = this->data;
	this->data = NULL;
	return swap_obj_result(rq, obj, data, result);
}
kev_result KAsyncHttpObjectSwaping::swapin_head_body()
{
	this->total_left = obj->index.content_length + obj->index.head_size;
	if (this->total_left <= 0) {
		return swapin_result(swap_in_success);
	}
	INT64 alloc_size = MIN((INT64)conf.io_buffer, this->total_left);
	alloc_size = MAX(alloc_size, obj->index.head_size);
	alloc_size = kgl_align(alloc_size, kgl_aio_align_size);
	this->alloc_buffer((int)alloc_size);	
	if (!kgl_selector_module.aio_read(rq->sink->GetSelector(), aio_file, buffer, offset, buffer_size, swapin_head_body_callback, this)) {
		return swapin_result(swap_in_failed_read);
	}
	return kev_ok;
}
kev_result KAsyncHttpObjectSwaping::swapin()
{
	if (!is_valide_dc_head_size(obj->index.head_size)) {
		return swapin_result(swap_in_failed_format);
	}
	offset = 0;
	assert(data == NULL);
	data = new KHttpObjectBody();
	data->create_type(&obj->index);
	
	return swapin_head_body();
}
kev_result KHttpObjectSwaping::swapResult(KHttpRequest *rq, KHttpObject *obj, swap_in_result result)
{
	kev_result ret = kev_ok;
	KHttpObjectSwapTask *next;
	while (queue) {
		next = queue->next;
		KHttpRequest *trq = queue->rq;
		if (trq->sink->GetSelector() == rq->sink->GetSelector()){
			ret = queue->cb(trq, obj, result);
			delete queue;
		} else {
			swap_in_next_call_param *cp = new swap_in_next_call_param;
			cp->queue = queue;
			cp->obj = obj;
			cp->result = result;
			kgl_selector_module.next(trq->sink->GetSelector(),swap_in_next_call, cp,0);
		}
		queue = next;
	}
	return ret;
}
kev_result KHttpObjectSwaping::swapin(KHttpRequest *rq,KHttpObject *obj)
{
	aio_swap = new KAsyncHttpObjectSwaping;
	aio_swap->rq = rq;
	aio_swap->obj = obj;
	char *filename = obj->getFileName();
	FILE_HANDLE fp = kfopen(filename, fileRead, KFILE_ASYNC);
	if (!kflike(fp)) {	
		free(filename);
		return aio_swap->swapin_result(swap_in_failed_open);
	}
	free(filename);	
	aio_swap->aio_file = kgl_selector_module.aio_open(rq->sink->GetSelector(), fp);
	if (aio_swap->aio_file) {
		return aio_swap->swapin();
	}
	delete aio_swap;
	aio_swap = NULL;	
	if (!kasync_worker_try_start(conf.ioWorker, rq, handle_sync_swapin, false)) {
		return handle_sync_swapin(rq, -1);
	}
	return kev_ok;
}
#endif
