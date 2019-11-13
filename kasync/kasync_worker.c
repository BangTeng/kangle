#include <assert.h>
#include "kasync_worker.h"
#include "kthread.h"
#include "kmalloc.h"

kasync_worker *kasync_worker_init(int max_worker, int max_queue)
{
	kasync_worker *worker = (kasync_worker *)xmalloc(sizeof(kasync_worker));
	memset(worker, 0, sizeof(kasync_worker));
	worker->max_worker = max_worker;
	worker->max_queue = max_queue;
	kmutex_init(&worker->lock, NULL);
	worker->refs = 1;
	return worker;
}
void kasync_worker_refs(kasync_worker *worker)
{
	katom_inc((void *)&worker->refs);
}
void kasync_worker_release(kasync_worker *worker)
{
	if (katom_dec((void *)&worker->refs) == 0) {
		kmutex_destroy(&worker->lock);
		xfree(worker);
	}
}
static void kasync_worker_param_destroy(kasync_worker_param *param)
{
	xfree(param);
}
KTHREAD_FUNCTION kasync_worker_worker_thread(void *param)
{
	kasync_worker *worker = (kasync_worker *)param;
	for (;;) {
		kasync_worker_param *rq = NULL;
		kmutex_lock(&worker->lock);
		if (worker->head == NULL) {
			worker->worker--;
			kmutex_unlock(&worker->lock);
			break;
		}
		worker->queue--;
		rq = worker->head;
		worker->head = worker->head->next;
		if (worker->head == NULL) {
			worker->last = NULL;
		}
		kmutex_unlock(&worker->lock);
		/*
		int msec = (int)(kgl_current_msec - rq->startTime);
		if (msec < 0) {
			msec = 0;
		}
		*/
		int msec = 1;
		if (rq->wait) {
			kcond_notice(rq->wait);
		}
		rq->cb(rq->data, msec);
		kasync_worker_param_destroy(rq);
	}
	kasync_worker_release(worker);
	KTHREAD_RETURN;
}
typedef struct  {
	void *param;
	kasync_worker_callback cb;
} thread_start_worker_param;

KTHREAD_FUNCTION thread_start_worker_thread(void *param)
{
	thread_start_worker_param *p = (thread_start_worker_param *)param;
	p->cb(p->param, 0);
	xfree(p);
	KTHREAD_RETURN;
}
bool kasync_worker_thread_start(void *param, kasync_worker_callback cb)
{
	thread_start_worker_param *p = xmemory_new(thread_start_worker_param);
	p->param = param;
	p->cb = cb;
	if (kthread_pool_start(thread_start_worker_thread,p)) {
		return true;
	}
	xfree(p);
	return false;
}
static void kasync_worker_add(kasync_worker *worker, kasync_worker_param *rq, bool high)
{
	worker->queue++;
	if (worker->last == NULL) {
		kassert(worker->head == NULL);
		worker->head = rq;
		worker->last = rq;
	} else {
		if (high) {
			//push head
			rq->next = worker->head;
			worker->head = rq;
		} else {
			//push end
			worker->last->next = rq;
			worker->last = rq;
		}
	}
	if (worker->worker >= worker->max_worker) {
		return;
	}
	worker->worker++;
	kasync_worker_refs(worker);
	if (!kthread_pool_start(kasync_worker_worker_thread,worker)) {
		worker--;
		kasync_worker_release(worker);
	}
	return;
}
bool kasync_worker_try_start(kasync_worker *worker, void *data, kasync_worker_callback cb, bool high)
{
	kmutex_lock(&worker->lock);
	if (worker->max_queue > 0 && worker->queue > worker->max_queue) {
		kmutex_unlock(&worker->lock);
		return false;
	}
	kasync_worker_param *rq = (kasync_worker_param *)xmalloc(sizeof(kasync_worker_param));
	memset(rq, 0, sizeof(kasync_worker_param));
	//rq->start_time = kgl_current_msec;
	rq->cb = cb;
	rq->data = data;
	rq->next = NULL;
	rq->wait = NULL;
	kasync_worker_add(worker,rq, high);
	kmutex_unlock(&worker->lock);
	return true;
}
bool kasync_worker_empty(kasync_worker *worker)
{
	kmutex_lock(&worker->lock);
	bool result = (worker->head == NULL);
	kmutex_unlock(&worker->lock);
	return result;
}
void kasync_worker_start(kasync_worker *worker, void *data, kasync_worker_callback cb)
{
	kcond *wait = NULL;
	kasync_worker_param *rq = (kasync_worker_param *)xmalloc(sizeof(kasync_worker_param));
	memset(rq, 0, sizeof(kasync_worker_param));
	rq->cb = cb;
	rq->data = data;
	rq->next = NULL;
	kmutex_lock(&worker->lock);
	if (worker->max_queue > 0 && worker->queue > worker->max_queue) {
		wait = kcond_init(true);
	}
	rq->wait = wait;
	kasync_worker_add(worker, rq, false);
	kmutex_unlock(&worker->lock);
	if (wait != NULL) {
		kcond_wait(wait);
		kcond_destroy(wait);
	}
}
