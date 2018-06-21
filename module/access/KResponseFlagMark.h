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
#ifndef KRESPONSEFLAGMARK_H_
#define KRESPONSEFLAGMARK_H_
#include <string>
#include <map>
#include "KMark.h"
#include "do_config.h"
class KResponseFlagMark : public KMark {
public:
	KResponseFlagMark() {
		flag=0;
		gzip = false;
		nogzip = false;
		identity_encoding = false;
	}
	virtual ~KResponseFlagMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType) {
		bool result = false;
		if (flag > 0) {
			SET(obj->index.flags, flag);
			result = true;
		}
		if (nogzip) {
			obj->need_gzip = 0;
			result = true;
		}
		if (gzip) {
			obj->need_gzip = 1;
			result = true;
		}
		if (identity_encoding && !TEST(obj->url->encoding, KGL_ENCODING_YES)) {
			obj->url->encoding = (uint8_t)(~KGL_ENCODING_YES);
			result = true;
		}
		return result;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (nogzip) {
			s << "nogzip,";
		}
		if (gzip) {
			s << "gzip,";
		}
		if (TEST(flag,FLAG_NO_NEED_CACHE)) {
			s << "nocache,";
		}
		if (TEST(flag,FLAG_NO_DISK_CACHE)) {
			s << "nodiskcache,";
		}
		if (TEST(flag, OBJ_CACHE_RESPONSE)) {
			s << "cache_response,";
		}
		if (identity_encoding) {
			s << "identity_encoding,";
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute)
			throw(KHtmlSupportException) {
		flag=0;
		nogzip = false;
		gzip = false;
		identity_encoding = false;
		const char *flagStr=attibute["flagvalue"].c_str();
		char *buf = strdup(flagStr);
		char *hot = buf;
		for (;;) {
			char *p = strchr(hot,',');
			if (p) {
				*p = '\0';
			}
			if (strcasecmp(hot,"nogzip")==0) {
				nogzip = true;
			}
			if (strcasecmp(hot,"gzip")==0) {
				gzip = true;
			}
			if (strcasecmp(hot, "identity_encoding") == 0) {
				identity_encoding = true;
			}
			if (strcasecmp(hot,"nocache")==0) {
				SET(flag,FLAG_NO_NEED_CACHE);
			}
			if (strcasecmp(hot,"nodiskcache")==0) {
				SET(flag,FLAG_NO_DISK_CACHE);
			}
			if (strcasecmp(hot, "cache_response") == 0) {
				SET(flag, OBJ_CACHE_RESPONSE);
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
		s << "<input type=text name=flagvalue value='";
		if (model) {
			s << model->getDisplay();
		}

		s << "'>(available:nogzip,gzip,nocache,nodiskcache,identity_encoding)";
		return s.str();
	}
	KMark *newInstance() {
		return new KResponseFlagMark();
	}
	const char *getName() {
		return "response_flag";
	}
public:
	bool startElement(KXmlContext *context,std::map<std::string,std::string> &attribute) {
		editHtml(attribute);
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " flagvalue='" << getDisplay() << "'>";
	}
private:
	int flag;
	bool nogzip;
	bool gzip;
	bool identity_encoding;
};

class KExtendFlagMark : public KMark {
public:
	KExtendFlagMark() {
		no_extend=true;
	}
	virtual ~KExtendFlagMark() {
	}

	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType) {
		if (no_extend) {
			SET(rq->filter_flags,RQ_NO_EXTEND);
		} else {
			CLR(rq->filter_flags,RQ_NO_EXTEND);
		}
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		if(no_extend){
			s << klang["no_extend"];
		} else {
			s << klang["normal"];
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute)
			throw(KHtmlSupportException) {
		if(attibute["no_extend"]=="1"){
			no_extend = true;
		} else {
			no_extend = false;
		}
	
	}
	std::string getHtml(KModel *model) {
		KExtendFlagMark *m_chain=(KExtendFlagMark *)model;
		std::stringstream s;
		s << "<input type=radio name='no_extend' value='1' ";
		if (m_chain==NULL || m_chain->no_extend) {
			s << "checked";
		}
		s << ">" << klang["no_extend"];
		s << "<input type=radio name='no_extend' value='0' ";
		if (m_chain && !m_chain->no_extend) {
			s << "checked";
		}
		s << ">" << klang["clear_no_extend"];
		return s.str();
	}
	KMark *newInstance() {
		return new KExtendFlagMark();
	}
	const char *getName() {
		return "extend_flag";
	}
public:
	bool startElement(KXmlContext *context,
			std::map<std::string,std::string> &attribute) {
//		if (context->qName==getName() && context->getParentName()==MARK_CONTEXT) {
			//flag=atoi(attribute["flag"].c_str());
			editHtml(attribute);
			return true;
//		}
//		return false;
	}
	void buildXML(std::stringstream &s) {
		s << " no_extend='" << (no_extend?1:0) << "'>";
	}
private:
	bool no_extend;
};


#endif /*KRESPONSEFLAGMARK_H_*/
