/*
 * KRewriteMark.cpp
 *
 *  Created on: 2010-4-27
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

#include <stdlib.h>
#include <sstream>
#include <vector>
#include <stdio.h>
#include "KRewriteMark.h"
#include "KString.h"
#include "http.h"
#include "malloc_debug.h"
using namespace std;
KRewriteMark::KRewriteMark() {
}

KRewriteMark::~KRewriteMark() {

}
KMark *KRewriteMark::newInstance() {
	return new KRewriteMark;
}
bool KRewriteMark::mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType) {
	return rule.mark(rq, obj,NULL, prefix,NULL ,jumpType);
}
const char *KRewriteMark::getName() {
	return "rewrite";
}
std::string KRewriteMark::getHtml(KModel *model) {
	KRewriteMark *mark = (KRewriteMark *) model;
	stringstream s;
	s << "prefix:<input name='prefix' value='";
	if(mark){
		s << mark->prefix;
	}
	s << "'><br>";
	s << "path:<input name='path' value='";
	if (mark) {
		s << mark->rule.reg.getModel();
	}
	s << "'><br>rewrite to:<input name='dst' value='";
	if (mark && mark->rule.dst) {
		s << mark->rule.dst;
	}
	s << "'><br>";
	s << "code:<input name='code' value='";
	if (mark) {
		s << mark->rule.code;
	}
	s << "'><br>";
	s << "<input type=checkbox name='internal' value='1' ";
	if (mark == NULL || mark->rule.internal) {
		s << "checked";
	}
	s << ">internal";
	s << "<input type=checkbox name='nc' value='1' ";
	if (mark == NULL || mark->rule.nc) {
		s << "checked";
	}
	s << ">no case";
	
	s << "<input type=checkbox name='qsa' value='1' ";
	if (mark  && mark->rule.qsa) {
		s << "checked";
	}
	s << ">qsappend";
	return s.str();
}
std::string KRewriteMark::getDisplay() {
	std::stringstream s;
	if(prefix.size()>0){
		s << prefix << " ";
	}
	s << rule.reg.getModel() << " ";
	if (rule.dst) {
		s << rule.dst;
	}
	if (rule.internal) {
		s << " P";
	}
	if(rule.nc){
		s << " nc";
	}
	return s.str();
}
void KRewriteMark::editHtml(std::map<std::string, std::string> &attribute)
		throw (KHtmlSupportException) {
	prefix = attribute["prefix"];
	rule.parse(attribute);
}
void KRewriteMark::buildXML(std::stringstream &s) {
	if(prefix.size()>0){
		s << " prefix='" << prefix << "'";
	}
	rule.buildXml(s);
	s << ">";
}
