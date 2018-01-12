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
#ifndef KCACHECONTROLMARK_H_
#define KCACHECONTROLMARK_H_
#include <string>
#include <map>
#include "KMark.h"
#include "do_config.h"
class KCacheControlMark: public KMark {
public:
	KCacheControlMark() {
		max_age = 0;
		staticUrl = false;
		lastModified = false;
		soft = false;
		must_revalidate = false;
	}
	virtual ~KCacheControlMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType) {
#ifdef ENABLE_FORCE_CACHE
		if (staticUrl) {
			if (!obj->force_cache(lastModified)) {
				return false;
			}
		}
#endif
		if (!TEST(obj->index.flags,ANSW_NO_CACHE)) {			
			if (max_age>0) {
				obj->index.max_age = max_age;
				//softָ���Ƿ���max-ageͷ���ͻ�
				SET(obj->index.flags,(soft?ANSW_HAS_EXPIRES:ANSW_HAS_MAX_AGE));
			}
			if (must_revalidate) {
				SET(obj->index.flags,OBJ_MUST_REVALIDATE);
			}
		}
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << "max_age:" << max_age;
		if(staticUrl){
			s << " static";
		}
		if (lastModified) {
			s << " last_modified";
		}
		if (soft) {
			s << " soft";
		}
		if (must_revalidate) {
			s << " must_revalidate";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException) {
		max_age = atoi(attibute["max_age"].c_str());
		if(attibute["static"]=="on" || attibute["static"]=="1"){
			staticUrl = true;
		}else{
			staticUrl = false;
		}
		soft = (attibute["soft"]=="1");
		lastModified = (attibute["last_modified"]=="1");
		must_revalidate = (attibute["must_revalidate"]=="1");
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KCacheControlMark *mark = (KCacheControlMark *) model;
		s << "max_age: <input type=text name=max_age size=6 value='";
		if (mark) {
			s << mark->max_age;
		}
		s << "'><input type=checkbox name='static' value='1' ";
		if(mark && mark->staticUrl){
			s << "checked";
		}
		s << ">" << klang["static"];
		s << "<input type=checkbox name='last_modified' value='1' ";
		if(mark && mark->lastModified){
			s << "checked";
		}
		s << ">last_modified";
		s << "<input type=checkbox name='soft' value='1' ";
		if(mark && mark->soft){
			s << "checked";
		}
		s << ">soft";
		s << "<input type=checkbox name='must_revalidate' value='1' ";
		if(mark && mark->must_revalidate){
			s << "checked";
		}
		s << ">must_revalidate";
		return s.str();
	}
	KMark *newInstance() {
		return new KCacheControlMark();
	}
	const char *getName() {
		return "cache_control";
	}
public:
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute) {
		editHtml(attribute);
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " max_age='" << max_age << "'";
		if(staticUrl){
			s << " static='1'";
		}
		if (soft) {
			s << " soft='1'";
		}
		if (lastModified) {
			s << " last_modified='1'";
		}
		if (must_revalidate) {
			s << " must_revalidate='1'";
		}
		s << ">";
	}
private:
	unsigned max_age;
	bool staticUrl;
	bool lastModified;
	bool soft;
	bool must_revalidate;
};
class KGuestCacheMark : public KMark {
public:
	KGuestCacheMark() {
		max_age = 0;
		lastModified = false;
		soft = false;
		must_revalidate = false;
		skip_set_cookie = false;
	}
	virtual ~KGuestCacheMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType, int &jumpType) {
		if (!TEST(rq->filter_flags, RF_GUEST)) {
			return false;
		}
		if (!TEST(obj->index.flags, ANSW_NO_CACHE)) {
			//������������
			return false;
		}
		if (!skip_set_cookie) {
			if (obj->findHeader(kgl_expand_string("Set-Cookie")) != NULL
				|| obj->findHeader(kgl_expand_string("Set-Cookie2")) != NULL) {
				//��Set-Cookie��˵���ǻ�Ա
				return false;
			}
		}
		if (!obj->force_cache(lastModified)) {
			return false;
		}
		if (skip_set_cookie) {
			obj->removeHttpHeader("Set-Cookie");
			obj->removeHttpHeader("Set-Cookie2");
		}
		if (max_age>0) {
			obj->index.max_age = max_age;
			//softָ���Ƿ���max-ageͷ���ͻ�
			SET(obj->index.flags, (soft ? ANSW_HAS_EXPIRES : ANSW_HAS_MAX_AGE));
		}
		if (must_revalidate) {
			SET(obj->index.flags, OBJ_MUST_REVALIDATE);
		}
		SET(obj->index.flags, OBJ_IS_GUEST);
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << "max_age:" << max_age;
		if (lastModified) {
			s << " last_modified";
		}
		if (soft) {
			s << " soft";
		}
		if (must_revalidate) {
			s << " must_revalidate";
		}
		if (skip_set_cookie) {
			s << " skip_set_cookie";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
		throw (KHtmlSupportException) {
		max_age = atoi(attibute["max_age"].c_str());
		soft = (attibute["soft"] == "1");
		lastModified = (attibute["last_modified"] == "1");
		must_revalidate = (attibute["must_revalidate"] == "1");
		skip_set_cookie = (attibute["skip_set_cookie"] == "1");
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KGuestCacheMark *mark = (KGuestCacheMark *)model;
		s << "max_age: <input type=text name=max_age size=6 value='";
		if (mark) {
			s << mark->max_age;
		}
		s << "'><input type=checkbox name='last_modified' value='1' ";
		if (mark && mark->lastModified) {
			s << "checked";
		}
		s << ">last_modified";
		s << "<input type=checkbox name='soft' value='1' ";
		if (mark && mark->soft) {
			s << "checked";
		}
		s << ">soft";
		s << "<input type=checkbox name='must_revalidate' value='1' ";
		if (mark && mark->must_revalidate) {
			s << "checked";
		}
		s << ">must_revalidate";
		s << "<input type=checkbox name='skip_set_cookie' value='1' ";
		if (mark && mark->skip_set_cookie) {
			s << "checked";
		}
		s << ">skip_set_cookie";
		return s.str();
	}
	KMark *newInstance() {
		return new KGuestCacheMark();
	}
	const char *getName() {
		return "guest_cache";
	}
public:
	bool startElement(KXmlContext *context,
		std::map<std::string, std::string> &attribute) {
		editHtml(attribute);
		return true;
	}
	void buildXML(std::stringstream &s) {
		s << " max_age='" << max_age << "'";
		if (soft) {
			s << " soft='1'";
		}
		if (lastModified) {
			s << " last_modified='1'";
		}
		if (must_revalidate) {
			s << " must_revalidate='1'";
		}
		if (skip_set_cookie) {
			s << " skip_set_cookie='1'";
		}
		s << ">";
	}
private:
	unsigned max_age;
	bool lastModified;
	bool soft;
	bool must_revalidate;
	bool skip_set_cookie;
};
#endif /*KRESPONSEFLAGMARK_H_*/
