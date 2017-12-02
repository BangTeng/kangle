#ifndef KHMUXFETCHOBJECT_H
#define KHMUXFETCHOBJECT_H
#include "KAsyncFetchObject.h"
/*-----------------------------------------------------------------------------
 *  
 *
 * 							hmux protocol
 *  A GET request:
 * 		Frontend       			Backend
 * 		CSE_METHOD
 * 		...
 * 		CSE_HEADER/CSE_VALUE
 * 		CSE_END
 * 								CSE_DATA
 * 								CSE_DATA
 * 								CSE_END
 *
 *  Short POST:
 * 		Frontend       			Backend
 * 		CSE_METHOD
 * 		...
 * 		CSE_HEADER/CSE_VALUE
 * 		CSE_DATA
 * 		CSE_END
 * 								CSE_DATA
 * 								CSE_DATA
 * 								CSE_END
 *  Long POST:
 * 		Frontend       			Backend
 * 		CSE_METHOD
 * 		...
 * 		CSE_HEADER/CSE_VALUE
 * 		CSE_DATA
 * 								CSE_DATA (optional)   #here we buffer response data
 * 		CSE_DATA
 * 								CSE_ACK
 * 								CSE_DATA (optional)   #here we buffer response data
 * 		CSE_DATA
 * 								CSE_ACK
 * 		CSE_END
 * 								CSE_DATA
 * 								CSE_END
 *
 *
 *-----------------------------------------------------------------------------*/
#define HMUX_CHANNEL        'C'
#define HMUX_ACK            'A'
#define HMUX_ERROR          'E'
#define HMUX_YIELD          'Y'
#define HMUX_QUIT           'Q'
#define HMUX_EXIT           'X'

#define HMUX_DATA           'D'
#define HMUX_URL            'U'
#define HMUX_STRING         'S'
#define HMUX_HEADER         'H'
#define HMUX_META_HEADER    'M'
#define HMUX_PROTOCOL       'P'

#define CSE_NULL            '?'
#define CSE_PATH_INFO       'b'
#define CSE_PROTOCOL        'c'
#define CSE_REMOTE_USER     'd'
#define CSE_QUERY_STRING    'e'
#define CSE_SERVER_PORT     'g'
#define CSE_REMOTE_HOST     'h'
#define CSE_REMOTE_ADDR     'i'
#define CSE_REMOTE_PORT     'j'
#define CSE_REAL_PATH       'k'
#define CSE_AUTH_TYPE       'n'
#define CSE_URI             'o'
#define CSE_CONTENT_LENGTH  'p'
#define CSE_CONTENT_TYPE    'q'
#define CSE_IS_SECURE       'r'
#define CSE_SESSION_GROUP   's'
#define CSE_CLIENT_CERT     't'
#define CSE_SERVER_TYPE	    'u'

#define HMUX_METHOD         'm'
#define HMUX_FLUSH          'f'
#define HMUX_SERVER_NAME    'v'
#define HMUX_STATUS         's'
#define HMUX_CLUSTER        'c'
#define HMUX_SRUN           's'
#define HMUX_SRUN_BACKUP    'b'
#define HMUX_SRUN_SSL       'e'
#define HMUX_UNAVAILABLE    'u'
#define HMUX_WEB_APP_UNAVAILABLE 'U'

#define CSE_HEADER          'H'
#define CSE_VALUE           'V'

#define CSE_STATUS          'S'
#define CSE_SEND_HEADER     'G'

/* #define CSE_PING            'P' */
#define CSE_QUERY           'Q'

#define CSE_ACK             'A'
#define CSE_DATA            'D'
#define CSE_FLUSH           'F'
#define CSE_KEEPALIVE       'K'
#define CSE_END             'Z'
#define CSE_CLOSE           'X'
class KHmuxFetchObject : public KAsyncFetchObject
{
public:
	KHmuxFetchObject() 
	{
		body_len = -1;
		header_len = 0;
		last_http_header = NULL;
		parsed_len = 0;
		bodyend = false;
	};
	~KHmuxFetchObject()
	{
		if(last_http_header){
			free(last_http_header);
		}
	}
	//和http proxy处理一样
	char *nextBody(KHttpRequest *rq,int &len)
	{
		char *str = header + parsed_len;
		int before_parsed_len = hot - header - parsed_len;
		len = before_parsed_len;
		if (len==0) {
			return NULL;
		}
		int get_len;
		char *body = parse(&str,len,get_len);
		int this_parsed_len = len - before_parsed_len;
		parsed_len += this_parsed_len;
		len = get_len;
		return body;
	}
	void buildHead(KHttpRequest *rq);
	bool checkContinueReadBody(KHttpRequest *rq)
	{
		return !bodyend;
	}
	Parse_Result parseHead(KHttpRequest *rq,char *data,int len);
	//和http proxy处理一样
	Parse_Result parseBody(KHttpRequest *rq,char *data,int len)
	{
		parsed_len = 0;
		hot = data + len;
		return Parse_Continue;
	}
	bool addEnv(const char code, const char *val);
	bool addEnv(const char code, const char *val,int length);
	bool addEnv(const char code, int val);
	bool addHttpHeader(char *attr, char *val);
	void buildPost(KHttpRequest *rq);
private:
	char *parse(char **str,int &len,int &get_len);
	int code;
	int body_len;
	char hmux_header[3];
	int header_len;
	char *last_http_header;
	int parsed_len;
	bool bodyend;
};
#endif
