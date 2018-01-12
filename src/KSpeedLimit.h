#ifndef KSPEEDLIMIT_H
#define KSPEEDLIMIT_H
#include "KCountable.h"
#include "time_utils.h"
class KSpeedLimit : public KCountableEx
{
public:
	KSpeedLimit()
	{
		current_send_time = 0;
	}
	void setSpeedLimit(int speed_limit)
	{
		this->speed_limit = speed_limit;
	}
	int getSpeedLimit()
	{
		return speed_limit;
	}
	int getSleepTime(int len)
	{
		refsLock.Lock();
		if (speed_limit <= 0) {
			refsLock.Unlock();
			return 0;
		}
		INT64 sleep_time = (INT64)(len * 1000 / speed_limit);
		if (current_send_time < kgl_current_msec - 5000) {
			current_send_time = kgl_current_msec;
		}
		current_send_time += sleep_time;
		//printf("len=[%d],sleep_time=[%lld],adjust_sleep_time=[%lld]\n", len, sleep_time, (current_send_time - kgl_current_msec));
		sleep_time = current_send_time - kgl_current_msec;
		refsLock.Unlock();
		if (sleep_time <= 0) {
			return 0;
		}
		if (sleep_time > 600000) {
			return 600000;
		}
		return (int)sleep_time;
	}
private:
	INT64 current_send_time;
	int speed_limit;
};
class KSpeedLimitHelper
{
public:
	KSpeedLimitHelper(KSpeedLimit *sl)
	{
		next = NULL;
		this->sl = sl;	
	}
	~KSpeedLimitHelper()
	{
		if (sl) {
			sl->release();
		}
	}
	KSpeedLimitHelper *next;
	KSpeedLimit *sl;
};
#endif
