#ifndef KREQUESTQUEUE_H
#define KREQUESTQUEUE_H
#include "KCountable.h"
#include "KHttpRequest.h"
#include "KThreadPool.h"
#ifdef ENABLE_REQUEST_QUEUE
class KRequestQueue : public KCountableEx
{
public:
	KRequestQueue();
	~KRequestQueue();
	bool start(KHttpRequest *context);
	void set(unsigned max_worker,unsigned max_queue);
	KHttpRequest *getQueue();
	void decWorker()
	{
		refsLock.Lock();
		cur_worker --;
		refsLock.Unlock();
	}
	unsigned getMaxWorker()
	{
		return max_worker;
	}
	unsigned getMaxQueue()
	{
		return max_queue;
	}
	unsigned getWorkerCount()
	{
		return cur_worker;
	}
	unsigned getQueueSize()
	{
		refsLock.Lock();
		unsigned queueSize = queue.size();
		refsLock.Unlock();
		return queueSize;
	}
	unsigned getBusyCount()
	{
		refsLock.Lock();
		unsigned count = cur_worker + queue.size();
		refsLock.Unlock();
		return count;
	}
private:
	bool startDirect(KHttpRequest *context);
	std::list<KHttpRequest *> queue;
	unsigned max_worker;
	unsigned max_queue;
	unsigned cur_worker;
};
void async_queue_destroy(KRequestQueue *queue);
extern KRequestQueue globalRequestQueue;
#endif
#endif

