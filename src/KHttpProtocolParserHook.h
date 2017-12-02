#ifndef KHTTPPROTOCOLPARSERHOOK_H_
#define KHTTPPROTOCOLPARSERHOOK_H_
#include "global.h"
/*
 * HTTP协议解析钩子
 */
#define 	PARSE_HEADER_FAILED 	  0
#define 	PARSE_HEADER_SUCCESS 	  1
#define 	PARSE_HEADER_NO_INSERT	  2
#define     PARSE_HEADER_INSERT_BEGIN 3
class KHttpProtocolParser;
class KHttpProtocolParserHook
{
public:
	KHttpProtocolParserHook();
	virtual ~KHttpProtocolParserHook();
	/*
	 * 检查header
	 * 返回: 0=failed,1=add headers,2=do not insert headers
	 */
	virtual int parseHeader(const char *attr,char *val,int &val_len,bool isFirst)=0;
	virtual void startParse()
	{
	}
	virtual void endParse()
	{
	}
	void setProto(Proto_t proto) {
		this->proto = proto;
	}
	Proto_t proto;
};

#endif /*KHTTPPROTOCOLPARSERHOOK_H_*/
