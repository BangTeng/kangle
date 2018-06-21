/*
 * KHttpProxyFetchObject.h
 *
 *  Created on: 2010-4-20
 *      Author: keengo
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

#ifndef KHTTPPROXYFETCHOBJECT_H_
#define KHTTPPROXYFETCHOBJECT_H_

#include "KFetchObject.h"
#include "KAcserver.h"
#include "KSocket.h"
#include "KAsyncFetchObject.h"
#include "KHttpObjectParserHook.h"
#include "KHttpEnv.h"
 
class KHttpProxyFetchObject: public KAsyncFetchObject {
public:
	KHttpProxyFetchObject()
	{
	}
	virtual ~KHttpProxyFetchObject()
	{
	}
	KFetchObject *clone(KHttpRequest *rq)
	{
		if (brd) {
			return KFetchObject::clone(rq);
		}
		return new KHttpProxyFetchObject();
	}
	bool needTempFile()
	{
		return false;
	}
protected:
	void buildHead(KHttpRequest *rq);
	Parse_Result parseHead(KHttpRequest *rq,char *buf,int len);
	char *nextBody(KHttpRequest *rq,int &len)
	{
		if (client->parser->bodyLen>0) {
			len = client->parser->bodyLen;
			client->parser->bodyLen = 0;
			return client->parser->body;
		}
		if (hot) {
			len = (int)(hot - header);
			hot = NULL;
			return header;
		}
		return NULL;
	}
	Parse_Result parseBody(KHttpRequest *rq,char *data,int len)
	{
		hot = data + len;
		return Parse_Continue;
	}
	bool checkContinueReadBody(KHttpRequest *rq)
	{
		if (rq->ctx->know_length && rq->ctx->left_read<=0) {
			assert(rq->ctx->left_read==0);
			expectDone(rq);
			return false;
		}
		return true;
	}
	void expectDone(KHttpRequest *rq)
	{
		if (client->hook->keep_alive_time_out>=0) {
			lifeTime = client->hook->keep_alive_time_out;
		} else {
			lifeTime = 0;
		}
		KAsyncFetchObject::expectDone(rq);
	}
	void readBodyEnd(KHttpRequest *rq)
	{
		expectDone(rq);
	}
private:
	
};

#endif /* KHTTPPROXYFETCHOBJECT_H_ */
