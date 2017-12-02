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
#ifndef KHOSTACL_H_
#define KHOSTACL_H_

#include "KMultiAcl.h"
#include "KReg.h"
#include "KXml.h"
#include "KVirtualHostContainer.h"
inline void multi_domain_iterator(void *arg,const char *domain,void *vh)
{
	std::stringstream *s = (std::stringstream *)arg;
	if (!s->str().empty()) {
		*s << "|";
	}
	*s << domain;
}
class KHostAcl: public KMultiAcl {
public:
	KHostAcl() {
		icase_can_change = false;
	}
	virtual ~KHostAcl() {
	}
	KAcl *newInstance() {
		return new KHostAcl();
	}
	const char *getName() {
		return "host";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return KMultiAcl::match(rq->url->host);
	}
};
class KWideHostAcl : public KAcl{
public:
	KWideHostAcl() {
		
	}
	virtual ~KWideHostAcl() {
	}
	KAcl *newInstance() {
		return new KWideHostAcl();
	}
	const char *getName() {
		return "wide_host";
	}
	std::string getDisplay() {
		return this->getValList();
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input name=v size=40 placeholder='abc.com|*.abc.com' value='";
		KWideHostAcl *acl = (KWideHostAcl *) (model);
		if (acl) {
			s << acl->getValList();
		}
		s << "'>";		
		return s.str();
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		return vhc.find(rq->url->host)!=NULL;		
	}
	void editHtml(std::map<std::string,std::string> &attibute)
		throw (KHtmlSupportException)
	{
		vhc.clear();
		char *buf = strdup(attibute["v"].c_str());
		char *hot = buf;
		for (;;) {
			char *p = strchr(hot,'|');
			if (p!=NULL) {
				*p++ = '\0';
			}
			vhc.bind(hot,this,kgl_bind_unique);
			if (p==NULL) {
				break;
			}
			hot = p;
		}
		free(buf);
	}
	void buildXML(std::stringstream &s) {
		s << "v='" << this->getValList() << "'>";
	}
private:
	std::string getValList() {
		std::stringstream s;
		vhc.iterator(multi_domain_iterator,&s);
		return s.str();
	}
	KVirtualHostContainer vhc;
};

#endif /*KHOSTACL_H_*/
