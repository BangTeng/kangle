/*
 * KHttpHeadPull.h
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */

#ifndef KHTTPHEADPULL_H_
#define KHTTPHEADPULL_H_
#include "KString.h"
#include "KHttpProtocolParser.h"

class KHttpHeadPull: public KHttpProtocolParser {
public:
	int pull(const char *str, int len, KHttpProtocolParserHook *hook);
	KHttpHeadPull();
	virtual ~KHttpHeadPull();
private:
	KStringBuf *buf;
};

#endif /* KHTTPHEADPULL_H_ */
