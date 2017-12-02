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
#ifndef KTRYFILEACL_H_
#define KTRYFILEACL_H_
#include "KXml.h"
class KTryFileAcl: public KAcl {
public:
	KTryFileAcl() {

	}
	virtual ~KTryFileAcl() {
	}
	KAcl *newInstance() {
		return new KTryFileAcl();
	}
	const char *getName() {
		return "try_file";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->svh==NULL) {
			return false;
		}
		KFileName file;
		bool exsit = false;
		KVirtualHost *vh = rq->svh->vh;
		if (!vh->alias(TEST(rq->workModel,WORK_MODEL_INTERNAL)>0,rq->url->path,&file,exsit,rq->getFollowLink())) {
			exsit = file.setName(rq->svh->doc_root, rq->url->path, rq->getFollowLink());
		}
		if (file.isDirectory()) {			
			KFileName *defaultFile = NULL;
			exsit = vh->getIndexFile(rq,&file,&defaultFile,NULL);
			if (defaultFile) {
				delete defaultFile;
			}
		}
		return exsit;
	}
	std::string getHtml(KModel *model) {
		return "";
	}	
	std::string getDisplay() {
		return "";
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException) {
		
	}
	void buildXML(std::stringstream &s) {
		s << " >";
	}
};

#endif
