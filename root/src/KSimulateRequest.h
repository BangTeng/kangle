#ifndef KSIMULATEREQUEST_H
#define KSIMULATEREQUEST_H
#include "global.h"
#include "KSocket.h"
#include "KHttpHeader.h"
#include "ksapi.h"
#ifdef ENABLE_SIMULATE_HTTP
int asyncHttpRequest(kgl_async_http *ctx);
class KSimulateSocket : public KClientSocket
{
public:
	KSimulateSocket();
	~KSimulateSocket();
	int sendHeader(int code,KHttpHeader *header);
	int sendError(int code,const char *msg);
	//·µ»Ø0³É¹¦
	int sendBody(const char *buf,int len);
	http_post_hook post;
	http_header_hook header;
	http_body_hook body;
	char *host;
	unsigned short port;
	int life_time;
	void *arg;
	KHttpHeader *rh;
	INT64 startTime;
};
bool test_simulate_request();
#endif
#endif
