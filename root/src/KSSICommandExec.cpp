/*
 * KSSICommandExec.cpp
 *
 *  Created on: 2010-8-11
 *      Author: keengo
 */

#include "KSSICommandExec.h"
#include "KCgiFetchObject.h"
//#include "KLocalFetchObject.h"
#include "KHttpProxyFetchObject.h"
#include "KCgiRedirect.h"
#include "http.h"
#include "malloc_debug.h"
KSSICommandExec::KSSICommandExec() {
	// TODO Auto-generated constructor stub

}

KSSICommandExec::~KSSICommandExec() {
	// TODO Auto-generated destructor stub
}
Process_status KSSICommandExec::process(KSSIContext *context, char *cmd, std::map<char *,
		char *, lessp_icase> &attribute, KWStream *out) {
#if 0
	KHttpRequest *rq = context->getRequest();
	bool result = true;
	std::map<char *, char *, lessp_icase>::iterator it;
	it = attribute.find((char *) "cmd");
	if (context->getFileCount() + rq->stackSize > 128) {
		*out << "<!-- include level have reached max[128] -->";
		return true;
	}
	if (it != attribute.end()) {
		char *cmd = (*it).second;
		char *p = strchr(cmd, ' ');
		std::string args;
		if (p) {
			*p = '\0';
			args = p + 1;

		}
		KCgiRedirect rd(cmd);
		if (args.size() > 0) {
			rd.setArg(args);
		}
		//rq->fetchObj
		KCgiFetchObject *fo = (KCgiFetchObject *) rd.makeFetchObject(rq, NULL);
		fo->setCmdModel(true);
		fo->process(rq);
#if 0
		if (ret == SEND_HEAD_SUCCESS) {
			char buf[512];
			for (;;) {
				int len = fo->read(buf, sizeof(buf));
				if (len <= 0) {
					break;
				}
				if (out->write_all(buf, len) != STREAM_WRITE_SUCCESS) {
					result = false;
					break;
				}
			}
		}
		fo->close(true);
		//fo->close(rq, true);
		delete fo;
#endif
	}
	if (!result) {
		return false;
	}
	char *url = NULL;
	it = attribute.find((char *) "cgi");
	if (it != attribute.end()) {
		url = (*it).second;
	}
	if (url == NULL) {
		it = attribute.find((char *) "url");
		if (it != attribute.end()) {
			url = (*it).second;
		}
	}
	if (url == NULL) {
		return true;
	}
	KHttpRequest rq2;
	rq2.init();
	rq2.server = rq->c->socket;
	rq2.auth = rq->auth;
	rq2.stackSize = rq->stackSize + 1;
	rq2.workModel = rq->workModel;
	rq2.meth = rq->meth;
	SET(rq2.workModel,WORK_MODEL_INTERNAL|WORK_MODEL_SYNC);
	if (!rq2.rewriteUrl(url)) {
		*out << "<!-- url is error -->";
		goto done;
	}
#if 0
	if (strcasecmp(rq->url->host, rq2.url.host) == 0) {
		//内部
		rq2.fetchObj = new KLocalFetchObject();
		rq2.svh = rq->svh;
	} else {
		//外部
		rq2.fetchObj = new KHttpProxyFetchObject();
	}
	result = processFetchObject(&rq2, out);
#endif
	done: rq2.server = NULL;
	rq2.auth = NULL;
	rq2.svh = NULL;
	return result;
#endif
	return Process_failed;
}
bool KSSICommandExec::processFetchObject(KHttpRequest *rq, KWStream *out) {
#if 0
	bool upStreamEnd = out->upStreamEnd;
	//阻断write_end调用
	out->upStreamEnd = false;
	rq->out = out;
	processHttpRequest(rq);
	out->upStreamEnd = upStreamEnd;
#endif
	return true;
}
