#ifndef KSELECTOR_MANAGER_H_99
#define KSELECTOR_MANAGER_H_99
#include "kfeature.h"
#include "kselector.h"
#include "kserver.h"
KBEGIN_DECLS
typedef bool (*selectable_iterator)(void *ctx, kselector *selector, kselectable *st);
int get_selector_count();
void selector_manager_init(unsigned  size,bool register_thread_timer);
void selector_manager_on_ready(void (*call_back)(void *arg), void *arg);
void selector_manager_add_timer(result_callback timer,void *arg,int msec, kselectable *st);
void kselector_add_timer_ts(kselector *selector,result_callback timer, void *arg, int msec, kselectable *st);
bool is_selector_manager_init();
void selector_manager_start(void(*time_hook)());
void selector_manager_close();
void selector_manager_set_timeout(int connect_tmo_sec,int rw_tmo_sec);
void selector_manager_adjust_time(int64_t diff_time);
void selector_manager_iterator(void *ctx,selectable_iterator it);
kselector *get_perfect_selector();
kselector *get_selector_by_index(int index);
bool selector_manager_listen(kserver *st, result_callback callback);
KEND_DECLS
#endif

