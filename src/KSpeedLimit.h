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
		speed_limit = 1048576;
	}
	void setSpeedLimit(int speed_limit)
	{
		if(speed_limit<=0){
			speed_limit = 1048576;
		}
		this->speed_limit = speed_limit;
	}
	int getSpeedLimit()
	{
		return speed_limit;
	}
	int getSleepTime(int len)
	{
		
		refsLock.Lock();
		int sleep_time = len * 1000 / speed_limit;
		if (current_send_time<kgl_current_msec) {
			current_send_time = kgl_current_msec;
		}
		current_send_time += sleep_time;
		//printf("len=[%d],sleep_time=[%d],adjust_sleep_time=[%d]\n", len, sleep_time, (int)(current_send_time - kgl_current_msec));
		sleep_time = (int)(current_send_time - kgl_current_msec);
		refsLock.Unlock();
		return sleep_time;
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
		sl->addRef();
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
