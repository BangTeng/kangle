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
#ifndef KLOADAVGACL_H_
#define KLOADAVGACL_H_

#include "KAcl.h"
class KLoadAvgAcl : public KAcl {
public:
	KLoadAvgAcl() {
		loadavg=0;
		maxavg=0;
		lastRead=0;
	}
	virtual ~KLoadAvgAcl() {
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		s << "&gt;<input name=maxavg value='";
		KLoadAvgAcl *acl=(KLoadAvgAcl *)(model);
		if (acl) {
			s << acl->maxavg;
		}
		s << "'>";
		return s.str();
	}
	KAcl *newInstance() {
		return new KLoadAvgAcl();
	}
	const char *getName() {
		return "loadavg";
	}
	bool match(KHttpRequest *rq, KHttpObject *obj) {
		readAvg();
		if(loadavg>maxavg){
			return true;
		}
		return false;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << "&gt;" << maxavg << "(cur:" << loadavg << ")";
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute)
			throw(KHtmlSupportException) {
		maxavg = atoi(attibute["maxavg"].c_str());
	}
	bool startElement(KXmlContext *context,
				std::map<std::string,std::string> &attribute) {
//			if (context->getParentName()==ACL_CONTEXT && context->qName==getName()) {
				editHtml(attribute);
				return true;
//			}
//			return false;
		}
	void buildXML(std::stringstream &s) {
		s << " maxavg='" << maxavg << "'>";
	}
	void readAvg()
	{
		time_t curTime=kgl_current_sec;
		if(curTime-lastRead>5){
			lastRead=curTime;
			FILE *fp=fopen("/proc/loadavg","rt");
			if(fp==NULL){
				fprintf(stderr,"cann't open /proc/loadavg file");
				return;
			}
			char buf[32];
			memset(buf,0,sizeof(buf));
			fread(buf,1,sizeof(buf)-1,fp);
			loadavg=atoi(buf);
			fclose(fp);
		}
	}
private:
	int loadavg;
	int maxavg;
	time_t lastRead;
};


#endif /*KLOADAVGACL_H_*/
