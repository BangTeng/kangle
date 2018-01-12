#ifndef KQUEUEMARK_H
#define KQUEUEMARK_H
#include "KMark.h"
#include "KReg.h"
#include "KHttpRequest.h"
#include "utils.h"
#ifdef ENABLE_REQUEST_QUEUE
class KRequestQueue;
class KQueueMark : public KMark
{
public:
	KQueueMark()
	{
		queue = NULL;
	}
	~KQueueMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType);
	bool supportRuntime()
	{
		return true;
	}
	KMark *newInstance()
	{
		return new KQueueMark;
	}
	const char *getName()
	{
		return "queue";
	}
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException);
	void buildXML(std::stringstream &s);
private:
	KRequestQueue *queue;
};
class KPerQueueMark;
struct per_queue_arg
{
	char *key;
	KPerQueueMark *mark;
};
void WINAPI per_queue_mark_call_back(void *data);
class KPerQueueMark : public KMark
{
public:
	KPerQueueMark()
	{
		max_worker = 0;
		max_queue = 0;
	}
	~KPerQueueMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType);
	bool supportRuntime()
	{
		return true;
	}
	KMark *newInstance()
	{
		return new KPerQueueMark;
	}
	const char *getName()
	{
		return "per_queue";
	}
	void callBack(char *key);
	std::string getHtml(KModel *model);
	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException);
	void buildXML(std::stringstream &s);
private:
	void addCallBack(KHttpRequest *rq, const char *key)
	{
		per_queue_arg *cd = new per_queue_arg;
		cd->key = strdup(key);
		cd->mark = this;
		addRef();
		rq->registerRequestCleanHook(per_queue_mark_call_back, cd);
	}
	KMutex lock;
	KReg reg_url;
	std::map<char *, KRequestQueue *, lessp> queues;
	unsigned max_worker;
	unsigned max_queue;
};
#endif
#endif

