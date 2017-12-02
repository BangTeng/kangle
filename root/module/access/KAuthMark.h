/*
 * KAuthMark.h
 *
 *  Created on: 2010-4-28
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


#ifndef KAUTHACL_H_
#define KAUTHACL_H_
#include <map>
#include "KMark.h"
#include "global.h"
#include "KLineFile.h"
#include "KReg.h"
class KAuthMark: public KMark {
public:
	KAuthMark();
	virtual ~KAuthMark();
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType);
	//bool match(KHttpRequest *rq, KHttpObject *obj);
	KMark *newInstance();
	const char *getName();
	std::string getHtml(KModel *model);

	std::string getDisplay();
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException);
	void buildXML(std::stringstream &s);
private:
	std::string getRequireUsers();
	bool loadAuthFile(std::string &path);
	bool checkLogin(KHttpRequest *rq);
	OpenState lastState;
	std::string file;
	time_t lastModified;
	int cryptType;
	int auth_type;
	char *realm;
	char *header;
	KMutex lock;
	std::map<std::string,std::string> users;
	KReg *reg_user;
	bool reg_user_revert;
	bool all;
	bool failed_deny;
	bool file_sign;
	time_t lastLoad;
};

#endif /* KAUTHacl_H_ */
