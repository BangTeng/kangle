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
#ifndef KHEADERACL_H_
#define KHEADERACL_H_
#include "KAcl.h"
#include "KReg.h"
#include "KXml.h"
class KHeaderAcl: public KAcl {
public:
	KHeaderAcl() {
		usereg = true;
	}
	virtual ~KHeaderAcl() {
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "attr:<input name=header value='";
		KHeaderAcl *urlAcl = (KHeaderAcl *) (model);
		if (urlAcl) {
			s << urlAcl->header;
		}
		s << "'>";
		s << "val:<input name=val value='";
		if (urlAcl) {
			s << urlAcl->reg.getModel();
		}
		s << "'>\n<input type=checkbox name='regex' value='1' ";
		if (urlAcl == NULL || urlAcl->usereg) {
			s << "checked";
		}
		s << ">regex";
		return s.str();
	}
	const char *getName() {
		return "header";
	}
	bool matchHeader(KHttpHeader *next) {
		while (next) {
			if (strcasecmp(next->attr, header.c_str()) == 0) {
				if (usereg) {
					if (reg.match(next->val, strlen(next->val), 0) >= 0) {
						return true;
					}
					return false;
				} else {
					if (strncasecmp(next->val, reg.getModel(), regLen) == 0) {
						return true;
					} else {
						return false;
					}
				}
			}
			next = next->next;
		}
		return false;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << header << "/" << reg.getModel();
		if (usereg) {
			s << " [regex]";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute)
			throw (KHtmlSupportException) {
		header = attribute["header"];
		if (attribute["regex"] == "1") {
			usereg = true;
		} else {
			usereg = false;
		}
		reg.setModel(attribute["val"].c_str(), 0);

		regLen = strlen(reg.getModel());

	}
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute) {
		header = attribute["header"];
		if (attribute["regex"] == "1") {
			usereg = true;
		} else {
			usereg = false;
		}
		return true;
	}
	bool startCharacter(KXmlContext *context, char *character, int len) {
		//	printf("character=%s\n",character);
		if(len>0){
			reg.setModel(character, 0);
			regLen = strlen(reg.getModel());
		}
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << "header='" << KXml::param(header.c_str()) << "' ";
		if (usereg) {
			s << "regex='1'";
		}
		s << ">" << CDATA_START << reg.getModel() << CDATA_END;
	}
private:
	std::string header;
	KReg reg;
	int regLen;
	bool usereg;
};

#endif /*KHEADERACL_H_*/
