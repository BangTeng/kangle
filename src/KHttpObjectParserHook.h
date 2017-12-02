#ifndef KHTTPOBJECTPARSER_H_
#define KHTTPOBJECTPARSER_H_
#include "KHttpObject.h"
#include "KHttpProtocolParserHook.h"
class KHttpObjectParserHook: public KHttpProtocolParserHook {
public:
	KHttpObjectParserHook(KHttpObject *obj, KHttpRequest *rq);
	KHttpObjectParserHook();
	void init(KHttpObject *obj, KHttpRequest *rq);
	virtual ~KHttpObjectParserHook();
	int parseHeader(const char *attr, char *val, int &val_len,bool isFirst);

	void startParse();
	void endParse();
	int httpv_major;
	int httpv_minor;
	int keep_alive_time_out;
	void checkHeaders(KHttpHeader *headers);
	KHttpRequest *rq;
	KHttpObject *obj;
private:
	time_t serverDate;
	time_t expireDate;
	time_t responseTime;
	unsigned age;
};

#endif /*KHTTPOBJECTPARSER_H_*/
