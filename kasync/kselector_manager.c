#include <assert.h>
#include "kselector_manager.h"
#include "kselector.h"
#include "klib.h"
#include "kmalloc.h"
#include "kthread.h"
#include "ksync.h"
#ifdef _WIN32
#include "kiocp_selector.h"
#endif
#ifdef LINUX
#include "kepoll_selector.h"
#endif
#ifdef BSD_OS
#include "kkqueue_selector.h"
#endif
#include <stdio.h>
#define KTHREAD_FLUSH_TIMER 60000
static kselector **kgl_selectors = NULL;
static int kgl_selector_count = 0;
static unsigned kgl_selector_hash = 0;
static unsigned kgl_selector_index = 0;

typedef struct {
	void *ctx;
	selectable_iterator it;
	kcond *cond;
} selectable_iterator_param;

typedef struct kgl_selector_manager_ready_s kgl_selector_manager_ready;
struct kgl_selector_manager_ready_s
{
	void (*call_back)(void *arg);
	void *arg;
	kgl_selector_manager_ready *next;
};
static kgl_selector_manager_ready *on_ready_list = NULL;
extern void(*kgl_second_change_hook)();

static kev_result next_add_timer(void *arg, int got)
{
	kgl_block_queue *brq = (kgl_block_queue *)arg;
	kselector_add_block_queue(kgl_get_tls_selector(),brq);
	return kev_ok;
}
static void add_timer_on_ready(void *arg)
{
	kgl_block_queue *brq = (kgl_block_queue *)arg;
	kselector *selector = get_perfect_selector();
	kgl_selector_module.next(selector,next_add_timer, brq,0);
}
static kev_result selector_iterator(void *arg, int got)
{
	selectable_iterator_param *param = (selectable_iterator_param *)arg;
	kselector *selector = kgl_get_tls_selector();
	struct krb_node *node = selector->block_first;
	for (int i = 0; i < KGL_LIST_BLOCK; i++) {
		kgl_list *l;
		klist_foreach(l, &selector->list[i]) {
			kselectable *st = (kselectable *)kgl_list_data(l, kselectable, queue);
			if (!param->it(param->ctx, selector, st)) {
				goto done;
			}
		}
	}	
	while (node) {
		kgl_block_queue *brq = (kgl_block_queue *)node->data;
		while (brq) {
			if (brq->st) {
				if (!param->it(param->ctx, selector, brq->st)) {
					goto done;
				}
			}
			brq = brq->next;
		}
		node = rb_next(node);
	}
done:
	kcond_notice(param->cond);
	return kev_ok;
}
void selector_manager_iterator(void *ctx, selectable_iterator it)
{
	selectable_iterator_param param;
	param.ctx = ctx;
	param.it = it;
	param.cond = kcond_init(true);
	for (int i = 0; i < kgl_selector_count; i++) {
		kselector *selector = kgl_selectors[i];
		kgl_selector_module.next(selector, selector_iterator, &param, 0);
		kcond_wait(param.cond);
	}
}
void selector_manager_add_timer(result_callback timer, void *arg, int msec, kselectable *st)
{
	kgl_block_queue *brq = xmemory_new(kgl_block_queue);
	brq->active_msec = kgl_current_msec + msec;
	brq->func = timer;
	brq->arg = arg;
	brq->st = st;
	if (is_selector_manager_init()) {
		kgl_selector_module.next(get_perfect_selector(), next_add_timer, brq, 0);
		return;
	}
	selector_manager_on_ready(add_timer_on_ready, brq);
}
void kselector_add_timer_ts(kselector *selector,result_callback timer, void *arg, int msec, kselectable *st)
{
	kgl_block_queue *brq = xmemory_new(kgl_block_queue);
	brq->active_msec = kgl_current_msec + msec;
	brq->func = timer;
	brq->arg = arg;
	brq->st = st;
	kassert(is_selector_manager_init());
	if (kselector_is_same_thread(selector)) {
		kselector_add_block_queue(selector, brq);
		return;
	}
	kgl_selector_module.next(selector, next_add_timer, brq, 0);		
}
int get_selector_count()
{
	return kgl_selector_count;
}
void selector_manager_close()
{
	for (int i = 0; i < kgl_selector_count; i++) {
		kgl_selectors[i]->shutdown = 1;
	}
	kgl_msleep(1000);
	for (int i = 0; i < kgl_selector_count; i++) {
		kgl_selectors[i]->closed = 1;
	}
	for (int i = 0; i < kgl_selector_count; i++) {
		kselector *selector = kgl_selectors[i];
		for (;;) {
			if (selector->thread_id == 0) {
				kselector_destroy(selector);
				break;
			}
			kgl_msleep(500);
		}
	}
	xfree(kgl_selectors);
}
void selector_manager_start(void(*time_hook)())
{
	kgl_second_change_hook = time_hook;
	kgl_program_start_sec = kgl_current_sec;
	int i;
	for (i = 0; i < kgl_selector_count; i++) {
		kselector_start(kgl_selectors[i]);
	}
}
void selector_manager_adjust_time(int64_t diff_time)
{
	int i;
	for (i = 0; i < kgl_selector_count; i++) {
		kselector_adjust_time(kgl_selectors[i], diff_time);
	}
}
static kev_result kthread_flush_timer(void *arg, int got)
{
	kselector *selector = (kselector *)arg;
	kthread_flush(0);
	kselector_add_timer(selector, kthread_flush_timer, selector, KTHREAD_FLUSH_TIMER, NULL);
	return kev_ok;
}
void selector_manager_init(unsigned  size,bool register_thread_timer)
{
	kselector_update_time();
	pthread_key_create(&kgl_selector_key, NULL);
	kgl_selector_module.name = NULL;
#ifdef _WIN32
	kiocp_module_init();
#elif LINUX
	kepoll_module_init();
#elif BSD_OS
	kkqueue_module_init();
#else
#error no selector module init
#endif
	if (kgl_selector_module.name==NULL) {
		fprintf(stderr,"kgl_selector_module init failed.\n");
		abort();
	}
	int i;
	for (i = 0; i < 7; i++) {
		kgl_selector_count = (1 << i);
		if (kgl_selector_count == (int)size) {
			break;
		}
		if (kgl_selector_count > (int)size) {
			kgl_selector_count--;
			break;
		}
	}
	kgl_selector_hash = kgl_selector_count - 1;
	kgl_selectors = (kselector **)xmalloc(sizeof(kselector *)*kgl_selector_count);
	for (i = 0; i < kgl_selector_count; i++) {
		kgl_selectors[i] = kselector_new();
		if (i == 0) {
			kgl_selectors[i]->utm = true;
		}
		kgl_selectors[i]->sid = i;
	}
	selector_manager_set_timeout(10, 30);
	//call onReadyList
	while (on_ready_list) {
		on_ready_list->call_back(on_ready_list->arg);
		kgl_selector_manager_ready *next = on_ready_list->next;
		free(on_ready_list);
		on_ready_list = next;
	}
	if (register_thread_timer) {
		kselector *selector = get_perfect_selector();
		selector_manager_add_timer(kthread_flush_timer, selector, KTHREAD_FLUSH_TIMER, NULL);
	}
}
static void selector_set_time_out(int time_out_index, int msec)
{
	if (msec <= 0) {
		msec = 60000;
	}
	int i;
	for (i = 0; i < kgl_selector_count; i++) {
		kgl_selectors[i]->timeout[time_out_index] = msec;
	}
}
void selector_manager_set_timeout(int connect_tmo_sec, int rw_tmo_sec)
{
	if (connect_tmo_sec <= 0) {
		connect_tmo_sec = rw_tmo_sec;
	}
	selector_set_time_out(KGL_LIST_CONNECT, connect_tmo_sec * 1000);
	selector_set_time_out(KGL_LIST_RW, rw_tmo_sec * 1000);

}
void selector_manager_on_ready(void(*call_back)(void *arg), void *arg)
{
	if (is_selector_manager_init()) {
		call_back(arg);
		return;
	}
	kgl_selector_manager_ready *item = (kgl_selector_manager_ready *)malloc(sizeof(kgl_selector_manager_ready));
	item->call_back = call_back;
	item->arg = arg;
	item->next = on_ready_list;
	on_ready_list = item;
}
bool is_selector_manager_init()
{
	return kgl_selectors != NULL;
}
kselector *get_selector_by_index(int index)
{
	return kgl_selectors[index & kgl_selector_hash];
}
kselector *get_perfect_selector()
{
	unsigned i = kgl_selector_index;
	for (int j = 0; j < kgl_selector_count; j++, i++) {
		kselector *selector = kgl_selectors[i & kgl_selector_hash];
		kselector *next_selector = kgl_selectors[(i + 1) & kgl_selector_hash];
		if (selector->count < next_selector->count + 64) {
			kgl_selector_index = i + 1;
			return selector;
		}
	}
	kassert(false);
	return get_selector_by_index(kgl_selector_index++);
}
bool selector_manager_listen(kserver *server, result_callback callback)
{
	if (server->ss == NULL) {
		return false;
	}
	if (!is_server_multi_selectable(server)) {
		kselector *selector = get_perfect_selector();
		//printf("selector=[%p]\n", selector);
		server->ss->st.selector = selector;
		kserver_refs(server);
		kgl_selector_module.listen(selector,server->ss, callback);
		return true;
	}
	kserver_selectable *ss = server->ss;
	int index = 0;
	while (ss) {
		kselector *selector = get_selector_by_index(index++);
		ss->st.selector = selector;
		kserver_refs(server);
		kgl_selector_module.listen(selector,ss, callback);
		ss = ss->next;
	}
	return true;
}
