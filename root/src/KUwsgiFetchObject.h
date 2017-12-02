#ifndef KUWSGIFETCHOBJECT_H
#define KUWSGIFETCHOBJECT_H
#include "KAsyncFetchObject.h"
#include "KEnvInterface.h"
#include "KHttpObjectParserHook.h"
/*
* uwsgi协议
* 参考 http://projects.unbit.it/uwsgi/wiki/uwsgiProtocol
*/
#pragma pack(push,1)
struct uwsgi_packet_header {
    unsigned char modifier1;
    u_short datasize;
    unsigned char modifier2;
};
#pragma pack(pop)
class KUwsgiFetchObject : public KAsyncFetchObject,public KEnvInterface
{
public:
	KUwsgiFetchObject() 
	{

	}
	~KUwsgiFetchObject()
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
	KHttpProtocolParser parser;
	KHttpObjectParserHook hook;
};
#endif
