#include "KTimer.h"
#include "KSelectorManager.h"
void timer_run(timer_func func,void *arg,int msec,unsigned short selector)
{
	KSelector *s = (selector == 0 ? selectorManager.getSelector() : selectorManager.getSelectorByIndex(selector));
	s->addTimer(NULL,func,arg,msec);
}
void WINAPI timer_test_func(void *arg)
{
	timer_run(timer_test_func,arg,500);
}
bool test_timer()
{
	return true;
}
