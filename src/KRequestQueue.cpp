#include <string.h>
#include <string>
#include "KRequestQueue.h"
#include "KThreadPool.h"
#include "KSelectable.h"
#include "http.h"
#include "KSelector.h"
#include "malloc_debug.h"
#ifdef ENABLE_REQUEST_QUEUE
/*
����һ��ȫ�ֵĶ���
*/
KRequestQueue globalRequestQueue;
//��Ҫ��չ���еģ��첽����ʼ
inline void stage_async_need_queue(KHttpRequest *rq)
{
	//����ʱ�����¼�ʱ
	rq->request_msec = kgl_current_msec;
	rq->fetchObj->open(rq);
}
void resultQueuedRequestTimeOut(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	if (got<0) {
		SET(rq->flags,RQ_CONNECTION_CLOSE);
		stageEndRequest(rq);
		return;
	}
	rq->closeFetchObject();
	send_error(rq,NULL,STATUS_SERVICE_UNAVAILABLE,"Server is busy");
}
bool checkQueuedRequestTimeOut(KHttpRequest *rq)
{
	if (kgl_current_msec - rq->request_msec > conf.time_out * 1000) {
		CLR(rq->flags,RQ_SYNC);
		//cut the stack call.
		//rq->c->handler = handleQueuedRequestTimeOut;
		//rq->c->selector->addRequest(rq,KGL_LIST_RW,STAGE_OP_NEXT);
		rq->c->next(resultQueuedRequestTimeOut,rq);
		return true;
	}
	return false;
}
void resultAsyncNextRequest(void *arg,int got)
{
	KHttpRequest *rq = (KHttpRequest *)arg;
	assert(rq->fetchObj && !rq->fetchObj->isSync());
	stage_async_need_queue(rq);
}
/*
�����̹߳������ͷŶ��У���������������rq�ǹ����߳����⡣
*/
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
		if(!m_thread.start(rq,stage_sync)){
			rq->queue = NULL;
			SET(rq->flags,RQ_CONNECTION_CLOSE);
			queue->decWorker();
			stageEndRequest(rq);
		}
	} else {
		//����Ҫ�ж϶�ջ�������п��ܻ��й���ĵ��á�
		rq->c->next(resultAsyncNextRequest,rq);
	}
}
FUNC_TYPE FUNC_CALL thread_queue(void *param)
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
			//ͬ�������̵߳����첽���󣬹������ʴ����첽���󡣱����˳���
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
	return m_thread.start(rq,thread_queue);	
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
			klog(KLOG_DEBUG,"add %p to queue\n",(KSelectable *)rq);
#endif
			rq->setState(STATE_QUEUE);
			queue.push_back(rq);
			result = true;
			if(!TEST(rq->flags,RQ_SYNC)){
				//in queue
				rq->c->removeRequest(rq,true);
			}
		}
	}
	refsLock.Unlock();
	//startResult ��ʶΪֱ�Ӵ������󣬵����(result)δ֪��
	//result = trueʱ����Ѿ���֪��,ֱ�ӷŵ�queue�С�
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
