#ifndef KTIMER_H
#define KTIMER_H
#include "KSelectable.h"
#include "KSocket.h"
#include "ksapi.h"
void timer_run(timer_func f,void *arg,int msec,unsigned short selector = 0);
bool test_timer();
#endif
