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
#include <string.h>
#include "KAcserver.h"
#include "KSockFastcgiFetchObject.h"
#include "KHttpProxyFetchObject.h"
#include "KAjpFetchObject.h"

#include "KUwsgiFetchObject.h"
#include "KScgiFetchObject.h"
#include "KHmuxFetchObject.h"
#include "malloc_debug.h"
using namespace std;
KJump::~KJump() {
}
KPoolableRedirect::KPoolableRedirect() {
	proto = Proto_http;
}
KPoolableRedirect::~KPoolableRedirect() {

}
KFetchObject *KPoolableRedirect::makeFetchObject(KHttpRequest *rq, KFileName *file) {
	CLR(rq->filter_flags,RQ_FULL_PATH_INFO);
	switch (proto) {
	case Proto_fcgi:
		return new KFastcgiFetchObject();
	case Proto_http:
		return new KHttpProxyFetchObject();
	case Proto_ajp:
		return new KAjpFetchObject();
	case Proto_uwsgi:
		return new KUwsgiFetchObject();
	case Proto_scgi:
		return new KScgiFetchObject();
	case Proto_hmux:
		return new KHmuxFetchObject();
		
	default:
		return NULL;
	}
	return NULL;
}

