#include <time.h>
#include <assert.h>
#include <string.h>
#ifndef _WIN32
#include <sys/time.h>
#endif
#include "kselector.h"
#include "kselectable.h"
#include "kthread.h"
#include "kselector_manager.h"
#include "klog.h"
#include "kmalloc.h"

pthread_key_t kgl_selector_key;
kselector_module kgl_selector_module;
volatile int64_t kgl_current_msec = 0;
volatile time_t kgl_current_sec = 0;
volatile uint32_t kgl_aio_count = 0;
time_t kgl_program_start_sec = 0;
void (*kgl_second_change_hook)() = NULL;


#ifdef WIN32
static inline int gettimeofday(struct timeval *tp, void *tzp)
{
	time_t clock;
	struct tm tm;
	SYSTEMTIME wtm;
	GetLocalTime(&wtm);
	tm.tm_year = wtm.wYear - 1900;
	tm.tm_mon = wtm.wMonth - 1;
	tm.tm_mday = wtm.wDay;
	tm.tm_hour = wtm.wHour;
	tm.tm_min = wtm.wMinute;
	tm.tm_sec = wtm.wSecond;
	tm.tm_isdst = -1;
	clock = mktime(&tm);
	tp->tv_sec = (long)clock;
	tp->tv_usec = wtm.wMilliseconds * 1000;
	return (0);
}
#endif

static kev_result next_adjust_time(void *arg, int got)
{
	int64_t *diff_time = (int64_t *)arg;
	kselector_adjust_time(kgl_get_tls_selector(), *diff_time);
	xfree(diff_time);
	return kev_ok;
}
static INLINE struct krb_node *kgl_insert_block_queue(struct krb_root *root, kgl_block_queue *brq, bool *is_first)
{
	struct krb_node **n = &(root->rb_node), *parent = NULL;
	kgl_block_queue *tmp = NULL;
	while (*n) {
		tmp = (kgl_block_queue *)((*n)->data);
		int64_t result = brq->active_msec - tmp->active_msec;
		parent = *n;
		if (result < 0) {
			n = &((*n)->rb_left);
		} else if (result > 0) {
			n = &((*n)->rb_right);
			*is_first = false;
		} else {
			*is_first = false;
			brq->next = tmp;
			(*n)->data = brq;
			return *n;
		}
	}
	struct krb_node *node = (struct krb_node *)xmalloc(sizeof(struct krb_node));
	node->data = brq;
	brq->next = NULL;
	rb_link_node(node, parent, n);
	rb_insert_color(node, root);
	return node;
}
KTHREAD_FUNCTION kselector_thread(void *param) 
{
	srand((unsigned)(time(NULL) * (int64_t)pthread_self()));
	kselector *selector = (kselector*)param;
	pthread_setspecific(kgl_selector_key, selector);
	selector->thread_id = pthread_self();
	kgl_selector_module.select(selector);	
	klog(KLOG_ERR, "selector thread = [%d] now close.\n", selector->thread_id);
	selector->thread_id = 0;
	KTHREAD_RETURN;
}
bool kselector_is_same_thread(kselector *selector)
{
	return pthread_self() == selector->thread_id;
}
void kselector_destroy(kselector *selector)
{
	kgl_selector_module.destroy(selector);
	xfree(selector);
}
kselector *kselector_new()
{
	kselector *selector = (kselector *)xmalloc(sizeof(kselector));
	memset(selector, 0, sizeof(kselector));
	for (int i = 0; i < KGL_LIST_BLOCK; i++) {
		klist_init(&selector->list[i]);
	}
	kgl_selector_module.init(selector);
	return selector;
}
bool kselector_start(kselector *selector)
{
	return kthread_pool_start(kselector_thread, selector);
}
void kselector_add_list(kselector *selector, kselectable *st, int list)
{
	kassert(kselector_is_same_thread(selector));
	st->tmo_left = st->tmo;
	kassert(st->selector == selector);
	st->active_msec = kgl_current_msec;
	kassert(list >= 0 && list < KGL_LIST_NONE);
	if (st->queue.next) {
		klist_remove(&st->queue);
	} else {
		selector->count++;
	}
	klist_append(&selector->list[list], &st->queue);
}
void kselector_remove_list(kselector *selector, kselectable *st)
{
	kassert(kselector_is_same_thread(selector));
	kassert(st->selector == selector);
	if (st->queue.next == NULL) {
		return;
	}
	klist_remove(&st->queue);
	memset(&st->queue, 0, sizeof(st->queue));
	kassert(selector->count > 0);
	selector->count--;
}
void kselector_adjust_time(kselector *selector, int64_t diff_time)
{
	if (!kselector_is_same_thread(selector)) {
		int64_t *param = xmemory_new(int64_t);
		*param = diff_time;
		kgl_selector_module.next(selector, next_adjust_time, param,0);
		return;
	}
	struct krb_node *node = selector->block_first;
	while (node) {
		kgl_block_queue *brq = (kgl_block_queue *)node->data;
		kassert(brq);
		brq->active_msec += diff_time;
		node = rb_next(node);
	}
}
void kselector_update_time()
{	
	struct timeval   tv;
	gettimeofday(&tv, NULL);
	if (unlikely(kgl_current_sec != tv.tv_sec)) {
		if (tv.tv_sec < kgl_current_sec) {
			//printf("发生时间倒退\n");
			int64_t diff_msec = (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000) - kgl_current_msec;
			selector_manager_adjust_time(diff_msec);
		}		
		kgl_current_sec = tv.tv_sec;
		if (kgl_second_change_hook) {
			kgl_second_change_hook();
		}
	}
	kgl_current_msec = (int64_t)tv.tv_sec * 1000 + (tv.tv_usec / 1000);
	return;
}
void kselector_check_timeout(kselector *selector,int event_number)
{
	for (;;) {
		kgl_list *l = klist_head(&selector->list[KGL_LIST_READY]);
		if (l == &selector->list[KGL_LIST_READY]) {
			break;
		}
		++event_number;
		kselectable *st = kgl_list_data(l, kselectable, queue);
		kassert(st->selector == selector);
		klist_remove(l);
		memset(l, 0, sizeof(kgl_list));
		selector->count--;
		uint16_t st_flags = st->st_flags;
		if (TEST(st_flags, STF_WREADY|STF_WREADY2) && TEST(st_flags, STF_WRITE | STF_RDHUP)) {
			selectable_write_event(st);
			CLR(st_flags, STF_WRITE | STF_RDHUP);
		}
		if (TEST(st_flags, STF_RREADY|STF_RREADY2)) {
			if (TEST(st_flags, STF_READ)) {
				selectable_read_event(st);
				CLR(st_flags, STF_READ);
			} else if (TEST(st_flags, STF_RECVFROM)) {
				selectable_recvfrom_event(st);
				CLR(st_flags, STF_RECVFROM);
			}
		}
		if (TEST(st_flags, STF_READ | STF_WRITE | STF_RECVFROM) &&
#ifdef STF_ET
			TEST(st_flags, STF_ET) &&
#endif
			st->queue.next == NULL) {
			kselector_add_list(selector, st, KGL_LIST_RW);
		}
	}
	for (int i = 0; i < KGL_LIST_SYNC; i++) {
		for (;;) {
			kgl_list *l = klist_head(&selector->list[i]);
			if (l == &selector->list[i]) {
				break;
			}
			kselectable *rq = kgl_list_data(l, kselectable, queue);
			kassert(rq->selector == selector);
#ifdef MALLOCDEBUG
			if (selector->shutdown) {
				selectable_shutdown(rq);
			}
#endif
			if ((kgl_current_msec - rq->active_msec) < (time_t)selector->timeout[i]) {
				break;
			}
			klist_remove(l);
			memset(l, 0, sizeof(kgl_list));
			if (rq->tmo_left > 0) {
				rq->tmo_left--;
				rq->active_msec = kgl_current_msec;
				klist_append(&selector->list[i], l);
				continue;
			}
			memset(l, 0, sizeof(kgl_list));
#ifndef NDEBUG
			klog(KLOG_DEBUG, "request timeout st=%p\n", (kselectable *)rq);
#endif
			kassert(selector->count > 0);
			if (TEST(rq->st_flags, STF_RTIME_OUT | STF_READ) == (STF_RTIME_OUT | STF_READ)) {
				//set read time out
				klist_append(&selector->list[i], l);
				rq->active_msec = kgl_current_msec;
				kassert(rq->e[OP_READ].result);
				rq->e[OP_READ].result(rq->e[OP_READ].arg, ST_ERR_TIME_OUT);
				continue;
			}
			selector->count--;
			selectable_shutdown(rq);
#ifdef _WIN32
			ksocket_cancel(rq->fd);
#endif
		}
	}
	struct krb_node *block = NULL;
	struct krb_node *last = NULL;

	while (selector->block_first) {
		kgl_block_queue *rq = (kgl_block_queue *)selector->block_first->data;
		kassert(rq);
#ifdef MALLOCDEBUG
		if (selector->shutdown) {
			rq->active_msec = kgl_current_msec - 1;
		}
#endif
		if (kgl_current_msec < rq->active_msec) {
			break;
		}
		struct krb_node *next = rb_next(selector->block_first);
		rb_erase(selector->block_first, &selector->block);
		if (last != NULL) {
			last->rb_right = selector->block_first;
		} else {
			block = selector->block_first;
		}
		last = selector->block_first;
		last->rb_right = NULL;
		selector->block_first = next;
	}

	while (block) {
		kgl_block_queue *rq = (kgl_block_queue *)block->data;
		while (rq) {
			kgl_block_queue *rq_next = rq->next;
			rq->func(rq->arg, rq->got);
			xfree(rq);
			rq = rq_next;
		}
		last = block->rb_right;
		xfree(block);
		block = last;
	}
	if (selector->check_timeout) {
		selector->check_timeout(selector, event_number);
	}
}
void kselector_add_block_queue(kselector *selector, kgl_block_queue *brq)
{
	kassert(kselector_is_same_thread(selector));
	bool is_first = true;
	struct krb_node
		*node = kgl_insert_block_queue(&selector->block, brq, &is_first);
	if (is_first) {
		selector->block_first = node;
	}
	kassert(selector->block_first == rb_first(&selector->block));
}
void kselector_add_timer(kselector *selector, result_callback result, void *arg, int msec, kselectable *st)
{
	kgl_block_queue *brq = (kgl_block_queue *)xmalloc(sizeof(kgl_block_queue));
	memset(brq, 0, sizeof(kgl_block_queue));
	brq->active_msec = kgl_current_msec + msec;
	brq->func = result;
	brq->arg = arg;
	brq->st = st;
	kselector_add_block_queue(selector, brq);
}
void kselector_default_bind(kselector *selector, kselectable *st)
{
	st->selector = selector;
}
bool kselector_default_readhup(kselector *selector, kselectable *st, result_callback result,  void *arg)
{
        return false;
}
bool kselector_default_remove_readhup(kselector *selector, kselectable *st)
{
        return false;
}

