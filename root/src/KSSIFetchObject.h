/*
 * KSSIFetchObject.h
 *
 *  Created on: 2010-8-3
 *      Author: keengo
 */

#ifndef KSSIFETCHOBJECT_H_
#define KSSIFETCHOBJECT_H_

#include "KFetchObject.h"
#include "KSSIProcess.h"
#include "KSSIContext.h"
class KSSIFetchObject: public KFetchObject {
public:
	KSSIFetchObject();
	virtual ~KSSIFetchObject();
	void open(KHttpRequest *rq);
	void readBody(KHttpRequest *rq);
#ifdef ENABLE_REQUEST_QUEUE
	bool needQueue()
	{
		return true;
	}
#endif
private:
	KSSIProcess ssiProcess;
};

#endif /* KSSIFETCHOBJECT_H_ */
