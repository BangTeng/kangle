#ifndef KCONCATFETCHOBJECT_H
#define KCONCATFETCHOBJECT_H
#include "KFetchObject.h"
struct KConcatPath {
	char *path;
	KConcatPath *next;
};
class KConcatFetchObject : public KFetchObject
{
public:
	KConcatFetchObject()
	{
		head = NULL;
		hot = NULL;
	}
	~KConcatFetchObject()
	{
		while (head) {
			hot = head->next;
			free(head->path);
			delete head;
			head = hot;
		}
	}
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue()
	{
		return true;
	}
#endif
	void open(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);
private:
	void startRequest(KHttpRequest *rq,KConcatPath *cp);
	void init(KHttpRequest *rq);
	void add(KConcatPath *item)
	{
		item->next = NULL;
		if (head==NULL) {
			head = item;
		} else {
			hot->next = item;
		}
		hot = item;
	}
	KConcatPath *head;
	KConcatPath *hot;
	
};
#endif
