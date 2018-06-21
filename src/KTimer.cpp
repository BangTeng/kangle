#include "KTimer.h"
#include "KSelectorManager.h"
struct timer_run_param
{
	KSelector *selector;
	timer_func func;
	void *arg;
};
void result_timer_call_back(void *arg, int got)
{
	timer_run_param *param = (timer_run_param *)arg;
	param->func(param->arg);
	delete param;
}
void next_timer_call_back(void *arg, int got)
{
	timer_run_param *param = (timer_run_param *)arg;
	param->selector->add_timer(result_timer_call_back, param,got,NULL);
}
void timer_run(timer_func func,void *arg,int msec,unsigned short selector)
{
	timer_run_param *param = new timer_run_param;
	param->selector = (selector == 0 ? selectorManager.getSelector() : selectorManager.getSelectorByIndex(selector));
	param->func = func;
	param->arg = arg;
	param->selector->next(next_timer_call_back, param,msec);
}
bool test_timer()
{
	return true;
}
