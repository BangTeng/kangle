/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KCONTENTTYPE_H_
#define KCONTENTTYPE_H_
#include "KXmlEvent.h"
#include "utils.h"
#include "KHttpObject.h"
#include "lang.h"
struct mime_type
{
	mime_type()
	{
		memset(this,0,sizeof(mime_type));
	}
	~mime_type()
	{
		if(type){
			free(type);
		}
	}
	void set(const char *type,bool gzip,int max_age)
	{
		if(this->type){
			free(this->type);
		}
		this->type = strdup(type);
		this->gzip = gzip;
		this->max_age = max_age;
	}
	char *apply(KHttpObject *obj)
	{
		if (gzip) {
			obj->need_gzip = 1;
		}
		if (max_age>0) {
			obj->index.max_age = max_age;
			SET(obj->index.flags,ANSW_HAS_MAX_AGE);
		}
		return strdup(type);
	}
	void buildXml(std::stringstream &s)
	{
		s << "type='" << type << "'";
		if (gzip) {
			s << " gzip='1'";
		}
		if (max_age>0) {
			s << " max_age='" << max_age << "'";
		}
	}
	void buildHtml(const char *ext,std::string &url,std::stringstream &s)
	{
		s << "<tr><td>";
		s << "[<a href=\"javascript:if(confirm('really delete?')){ window.location='/vhbase?action=mimetypedelete&ext=";
		s << ext << "&" << url << "';}\">" << LANG_DELETE	<< "</a>]</td>";
		s << "<td>" << ext << "</td>";
		s << "<td>" << type << "</td>";
		s << "<td>" << (gzip?"yes":"&nbsp;") << "</td>";
		s << "<td>" << max_age << "</td>";
		s << "</tr>";
	}
	char *type;
	bool gzip;
	int max_age;
};
class KMimeType
{
public:
	KMimeType()
	{
		defaultMimeType = NULL;
	}
	~KMimeType()
	{
		if (defaultMimeType) {
			delete defaultMimeType;
		}
		std::map<char *,mime_type *,lessp_icase>::iterator it;
		for(it = mimetypes.begin();it!=mimetypes.end();it++){
			xfree((*it).first);
			delete (*it).second;
		}
	}
	//�ϲ�mime���͵�m�С�
	void mergeTo(KMimeType *m,bool overwrite)
	{
		std::map<char *,mime_type *,lessp_icase>::iterator it;
		for(it=mimetypes.begin();it!=mimetypes.end();it++) {
			mime_type *type = (*it).second;
			m->add((*it).first,type->type,type->gzip,type->max_age,overwrite);
		}
		if(defaultMimeType){
			m->add("*",defaultMimeType->type,defaultMimeType->gzip,defaultMimeType->max_age,overwrite);
		}
	}
	char *get(KHttpObject *obj,const char *ext)
	{
		char *type = NULL;
		if(ext==NULL){
			if(defaultMimeType){
				type = defaultMimeType->apply(obj);
			}
			return type;
		}
		std::map<char *,mime_type *,lessp_icase>::iterator it;
		it = mimetypes.find((char *)ext);
		if (it != mimetypes.end()) {
			type = (*it).second->apply(obj);
		} else if(defaultMimeType) {
			type = defaultMimeType->apply(obj);
		}
		return type;
	}
	bool remove(const char *ext)
	{
		bool result = false;
		std::map<char *,mime_type *,lessp_icase>::iterator it;
		if(*ext=='*'){
			if(defaultMimeType){
				delete defaultMimeType;
				defaultMimeType = NULL;
				result = true;
			}
		} else {
			it = mimetypes.find((char *)ext);
			if(it!=mimetypes.end()){
				free((*it).first);
				delete (*it).second;
				mimetypes.erase(it);
				result = true;
			}
		}
		return result;
	}
	void add(const char *ext,const char *type,bool gzip,int max_age,bool overwrite=true)
	{
		std::map<char *,mime_type *,lessp_icase>::iterator it;
		if (*ext=='*') {
			if(defaultMimeType==NULL){
				defaultMimeType = new mime_type;
			} else if(!overwrite) {
				return;
			}
			defaultMimeType->set(type,gzip,max_age);
		} else {
			it = mimetypes.find((char *)ext);
			if (it==mimetypes.end()) {
				mime_type *m_type = new mime_type;
				m_type->set(type,gzip,max_age);
				mimetypes.insert(std::pair<char *,mime_type *>(xstrdup(ext),m_type));
			} else if(overwrite) {
				(*it).second->set(type,gzip,max_age);
			}
		}
	}
	void buildXml(std::stringstream &s)
	{
		if (defaultMimeType) {
			s << "<mime_type ext='*' ";
			defaultMimeType->buildXml(s);
			s << "/>\n";
		}
		std::map<char *,mime_type *,lessp_icase>::iterator it;
		for (it = mimetypes.begin();it!=mimetypes.end();it++) {
			s << "<mime_type ext='" << (*it).first << "' ";
			(*it).second->buildXml(s);
			s << "/>\n";
		}
	}
	void swap(KMimeType *a)
	{
		mimetypes.swap(a->mimetypes);
		mime_type *t = defaultMimeType;
		defaultMimeType = a->defaultMimeType;
		a->defaultMimeType = t;
	}
	friend class KBaseVirtualHost;
private:
	mime_type *defaultMimeType;
	std::map<char *,mime_type *,lessp_icase> mimetypes;

};
class KContentType : public KXmlEvent {
public:
	KContentType();
	virtual ~KContentType();
	void destroy();
	const char *get(KHttpObject *obj,const char *ext);
	bool load(std::string mimeFile);
	bool startElement(KXmlContext *context,
			std::map<std::string,std::string> &attribute);
	bool startCharacter(KXmlContext *context, char *character, int len);
};
extern KContentType contentType;
#endif /*KCONTENTTYPE_H_*/
