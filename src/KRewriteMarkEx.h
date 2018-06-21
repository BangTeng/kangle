#ifndef KREWRITEMARKEX_H
#define KREWRITEMARKEX_H
#include <list>
#include "KMark.h"
#include "KReg.h"
#include "KString.h"
/*
 �������Ի���
 */
class KRewriteCondTestor {
public:
	virtual ~KRewriteCondTestor() {
	}
	/*
	 test a string the lastSubString must be delete if exsit when reset it;
	 */
	virtual bool test(const char *str, KRegSubString **lastSubString) = 0;
	virtual std::string getString() = 0;
	virtual bool parse(char *str, bool nc) = 0;
};
/*
 �ļ����Բ���
 */
class KFileAttributeTestor: public KRewriteCondTestor {
public:
	KFileAttributeTestor() {

	}
	bool test(const char *str, KRegSubString **lastSubString);
	std::string getString() {
		std::stringstream s;
		s << "-" << type;
		return s.str();
	}
	bool parse(char *str, bool nc) {
		type = str[1];
		return true;
	}
private:
	char type;
};
/*
 ������ʽ����
 */
class KRegexTestor: public KRewriteCondTestor {
public:
	std::string getString() {
		return reg.getModel();
	}
	bool parse(char *str, bool nc) {
		return reg.setModel(str, (nc ? PCRE_CASELESS : 0));
	}
	bool test(const char *str, KRegSubString **lastSubString) {
		if (*lastSubString) {
			delete *lastSubString;
		}
		*lastSubString = reg.matchSubString(str, (int)strlen(str), 0);
		return *lastSubString != NULL;
	}
private:
	KReg reg;
};
/*
 �ַ����Ƚϲ���
 */
class KCompareTestor: public KRewriteCondTestor {
public:
	KCompareTestor() {
		str = NULL;
	}
	~KCompareTestor() {
		if (str) {
			xfree(str);
		}
	}
	bool test(const char *buf, KRegSubString **lastSubString) {
		int r;
		if(str==NULL){
			return false;
		}
		if (nc) {
			r = strcasecmp(str+1, buf );
		} else {
			r = strcmp(str+1, buf);
		}
		switch (*str) {
		case '>':
			return r > 0;
		case '=':
			return r == 0;
		case '<':
			return r < 0;
		}
		return false;
	}
	std::string getString() {
		return str;
	}
	bool parse(char *buf, bool nc) {
		if (*buf == '\0') {
			return false;
		}
		this->nc = nc;
		str = xstrdup(buf);
		return true;
	}
private:
	char *str;
	bool nc;
};
class KRewriteCond {
public:
	KRewriteCond() {
		str = NULL;
		testor = NULL;
		revert = false;
		is_or = false;
	}
	~KRewriteCond() {
		if (str) {
			xfree(str);
		}
		if (testor) {
			delete testor;
		}
	}
	bool nc;
	bool revert;
	//true is or false is and with prev cond;
	//the first cond or must be false
	bool is_or;
	//cond origin str
	char *str;
	KRewriteCondTestor *testor;
};
class KRewriteRule {
public:
	KRewriteRule();
	~KRewriteRule();
	bool mark(KHttpRequest *rq, KHttpObject *obj, std::list<KRewriteCond *> *conds,
			const std::string &prefix,const char *rewriteBase,int &jumpType);
	bool parse(std::map<std::string,std::string> &attribute);
	void buildXml(std::stringstream &s);
	bool revert;
	KReg reg;
	char *dst;
	bool internal;
	bool nc;
	bool qsa;
	char *proxy;
	int code;
};
class KRewriteMarkEx: public KMark {
public:
	KRewriteMarkEx(void);
	~KRewriteMarkEx(void);
	bool mark(KHttpRequest *rq, KHttpObject *obj, const int chainJumpType,
			int &jumpType);
	KMark *newInstance();
	const char *getName();
	std::string getHtml(KModel *model);

	std::string getDisplay();
	static void getEnv(KHttpRequest *rq, char *env, KStringBuf &s);
	void editHtml(std::map<std::string, std::string> &attribute)
			throw (KHtmlSupportException);
	bool startElement(KXmlContext *context,
			std::map<std::string, std::string> &attribute);
	bool startCharacter(KXmlContext *context, char *character, int len);
	void buildXML(std::stringstream &s);
	static KStringBuf *getString(const char *prefix, const char *str,
			KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2);
	static void getString(const char *prefix, const char *str,
			KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2,KStringBuf *s);
private:
	std::list<KRewriteCond *> conds;
	std::list<KRewriteRule *> rules;
	std::string prefix;
	std::string rewriteBase;
	int flag;
};
#endif
