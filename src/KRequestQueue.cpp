#include <string.h>
#include <string>
#include "KRequestQueue.h"
#include "kthread.h"
#include "kselectable.h"
#include "http.h"
#include "kselector.h"
#include "kmalloc.h"
#ifdef ENABLE_REQUEST_QUEUE
KRequestQueue globalRequestQueue;
inline kev_result stage_async_need_queue(KHttpRequest *rq)
{
	rq->begin_time_msec = kgl_current_msec;
	rq->sink->RemoveSync();
	return rq->fetchObj->open(rq);
}
kev_result resultQueuedRequestTimeOut(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got<0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		return stageEndRequest(rq);
	}
	rq->closeFetchObject();
	return send_error(rq,NULL,STATUS_SERVICE_UNAVAILABLE,"Server is busy");
}
bool checkQueuedRequestTimeOut(KHttpRequest *rq)
{
	if (kgl_current_msec - rq->begin_time_msec > conf.time_out * 1000) {
		CLR(rq->flags,RQ_SYNC);
		kgl_selector_module.next(rq->sink->GetSelector(),resultQueuedRequestTimeOut,rq,0);
		return true;
	}
	return false;
}
kev_result resultAsyncNextRequest(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	assert(rq->fetchObj && !rq->fetchObj->isSync());
	return stage_async_need_queue(rq);
}

void async_queue_destroy(KRequestQueue *queue)
{
	//KRequestQueue *queue = rq->ctx->queue;

	assert(queue);
	//rq->ctx->queue = NULL;
	KHttpRequest *rq = queue->getQueue();
	if (rq == NULL || checkQueuedRequestTimeOut(rq)) {		
		return;
	}
	assert(rq->queue == queue);
	if (TEST(rq->flags,RQ_SYNC)) {		
		if(!kthread_pool_start(stage_sync, rq)){
			rq->queue = NULL;
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			queue->decWorker();
			stageEndRequest(rq);
		}
	} else {
		kgl_selector_module.next(rq->sink->GetSelector(),resultAsyncNextRequest,rq,0);
	}
}
KTHREAD_FUNCTION thread_queue(void *param)
{
	//debug("thread_queue process %p...\n",param);
	KHttpRequest *rq = (KHttpRequest *)param;	
	for(;;){
		KRequestQueue *queue = rq->queue;
		assert(queue);
		if (TEST(rq->flags,RQ_SYNC)) {
			rq->queue = NULL;
			stage_sync(rq);
			rq = queue->getQueue();
			queue->release();
			if (rq == NULL || checkQueuedRequestTimeOut(rq)) {
				break;
			}
		} else {
			stage_async_need_queue(rq);
			break;
		}
	
	}
	KTHREAD_RETURN;
}
KRequestQueue::KRequestQueue()
{
	cur_worker = 0;
	max_worker = 0;
	max_queue = 0;
}
KRequestQueue::~KRequestQueue()
{
	assert(queue.empty());
}
void KRequestQueue::set(unsigned max_worker,unsigned max_queue)
{
	this->max_worker = max_worker;
	this->max_queue = max_queue;
}
bool KRequestQueue::startDirect(KHttpRequest *rq)
{
	if(!TEST(rq->flags,RQ_SYNC)){
		stage_async_need_queue(rq);
		return true;
	}
	return kthread_pool_start(thread_queue, rq);
}
bool KRequestQueue::start(KHttpRequest *rq)
{
	bool result = false;
	SET(rq->flags,RQ_QUEUED);
	bool startResult = false;
	rq->ctx->queue_handled = true;
	refsLock.Lock();
	if (rq->queue != NULL) {
		assert(rq->queue == this);
		rq->queue = NULL;
		refs --;
	}
	if (max_worker == 0 || cur_worker < max_worker) {
		rq->queue = this;
		refs ++;
		cur_worker ++;
		startResult = true;
	} else {
		if(max_queue == 0 || queue.size() < max_queue){
			//printf("add to queue..............\n");
#ifndef NDEBUG
			klog(KLOG_DEBUG,"add %p to queue\n",rq);
#endif
			rq->setState(STATE_QUEUE);
			queue.push_back(rq);
			result = true;
			if(!TEST(rq->flags,RQ_SYNC)){
				//in queue
				rq->sink->AddSync();
			}
		}
	}
	refsLock.Unlock();
	if (startResult) {
		result = startDirect(rq);
		if (!result) {
			rq->queue = NULL;
			refsLock.Lock();
			refs --;
			cur_worker --;
			refsLock.Unlock();
		}
	}
	return result;
}
KHttpRequest *KRequestQueue::getQueue()
{
	KHttpRequest *context;
	refsLock.Lock();
	if ((max_worker == 0 || cur_worker <= max_worker) && queue.size()>0) {
		context = *(queue.begin());
		queue.pop_front();
		assert(context);
		context->queue = this;
		refs++;
		//debug("***********************get from queue size=%d\n",queue.size());
	} else {
		context = NULL;
		cur_worker --;
	}
	refsLock.Unlock();
	return context;
}
#endif
