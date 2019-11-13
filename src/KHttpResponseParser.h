#ifndef KHTTPOBJECTPARSER_H_
#define KHTTPOBJECTPARSER_H_
#include "KHttpObject.h"
class KHttpResponseParser : public KHttpHeaderManager {
public:
	KHttpResponseParser()
	{
		memset(this, 0, sizeof(*this));
	}
	~KHttpResponseParser()
	{
		if (header) {
			free_header(header);
		}
	}
	bool ParseHeader(KHttpRequest *rq, const char *attr, int attr_len, const char *val, int val_len, bool request_line);
	void EndParse(KHttpRequest *rq);
	uint8_t http_major;
	uint8_t http_minor;
	int keep_alive_time_out;
	Proto_t proto;
	time_t first_body_time;
private:
	kgl_header_result InternalParseHeader(KHttpRequest *rq,KHttpObject *obj,const char *attr, int attr_len, const char *val, int *val_len, bool request_line);
	time_t serverDate;
	time_t expireDate;	
	unsigned age;
};
#endif /*KHTTPOBJECTPARSER_H_*/
