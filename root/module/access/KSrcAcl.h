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
#ifndef KSRCACL_H_
#define KSRCACL_H_
#include "KIpAclBase.h"

class KSrcAcl : public KIpAclBase {
public:
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		ip_addr addr;
		if (!KSocket::getaddr(rq->getClientIp(), &addr)) {
			return false;
		}
		return matchIP(addr);
	}
	void editHtml(std::map<std::string,std::string> &attibute)
			throw(KHtmlSupportException) {
		addIpModel(attibute["ip"].c_str(), ip);
	}
	std::string getHtml(KModel *acl) {
		std::stringstream s;
		s << "<input name=ip value='";
		KSrcAcl *acl2=(KSrcAcl *)(acl);
		if (acl2) {
			s << acl2->getDisplay();
		}
		s << "'>(cidr format)";
		return s.str();
	}
	KAcl *newInstance() {
		return new KSrcAcl();
	}
	const char *getName() {
		return "src";
	}
};
#endif /*KSRCACL_H_*/
