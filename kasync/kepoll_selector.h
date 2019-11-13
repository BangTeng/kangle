#ifndef KEPOLL_SELECTOR_H_99
#define KEPOLL_SELECTOR_H_99
#include "kselectable.h"
#include "ksync.h"

void kepoll_module_init();

typedef struct kepoll_notice_selectable_s kepoll_notice_selectable;
struct kepoll_notice_selectable_s {
        kselectable st;
        kmutex lock;
	kselector_notice *head;
};
#endif
