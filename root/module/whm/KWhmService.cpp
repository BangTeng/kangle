/*
 * KWhmService.cpp
 *
 *  Created on: 2010-8-30
 *      Author: keengo
 */

#include "KWhmService.h"
#include "WhmContext.h"
#include "WhmPackageManage.h"
#include "KHttpPost.h"

KWhmService::KWhmService() {
	// TODO Auto-generated constructor stub

}

KWhmService::~KWhmService() {
	// TODO Auto-generated destructor stub
}
bool KWhmService::service(KServiceProvider *provider) {
	WhmContext context(provider);
	KUrlValue *uv = context.getUrlValue();
	uv->parse(provider->getQueryString());
	int contentLength = provider->getContentLength();
	if(contentLength > 0){
		KHttpPost httpPost;
		httpPost.init(contentLength);
		int len;
		char *data = provider->getPreLoadedBody(&len);
		if(data && len>0){
			if(!httpPost.addData(data,len)){
				return false;
			}
		}
		if(!httpPost.readData(provider->getInputStream())){
			return false;
		}
		httpPost.parse(uv);
	}
	provider->sendUnknowHeader("Cache-Control","no-cache,no-store");
	context.buildVh();
	WhmPackage *package = packageManage.load(provider->getFileName());
	if (package == NULL) {
		provider->sendStatus(STATUS_NOT_FOUND, "Not Found");
		return true;
	}
	provider->sendStatus(STATUS_OK, "OK");
	const char *callName = uv->getx("whm_call");
	KWStream *out = provider->getOutputStream();
	*out << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";

	if(callName){
		*out << "<" << callName << " whm_version=\"1.0\">";
		int ret = package->process(callName,&context);
		context.flush(ret);
		*out << "</" << callName << ">\n";
	}else{
		*out << "<result whm_version=\"1.0\">whm_call cann't be empty</result>";
	}
	package->destroy();
	return true;
}
