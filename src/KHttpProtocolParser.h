/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KHTTPPROTOCOLPARSER_H_
#define KHTTPPROTOCOLPARSER_H_
#include "forwin32.h"
#include <string.h>
#include <stdlib.h>
#include "global.h"
#define HTTP_PARSE_FAILED	0
#define HTTP_PARSE_SUCCESS	1
#define HTTP_PARSE_CONTINUE	2
#define HTTP_PARSE_NO_NEXT_BUT_CONTINUE		3
#define MAX_HTTP_HEAD_SIZE	4194304
/*
 * HTTP protocol parser
 */
#include "KHttpProtocolParserHook.h"
#include "KHttpHeader.h"
#include "malloc_debug.h"

class KHttpProtocolParser {
public:
	inline KHttpProtocolParser() {
		headers = NULL;
		start();
	}
	inline void start() {
		checked = 0;
		orig_body = NULL;
		body = NULL;
		bodyLen = 0;
		last = NULL;
		started = false;
	}
	inline void restart() {
		destroy();
		start();
	}
	inline void destroy() {
		while (headers) {
			last = headers->next;
			free(headers->attr);
			free(headers->val);
			free(headers);
			headers=last;
		}
		headers = NULL;
	}
	/*
	 * parse the http protocol
	 * return 0=failed,1=success,2=continue
	 */
	int parse(char *buf, int len, KHttpProtocolParserHook *hook);
	virtual ~KHttpProtocolParser();
	/*
	只用来解析不带http的request line协议，设置started=true
	*/
	inline void setStarted(bool started){
		this->started = started;
	}
	KHttpHeader *getHeaders() {
		return headers;
	}
	KHttpHeader *removeHeader(const char *attr)
	{
		KHttpHeader *l = headers;
		KHttpHeader *prev = NULL;
		while (l) {
			if (strcasecmp(l->attr,attr)==0) {
				if (prev) {
					prev->next = l->next;
				} else {
					headers = l->next;
				}
				return l;
			}
			prev = l;
			l = l->next;
		}
		return NULL;
	}
	inline void setHeaders(KHttpHeader *headers)
	{
		if(this->headers){
			free_header(this->headers);
		}
		this->headers = headers;
	}
	void setBody(char *body,int bodyLen)
	{
		this->orig_body = body;
		this->body = body;
		this->bodyLen = bodyLen;
	}
	KHttpHeader *stealHeaders(KHttpHeader *header){
		if (last) {
			last->next = header;
			KHttpHeader *tmp = headers;
			headers = NULL;
			last = NULL;
			return tmp;
		}
		return header;
	}
	KHttpHeader *findHttpHeader(const char *attr,int len) {
		KHttpHeader *next = headers;
		while (next) {
			if (is_attr(next,attr,len)) {
				return next;
			}
			next = next->next;
		}
		return NULL;
	}
	/*
		get the http head value
	*/
	const char *getHttpValue(const char *header)
	{
		KHttpHeader *next = headers;
		while(next){
			if(!strcasecmp(header,next->attr)){
				return next->val;
			}
			next = next->next;
		}
		return NULL;
	}
	char *orig_body;
	/*
	 * http protocol body
	 */
	char *body;
	/*
	 * body len
	 */
	union {
		int bodyLen;
		int headerCount;
	};
	bool parseHeader(kgl_str_t *name, kgl_str_t *value, KHttpProtocolParserHook *hook);
	bool insertHeader(const char *attr,int attr_len,const char *val,int val_len,bool tail=true);
	KHttpHeader *headers;
private:
	bool insertHeader(KHttpHeader *new_t,bool tail=true);
	bool parseHeader(char *header,char *end, bool isFirst, KHttpProtocolParserHook *hook);
	int checked;
	bool started;
	KHttpHeader *last;
};

#endif /*KHTTPPROTOCOLPARSER_H_*/
