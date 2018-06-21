#ifndef KHTTPOBJECTSWAPING_H
#define KHTTPOBJECTSWAPING_H
#include "global.h"
#include "KAsyncFile.h"
#include "KBuffer.h"
enum swap_in_result {
	swap_in_success,
	swap_in_failed,
	swap_in_busy
};
#ifdef ENABLE_DISK_CACHE
class KHttpRequest;
class KHttpObject;
class KHttpObjectBody;
typedef void(*swap_http_obj_call_back)(KHttpRequest *rq, KHttpObject *obj, swap_in_result result);
class KHttpObjectSwapTask {
public:
	KHttpRequest *rq;
	swap_http_obj_call_back cb;
	KHttpObjectSwapTask *next;
};
class KAsyncHttpObjectSwaping
{
public:
	KAsyncHttpObjectSwaping()
	{
		memset(this, 0, sizeof(KAsyncHttpObjectSwaping));
	}
	~KAsyncHttpObjectSwaping();
	void alloc_buffer(size_t buffer_size)
	{
		clean_buffer();
		this->buffer_size = buffer_size;
		this->buffer_left = (int)buffer_size;
		buffer = (char *)aio_alloc_buffer(buffer_size);
	}
	void clean_buffer()
	{
		if (buffer) {
			aio_free_buffer(buffer);
			buffer = NULL;
		}
	}
	
	void swapin_head_body_result(char *buf, int got);
	void swapin_result(swap_in_result result);
	void swapin();
	void swapin_head_body();
	KFile fp;
	KAsyncFile *aio_file;
	KHttpRequest *rq;
	KHttpObject *obj;
	KHttpObjectBody *data;
	swap_http_obj_call_back cb;
	char *buffer;
	char *hot;
	int buffer_left;
	size_t buffer_size;
	buff *last;
	INT64 total_left;
	INT64 offset;
};
class KHttpObjectSwaping
{
public:
	KHttpObjectSwaping()
	{
		queue = NULL;
		aio_swap = NULL;
	}
	~KHttpObjectSwaping()
	{
		assert(queue == NULL);
		if (aio_swap) {
			delete aio_swap;
		}
	}
	void swapin(KHttpRequest *rq, KHttpObject *obj);
	void addTask(KHttpRequest *rq, swap_http_obj_call_back cb)
	{
		KHttpObjectSwapTask *task = new KHttpObjectSwapTask();
		task->rq = rq;
		task->cb = cb;
		task->next = queue;
		queue = task;
	}
	void swapResult(KHttpRequest *rq, KHttpObject *obj, swap_in_result result);
private:
	KHttpObjectSwapTask *queue;
	KAsyncHttpObjectSwaping *aio_swap;
};
#endif
#endif
