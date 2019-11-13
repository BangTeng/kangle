#ifndef KWORKER_H_99
#define KWORKER_H_99
#include "kfeature.h"
#include "ksync.h"
#include "kcountable.h"
KBEGIN_DECLS
typedef kev_result (*kasync_worker_callback)(void *data, int msec);

typedef struct kasync_worker_param_s kasync_worker_param;

struct kasync_worker_param_s {
	kcond *wait;
	int64_t start_time;
	void *data;
	kasync_worker_callback cb;
	kasync_worker_param *next;
};
typedef struct {
	kmutex lock;
	kasync_worker_param *head;
	kasync_worker_param *last;
	int max_worker;
	int max_queue;
	int worker;
	int queue;
	kcountable_t refs;
} kasync_worker;

INLINE void kasync_worker_set(kasync_worker *worker, int max_worker, int max_queue)
{
	worker->max_worker = max_worker;
	worker->max_queue = max_queue;
}
bool kasync_worker_empty(kasync_worker *worker);
kasync_worker *kasync_worker_init(int max_worker, int max_queue);
bool kasync_worker_thread_start(void *param, kasync_worker_callback cb);
bool kasync_worker_try_start(kasync_worker *worker,void *data, kasync_worker_callback cb, bool high);
void kasync_worker_start(kasync_worker *worker,void *data, kasync_worker_callback cb);
void kasync_worker_release(kasync_worker *worker);
void kasync_worker_refs(kasync_worker *worker);
INLINE bool kasync_worker_will_wait(kasync_worker *worker)
{
	kmutex_lock(&worker->lock);
	bool wait = (worker->worker >= worker->max_worker);
	kmutex_unlock(&worker->lock);
	return wait;
}
KEND_DECLS
#endif
