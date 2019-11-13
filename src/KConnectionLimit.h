#ifndef KCONNECTION_LIMIT_H
#define KCONNECTION_LIMIT_H
#include "KCountable.h"
class KHttpRequest;
/**
* ������������
*/
class KConnectionLimit : public KAtomCountable
{
public:
	KConnectionLimit()
	{
		cur_connect = 0;
	}
	bool addConnection(KHttpRequest *rq, int max_connect);
	void releaseConnection()
	{
		katom_dec((void *)&cur_connect);
		release();
	}
	int getConnectionCount()
	{
		return (int)katom_get((void *)&cur_connect);
	}
private:
	volatile int32_t cur_connect;
};
#endif
