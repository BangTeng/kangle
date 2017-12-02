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
#ifndef kacserver_h_skdfjs999sfkh1lk2j3
#define kacserver_h_skdfjs999sfkh1lk2j3
#include <string>
#include <list>
#include "KSocket.h"
#include "KMutex.h"
#include "KJump.h"
#include "KFetchObject.h"
#include "KRedirect.h"
#include "KUpstreamSelectable.h"
#include "malloc_debug.h"
#include "KFileName.h"
class KFetchObject;

class KPoolableRedirect: public KRedirect {
public:
	Proto_t proto;
public:
	KPoolableRedirect();
	virtual ~KPoolableRedirect();
	KFetchObject *makeFetchObject(KHttpRequest *rq, KFileName *file);
	virtual KUpstreamSelectable *getConnection(bool &half)
	{
		return NULL;
	}
	virtual bool isChanged(KPoolableRedirect *rd)
	{
		if (this->proto != rd->proto) {
			return true;
		}
		return false;
	}
	static const char *buildProto(Proto_t proto) {
		switch (proto) {
		case Proto_http:
			return "http";
		case Proto_fcgi:
			return "fastcgi";
		case Proto_ajp:
			return "ajp";
		case Proto_uwsgi:
			return "uwsgi";
		case Proto_scgi:
			return "scgi";
		case Proto_hmux:
			return "hmux";
		case Proto_spdy:
			return "spdy";
		case Proto_tcp:
			return "tcp";
		}
		return "unknow";
	}
	static Proto_t parseProto(const char *proto) {
		if (strcasecmp(proto, "fcgi") == 0 || strcasecmp(proto, "fastcgi") == 0) {
			return Proto_fcgi;
		}
		if (strcasecmp(proto, "ajp") == 0) {
			return Proto_ajp;
		}
		if (strcasecmp(proto,"uwsgi")==0) {
			return Proto_uwsgi;
		}
		if (strcasecmp(proto,"scgi")==0) {
			return Proto_scgi;
		}
		if (strcasecmp(proto,"hmux")==0) {
			return Proto_hmux;
		}
		if (strcasecmp(proto, "tcp") == 0) {
			return Proto_tcp;
		}
		return Proto_http;
	}
};
#endif
