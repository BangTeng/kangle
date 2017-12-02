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
#ifndef KMETHODACL_H_
#define KMETHODACL_H_

#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KHttpKeyValue.h"
class KMethodAcl: public KAcl {
public:
	KMethodAcl() {
		meth = -1;
	}
	virtual ~KMethodAcl() {
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name=meth value='";
		KMethodAcl *urlAcl = (KMethodAcl *) (model);
		if (urlAcl) {
			s << urlAcl->getDisplay();
		}
		s << "'>";
		return s.str();
	}
	KAcl *newInstance() {
		return new KMethodAcl();
	}
	const char *getName() {
		return "meth";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		if (rq->meth == meth)
			return true;
		return false;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << KHttpKeyValue::getMethod(meth);
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException) {
		if(attibute["meth"].size()>0){
			meth = KHttpKeyValue::getMethod(attibute["meth"].c_str());
		}
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		if(len>0){
			meth = KHttpKeyValue::getMethod(character);
		}
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << ">" << KHttpKeyValue::getMethod(meth);
	}
private:
	char meth;
};

#endif /*KHOSTACL_H_*/
