#ifndef KSCGIFETCHOBJECT_H
#define KSCGIFETCHOBJECT_H
#include "KAsyncFetchObject.h"
#include "KEnvInterface.h"

//scgi协议处理，参考:http://python.ca/scgi/protocol.txt
class KScgiFetchObject : public KAsyncFetchObject,public KEnvInterface
{
public:
	KScgiFetchObject()
	{

	}
	~KScgiFetchObject()
	{
	}
	//和http proxy处理一样
	char *nextBody(KHttpRequest *rq,int &len)
	{
		if (parser.bodyLen>0) {
			len = parser.bodyLen;
			parser.bodyLen = 0;
			return parser.body;
		}
		if (hot) {
			len = hot - header;
			hot = NULL;
			return header;
		}
		return NULL;
	}
	void buildHead(KHttpRequest *rq);
	Parse_Result parseHead(KHttpRequest *rq,char *data,int len);
	//和http proxy处理一样
	Parse_Result parseBody(KHttpRequest *rq,char *data,int len)
	{
		hot = data + len;
		return Parse_Continue;
	}
	bool addEnv(const char *attr, const char *val);
	bool addHttpHeader(char *attr, char *val);
private:
	bool content_length_added;
	KHttpProtocolParser parser;
	KHttpObjectParserHook hook;
};
#endif
