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
#ifndef KXml_h_sd09f8sf1231
#define KXml_h_sd09f8sf1231
#include<string>
#include<map>
#include<list>
#include<stdio.h>
#include <stdlib.h>
#include "KXmlEvent.h"
#include "KXmlException.h"
#define CDATA_START	"<![CDATA["
#define CDATA_END	"]]>"

#define		PARSE_ERROR			-1
#define		PARSE_CONTINUE		0
#define		PARSE_EVENT			1
#define		PARSE_END			2

#define		START_ELEMENT		1
#define		START_CHAR			2
#define		END_ELEMENT			3
class KXmlResult {
public:
	KXmlResult() {
		context = NULL;
		character = NULL;
		single = false;
		state = 0;
		last = 0;

	}
	~KXmlResult() {
		destroy();
	}
	KXmlContext *context;
	std::map<std::string, std::string> attibute;
	char *character;
	int len;
	bool single;
	int state;
	char *last;
	void destroy() {
		if (state == END_ELEMENT || (state == START_ELEMENT && single)) {
			if (context) {
				delete context;
				context = NULL;
			}
		}
		if (state == START_CHAR) {
			if (character) {
				xfree(character);
				character = NULL;
			}
		}
		state = 0;
	}
};
/*
 * ��xml������: ����:�����(khj99@tom.com)
 * ע��: Ŀǰ����֧��ȫ��xml�﷨(һ��������ȡ�����ļ�)
 */
class KXml {
public:
	/**
	* @deprecated
	* encode,decode,param������������ʱ��
	* ����Ч�ʸ��ߵ�htmlEncode��htmlDecode
	*/
	static std::string encode(std::string str);
	//@deprecated
	static std::string decode(std::string str);
	//@deprecated
	static std::string param(const char *str);

	static char *htmlEncode(const char *str,int &len,char *buf);
	static char *htmlDecode(char *str,int &len);
	/*
	 * �����¼�������
	 */
	void setEvent(KXmlEvent *event);
	void addEvent(KXmlEvent *event);
	/*
	 * ��ʼ�����ļ�
	 */
	bool parseFile(std::string file) throw (KXmlException);
	/*
	 * ��ʼ����һ���ַ���
	 */
	bool parseString(const char *buf) throw (KXmlException);
	KXml();
	~KXml();
	bool startParse(char * buf) throw (KXmlException);
	void setData(void *data)
	{
		this->data = data;
	}
	void *getData()
	{
		return this->data;
	}
private:

	long getFileSize(FILE *fp);
	/*
	 * ��һ���ļ��ж����ַ���
	 */
	int getLine();
	char *getContent(const std::string &file);
	/*
	 * ������ǩ����
	 */
	//void
	//buildAttribute(char *buf, std::map<std::string, std::string> &attibute)
	//		throw (KXmlException);
	/*
	 * �õ���ǩ������
	 */
	//	void getContext(std::string &context);
	KXmlContext *newContext(std::string qName);
	/*
	 * ��ǩ������
	 */
	std::list<KXmlContext *> contexts;
	/*
	 * ������¼�������
	 */
	//KXmlEvent *event;
	std::list<KXmlEvent *> events;
	friend class KXmlContext;
	bool internelParseString(char *buf) throw (KXmlException);
	void clear();
	void startXml(const std::string &encoding) {
		std::list<KXmlEvent *>::iterator it;
		for (it = events.begin(); it != events.end(); it++) {
			(*it)->startXml(encoding);
		}
	}
	void endXml(bool success) {
		std::list<KXmlEvent *>::iterator it;
		for (it = events.begin(); it != events.end(); it++) {
			(*it)->endXml(success);
		}
	}

	std::string encoding;
	int line;
	const char *file;
	char *hot;
	char *origBuf;
	void *data;
};
void buildAttribute(char *buf, std::map<std::string, std::string> &attibute) ;
char *getString(char *str, char **nextstr, const char *ended_chars = NULL,bool end_no_quota_value = false,bool skip_slash=false);
std::string replace(const char *buf,std::map<std::string, std::string> &replaceMap, const char *start =	NULL, const char *end = NULL);
#endif
