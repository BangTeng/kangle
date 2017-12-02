#include "KAsyncWorker.h"
#include "KThreadPool.h"
FUNC_TYPE FUNC_CALL kasyncWorkerThread(void *param)
{
	KAsyncWorker *worker = (KAsyncWorker *)param;
	worker->workThread();
	KTHREAD_RETURN;
}
struct thread_start_worker_param {
	void *param;
	asyncWorkerCallBack callback;
};
FUNC_TYPE FUNC_CALL thread_start_worker_thread(void *param)
{
	thread_start_worker_param *p = (thread_start_worker_param *)param;
	p->callback(p->param, 0);
	delete p;
	KTHREAD_RETURN;
}
bool thread_start_worker(void *param, asyncWorkerCallBack callback)
{
	thread_start_worker_param *p = new thread_start_worker_param;
	p->param = param;
	p->callback = callback;
	if (m_thread.start(p, thread_start_worker_thread)) {
		return true;
	}
	delete p;
	return false;
}
void KAsyncWorker::start(void *data,asyncWorkerCallBack callBack,bool high)
{
	KAsyncParam *rq = new KAsyncParam;
	rq->startTime = kgl_current_msec;
	rq->callBack = callBack;
	rq->data = data;
	rq->next = NULL;
	lock.Lock();
	queue++;
	if (last==NULL) {
		assert(head==NULL);
		head = rq;
		last = rq;
	} else {
		if (high) {
			//push head
			rq->next = head;
			head = rq;
		} else {
			//push end
			last->next = rq;
			last = rq;
		}
	}
	if (worker>=maxWorker) {
		lock.Unlock();
		return;
	}
	worker++;
	lock.Unlock();
	addRef();
	if (!m_thread.start(this,kasyncWorkerThread)) {
		lock.Lock();
		worker--;
		lock.Unlock();
		release();
	}
}
void KAsyncWorker::workThread()
{
	for (;;) {
		KAsyncParam *rq = NULL;
		lock.Lock();
		if (head==NULL) {
			worker--;
			lock.Unlock();
			break;
		}
		queue--;
		rq = head;
		head = head->next;
		if (head==NULL) {
			last = NULL;
		}
		lock.Unlock();
		rq->callBack(rq->data,(int)(kgl_current_msec - rq->startTime));
		delete rq;
	}
	release();
}
