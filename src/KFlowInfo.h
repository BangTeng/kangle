#ifndef KFLOWINFO_H
#define KFLOWINFO_H
#include "global.h"
#include "KCountable.h"
#define FLOW_UPDATE_TOTAL        1
#define FLOW_UPDATE_CACHE        2
/**
* 流量统计类
*/
class KFlowInfo : public KCountableEx
{
public:
	KFlowInfo()
	{
		flow = 0;
		cache = 0;
		last_flow = 0;
		last_time = kgl_current_msec;
	}
	void addFlow(INT64 flow,int flowFlag)
	{
		refsLock.Lock();
		if (TEST(flowFlag,FLOW_UPDATE_CACHE)) {
			cache += flow;
		}
		if (TEST(flowFlag,FLOW_UPDATE_TOTAL)) {
			this->flow += flow;
		}
		refsLock.Unlock();
	}
	void reset()
	{
		refsLock.Lock();
		last_flow -= flow;
		flow = 0;
		cache = 0;
		refsLock.Unlock();
	}
	int dump(char *buf,int len)
	{
#ifdef _WIN32
		const char *formatString="%I64d\t%I64d";
#else
		const char *formatString = "%lld\t%lld";
#endif
		refsLock.Lock();
		len = snprintf(buf,len,formatString,flow,cache);
		last_flow -= flow;
		flow = 0;
		cache = 0;
		refsLock.Unlock();
		return len;
	}
	int getSpeed(bool reset)
	{
		INT64 current_msec = kgl_current_msec;	
		refsLock.Lock();
		INT64 inc_time = current_msec - last_time;
		if (inc_time<=0) {
			refsLock.Unlock();
			return 0;
		}
		INT64 inc_flow = flow - last_flow;	
		if (reset) {
			last_flow = flow;
			last_time = current_msec;
		}
		refsLock.Unlock();
		return (int)((inc_flow*1000)/inc_time);
	}
	//总流量
	INT64 flow;
	//缓存的流量
	INT64 cache;
	INT64 request;
	INT64 last_flow;
	INT64 last_time;
};
class KFlowInfoHelper
{
public:
	KFlowInfoHelper(KFlowInfo *fi)
	{
		next = NULL;
		this->fi = fi;
		fi->addRef();
	}
	~KFlowInfoHelper()
	{
		if (fi) {
			fi->release();
		}
	}
	KFlowInfoHelper *next;
	KFlowInfo *fi;
};
#endif
