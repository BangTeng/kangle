#ifndef KASYNCWORKER_H
#define KASYNCWORKER_H
#include "KMutex.h"
#include "KCountable.h"
#include "ksapi.h"
//异步工作
/**
* 异步回调函数
*/
typedef KTHREAD_FUNCTION (* asyncWorkerCallBack)(void *data,int msec);


struct KAsyncParam {
	void *data;
	INT64 startTime;
	asyncWorkerCallBack callBack;
	KAsyncParam *next;
};
bool thread_start_worker(void *param, asyncWorkerCallBack callback);
//异步工作类
class KAsyncWorker : public KCountableEx
{
public:
	KAsyncWorker()
	{
		maxWorker = 1;
		worker = 0;
		queue = 0;
		head = NULL;
		last = NULL;
	}
	KAsyncWorker(int maxWorker)
	{
		this->maxWorker = maxWorker;
		worker = 0;
		queue = 0;
		if (this->maxWorker<=0) {
			this->maxWorker = 1;
		}
		head = NULL;
		last = NULL;
	}
	void start(void *data,asyncWorkerCallBack callBack,bool high=false);
	void workThread();
	int getBusyCount()
	{
		lock.Lock();
		int count = worker + queue;
		lock.Unlock();
		return count;
	}
	bool isEmpty()
	{
		lock.Lock();
		bool result = (head==NULL);
		lock.Unlock();
		return result;
	}
	int getQueue()
	{
		return queue;
	}
	int getWorker()
	{
		return worker;
	}
	void setWorker(int maxWorker)
	{
		this->maxWorker = maxWorker;
		if (this->maxWorker<=0) {
			this->maxWorker = 1;
		}
	}
	~KAsyncWorker()
	{
		while (head) {
			last = head->next;
			delete head;
			head = last;
		}
	}
private:
	KMutex lock;
	KAsyncParam *head;
	KAsyncParam *last;
	int maxWorker;
	int worker;
	int queue;
};
#endif
