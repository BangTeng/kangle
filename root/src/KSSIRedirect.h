/*
 * KSSIRedirect.h
 *
 *  Created on: 2010-8-2
 *      Author: keengo
 */

#ifndef KSSIREDIRECT_H_
#define KSSIREDIRECT_H_

#include "KRedirect.h"

class KSSIRedirect: public KRedirect {
public:
	KSSIRedirect();
	virtual ~KSSIRedirect();
	KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file);
	const char *getType() {
		return "ssi";
	}
};
extern KSSIRedirect ssi;
#endif /* KSSIREDIRECT_H_ */
