#ifndef KQUEUEMARK_H
#define KQUEUEMARK_H
#include "KMark.h"
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
#endif
#endif

