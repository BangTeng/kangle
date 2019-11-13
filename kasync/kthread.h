#ifndef KTHREAD_H_99
#define KTHREAD_H_99
#include "kfeature.h"
#include "kforwin32.h"
KBEGIN_DECLS
void kthread_init();
void kthread_flush(int min_free_thread);
bool kthread_start(KTHREAD_FUNCTION(*work)(void *param), void *param);
bool kthread_pool_start(KTHREAD_FUNCTION(*work)(void *param), void *param);
void kthread_get_count(int *work_count,int *free_count);
void kthread_close_all_free();
KEND_DECLS
#endif
