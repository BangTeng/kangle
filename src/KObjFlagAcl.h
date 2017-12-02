#ifndef KOBJFLAGACL_H
#define KOBJFLAGACL_H
#include "KAcl.h"
class KObjFlagAcl : public KAcl {
public:
	KObjFlagAcl() {
		flag=0;
	}
	virtual ~KObjFlagAcl() {
	}

	bool match(KHttpRequest *rq,KHttpObject *obj)
	{
		return TEST(obj->index.flags,flag)>0;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (TEST(flag,ANSW_NO_CACHE)) {
			s << "nocache,";
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute)
			throw(KHtmlSupportException) {
		flag=0;
		const char *flagStr=attibute["flag"].c_str();
		char *buf = strdup(flagStr);
		char *hot = buf;
		for (;;) {
			char *p = strchr(hot,',');
			if (p) {
				*p = '\0';
			}
			if (strcasecmp(hot,"nocache")==0) {
				SET(flag,ANSW_NO_CACHE);
			}
			if (p==NULL) {
				break;
			}
			hot = p+1;
		}
		free(buf);
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "<input type=text name=flag value='";
		if (model) {
			s << model->getDisplay();
		}

		s << "'>(available:nocache)";
		return s.str();
	}
	KAcl *newInstance() {
		return new KObjFlagAcl();
	}
	const char *getName() {
		return "obj_flag";
	}
public:
	bool startElement(KXmlContext *context,std::map<std::string,std::string> &attribute) {
		editHtml(attribute);
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " flag='" << getDisplay() << "'>";
	}
private:
	int flag;
};
#endif
