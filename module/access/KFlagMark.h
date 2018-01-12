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
#ifndef KFLAGMARK_H_
#define KFLAGMARK_H_
#include <string>
#include <map>
#include "KMark.h"
#include "do_config.h"
#include "KSocket.h"
#include "lang.h"
class KFlagMark: public KMark {
public:
	KFlagMark() {
		flag = 0;
		clear = false;
	}
	virtual ~KFlagMark() {
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,int &jumpType) {
		if (clear) {
			CLR(rq->filter_flags,flag);
		} else {
			SET(rq->filter_flags,flag);
		}
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (clear) {
			s << "clear ";
		}
		getFlagString(s);
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException) {
		flag = 0;
		clear = (attibute["clear"] == "1");
		if (attibute["no_cache"] == "1") {
			flag |= RF_NO_CACHE;
		}
		if (attibute["no_disk_cache"] == "1") {
			flag |= RF_NO_DISK_CACHE;
		}
		if(attibute["guest"] == "1"){
			flag |= RF_GUEST;
		}
		if(attibute["ignore_error"] == "1" || attibute["always_online"]=="1" ){
			flag |= RF_ALWAYS_ONLINE;
		}
		if(attibute["upstream_noka"] == "1"){
			flag |= RF_UPSTREAM_NOKA;
		}
		if(attibute["raw_proxy"] == "1"){
			flag |= RF_PROXY_RAW_URL;
		}
		if(attibute["proxy_full_url"] == "1"){
			flag |= RF_PROXY_FULL_URL;
		}
#ifdef ENABLE_TPROXY
		if(attibute["tproxy_trust_dns"] == "1") {
			flag |= RF_TPROXY_TRUST_DNS;
		}
		if(attibute["tproxy_upstream"] == "1") {
			flag |= RF_TPROXY_UPSTREAM;
		}
#endif
		if (attibute["double_cache_expire"] == "1") {
			flag |= RF_DOUBLE_CACHE_EXPIRE;
		}
		if (attibute["x_cache"] == "1") {
			flag |= RF_X_CACHE;
		}
		if (attibute["via"] == "1") {
			flag |= RF_VIA;
		}
		if (attibute["no_x_forwarded_for"] == "1") {
			flag |= RF_NO_X_FORWARDED_FOR;
		}
		if (attibute["x_real_ip"]=="1") {
			flag |= RF_X_REAL_IP;
		}
		if (attibute["no_buffer"]=="1") {
			flag |= RF_NO_BUFFER;
		}
		if (attibute["no_x_sendfile"]=="1") {
			flag |= RF_NO_X_SENDFILE;
		}
		if (attibute["follow_link_all"]=="1") {
			flag |= RF_FOLLOWLINK_ALL;
		}
		if (attibute["follow_link_own"]=="1") {
			flag |= RF_FOLLOWLINK_OWN;
		}
		if (attibute["age"]=="1") {
			flag |= RF_AGE;
		}
		if (attibute["upstream_nosni"] == "1") {
			flag |= RF_UPSTREAM_NOSNI;
		}
		if (attibute["log_drill"] == "1") {
			flag |= RF_LOG_DRILL;
		}
	}
	std::string getHtml(KModel *model) {
		KFlagMark *m_chain = (KFlagMark *) model;
		std::stringstream s;
		s << "<input type=checkbox name='clear' value='1' ";
		if (m_chain && m_chain->clear) {
			s << "checked";
		}
		s << ">clear flag";

		s << "<input type=checkbox name='no_cache' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_NO_CACHE)) {
			s << "checked";
		}
		s << ">" << LANG_NO_CACHE;
		s << "<input type=checkbox name='no_disk_cache' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_NO_DISK_CACHE)) {
			s << "checked";
		}
		s << ">no_disk_cache";
		s << "<input type=checkbox name='guest' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_GUEST)) {
			s << "checked";
		}
		s << ">guest";

		s << "<input type=checkbox name='always_online' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_ALWAYS_ONLINE)) {
			s << "checked";
		}
		s << ">always_online";
		s << "<input type=checkbox name='upstream_noka' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_UPSTREAM_NOKA)) {
			s << "checked";
		}
		s << ">" << klang["upstream_noka"];

		s << "<input type=checkbox name='raw_proxy' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_PROXY_RAW_URL)) {
			s << "checked";
		}
		s << ">" << klang["raw_proxy"];
		s << "<input type=checkbox name='proxy_full_url' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_PROXY_FULL_URL)) {
			s << "checked";
		}
		s << ">proxy_full_url" ;
#ifdef ENABLE_TPROXY
		s << "<input type=checkbox name='tproxy_trust_dns' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_TPROXY_TRUST_DNS)) {
			s << "checked";
		}
		s << ">tproxy_trust_dns";
		s << "<input type=checkbox name='tproxy_upstream' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_TPROXY_UPSTREAM)) {
			s << "checked";
		}
		s << ">tproxy_upstream";
#endif
		s << "<input type=checkbox name='double_cache_expire' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_DOUBLE_CACHE_EXPIRE)) {
			s << "checked";
		}
		s << ">double_cache_expire";

		s << "<input type=checkbox name='x_cache' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_X_CACHE)) {
			s << "checked";
		}
		s << ">x_cache";

		s << "<input type=checkbox name='via' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_VIA)) {
			s << "checked";
		}
		s << ">via";
		s << "<input type=checkbox name='no_x_forwarded_for' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_NO_X_FORWARDED_FOR)) {
			s << "checked";
		}
		s << ">no_x_forwared_for";

		s << "<input type=checkbox name='x_real_ip' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_X_REAL_IP)) {
			s << "checked";
		}
		s << ">x_real_ip";

		s << "<input type=checkbox name='no_buffer' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_NO_BUFFER)) {
			s << "checked";
		}
		s << ">no_buffer";
		s << "<input type=checkbox name='no_x_sendfile' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_NO_X_SENDFILE)) {
			s << "checked";
		}
		s << ">no_x_sendfile";
		s << "<input type=checkbox name='follow_link_all' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_FOLLOWLINK_ALL)) {
			s << "checked";
		}
		s << ">follow_link_all";
		s << "<input type=checkbox name='follow_link_own' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_FOLLOWLINK_OWN)) {
			s << "checked";
		}
		s << ">follow_link_own";

		s << "<input type=checkbox name='age' value='1' ";
		if (m_chain && TEST(m_chain->flag,RF_AGE)) {
			s << "checked";
		}
		s << ">age";
#if (ENABLE_UPSTREAM_SSL && SSL_CTRL_SET_TLSEXT_HOSTNAME)
		s << "<input type=checkbox name='upstream_nosni' value='1' ";
		if (m_chain && TEST(m_chain->flag, RF_UPSTREAM_NOSNI)) {
			s << "checked";
		}
		s << ">upstream_nosni";
#endif
		s << "<input type=checkbox name='log_drill' value='1' ";
		if (m_chain && TEST(m_chain->flag, RF_LOG_DRILL)) {
			s << "checked";
		}
		s << ">log_drill";
		return s.str();
	}
	KMark *newInstance() {
		return new KFlagMark();
	}
	const char *getName() {
		return "flag";
	}
public:
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute) {
		if (attribute["flag"].size()>0) {
			//@deprecated
			flag = atoi(attribute["flag"].c_str());
		} else {
			editHtml(attribute);
		}
		return true;
	}
	void getFlagString(std::stringstream &s)
	{
		if (TEST(flag,RF_NO_CACHE)) {
			s << "no_cache='1' ";
		}
		if (TEST(flag,RF_NO_DISK_CACHE)) {
			s << "no_disk_cache='1' ";
		}
		if (TEST(flag,RF_GUEST)) {
			s << "guest='1' ";
		}
		if (TEST(flag,RF_ALWAYS_ONLINE)) {
			s << "always_online='1' ";
		}
		if (TEST(flag,RF_UPSTREAM_NOKA)) {
			s << "upstream_noka='1' ";
		}
		if (TEST(flag,RF_PROXY_RAW_URL)) {
			s << "raw_proxy='1' ";
		}
		if (TEST(flag,RF_PROXY_FULL_URL)) {
			s << "proxy_full_url='1' ";
		}
		if (TEST(flag, RF_UPSTREAM_NOSNI)) {
			s << "upstream_nosni='1' ";
		}
#ifdef ENABLE_TPROXY
		if (TEST(flag,RF_TPROXY_TRUST_DNS)) {
			s << "tproxy_trust_dns='1' ";
		}
		if (TEST(flag,RF_TPROXY_UPSTREAM)) {
			s << "tproxy_upstream='1' ";
		}
#endif
		if (TEST(flag,RF_DOUBLE_CACHE_EXPIRE)) {
			s << "double_cache_expire='1' ";
		}
		if (TEST(flag,RF_X_CACHE)) {
			s << "x_cache='1' ";
		}
		if (TEST(flag,RF_VIA)) {
			s << "via='1' ";
		}
		if (TEST(flag,RF_NO_X_FORWARDED_FOR)) {
			s << "no_x_forwarded_for='1' ";
		}
		if (TEST(flag,RF_X_REAL_IP)) {
			s << "x_real_ip='1' ";
		}
		if (TEST(flag,RF_NO_BUFFER)) {
			s << "no_buffer='1' ";
		}
		if (TEST(flag,RF_FOLLOWLINK_ALL)) {
			s << "follow_link_all='1' ";
		}
		if (TEST(flag,RF_FOLLOWLINK_OWN)) {
			s << "follow_link_own='1' ";
		}
		if (TEST(flag,RF_NO_X_SENDFILE)) {
			s << "no_x_sendfile='1' ";
		}
		if (TEST(flag,RF_AGE)) {
			s << "age='1' ";
		}
		if (TEST(flag, RF_LOG_DRILL)) {
			s << "log_drill='1' ";
		}
	}
	void buildXML(std::stringstream &s) {
		if (clear) {
			s << "clear='1' ";
		}
		getFlagString(s);
		s << ">";
	}
private:
	int flag;
	bool clear;
};
#endif /*KFLAGMARK_H_*/
