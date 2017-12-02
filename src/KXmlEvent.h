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
#ifndef KXmlEvent_h_1l2kj312312
#define KXmlEvent_h_1l2kj312312
#include <string>
#include <map>
#include "KXmlContext.h"
/*
 * xml�����¼�������
 */
class KXmlEvent {
public:
	virtual ~KXmlEvent() {
	}
	virtual void startXml(const std::string &encoding) {
	}
	virtual void endXml(bool success) {
	}

	/*
	 * ��ʼһ����ǩ
	 * context ��ǩ������
	 * qName ��ǩ����
	 * attribute ��ǩ����
	 */
	virtual bool startElement(std::string &context, std::string &qName,
			std::map<std::string, std::string> &attribute) {
		return false;
	}

	virtual bool startElement(KXmlContext *context, std::map<std::string,
			std::string> &attribute) {
		return false;
	}

	/*
	 * ��ʼһ����ǩ�ı�
	 * context ��ǩ������
	 * qName ��ǩ����
	 * character �ı�
	 * len �ı�����
	 */
	virtual bool startCharacter(std::string &context, std::string &qName,
			char *character, int len) {
		return false;
	}

	virtual bool startCharacter(KXmlContext *context, char *character, int len) {
		return false;
	}

	/*
	 * ����һ����ǩ
	 * context ��ǩ������
	 * qName ��ǩ����
	 */
	virtual bool endElement(std::string &context, std::string &qName) {
		return false;
	}

	virtual bool endElement(KXmlContext *context) {
		return false;
	}

};
#endif
