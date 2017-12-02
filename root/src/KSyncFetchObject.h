#ifndef KSYNCFETCHOBJECT_H
#define KSYNCFETCHOBJECT_H
#include "KFetchObject.h"
/**
* ͬ��������չ������Ҫ��ͬ�����õ���չ���Ӹ���̳�
*/
class KSyncFetchObject : public KFetchObject
{
public:
	void open(KHttpRequest *rq);
	bool isSync()
	{
		return true;
	}
#ifdef ENABLE_REQUEST_QUEUE
	virtual bool needQueue()
	{
		return true;
	}
#endif
	virtual void process(KHttpRequest *rq) = 0;	
	/*
	 * return value:
	 * READ_SWITCH_FUNCTION		switch to buff *read() function
	 * READ_PROTOCOL_ERROR		protocol error,must close connection
	 * 0 or -1	read end
	 * >0		success read len
	 * @��������ʹ��process
	 */
	virtual int read(char *buf, int len) {
		return -1;
	}
	//deal with the post data
	virtual bool write(const char *buf, int len) {
		return false;
	}
	virtual bool writeComplete() {
		return true;
	}
};
#endif
