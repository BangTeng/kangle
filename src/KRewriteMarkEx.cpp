#include <stdlib.h>
#include <string.h>
#include <vector>
#include "KXml.h"
#include "KRewriteMarkEx.h"
#include "http.h"
#include "KSubVirtualHost.h"
#include "KExtendProgram.h"
#include "http.h"
#include "KCdnContainer.h"
#include "malloc_debug.h"
bool check_path_info(const char *file,struct _stat64 *buf)
{
	bool result = false;
	char *str = strdup(file);
	char *end = str + strlen(str) - 1;
	while (end>str) {
		if(*end=='/' 
#ifdef _WIN32
			|| *end=='\\'
#endif
			){
				*end = '\0';
				result = (_stati64(str, buf) == 0);
				if (result) {
					break;
				}
		}
		end--;
	}
	free(str);
	if (result) {
		//�ų�Ŀ¼
		result = (S_ISDIR(buf->st_mode) == 0);
	}
	return result;
}
KRewriteRule::KRewriteRule() {
	dst = NULL;
	qsa = false;
	proxy = NULL;
	internal = true;
	revert = false;
	code = 0;
}
KRewriteRule::~KRewriteRule() {
	if (dst) {
		xfree(dst);
	}
	if (proxy) {
		xfree(proxy);
	}
}
void KRewriteRule::buildXml(std::stringstream &s)
{
	s << " path='";
	if (revert) {
		s << "!";
	}
	s << reg.getModel() << "'";
	s << " dst='" << (dst?dst:"") << "'";
	s << " internal='" << (internal ? "1" : "0") << "'";
	s << " nc='" << (nc ? "1" : "0") << "'";
	if (qsa) {
		s << " qsa='1'";
	}
	if (proxy) {
		s << " proxy='" << proxy << "'";
	}
	if (!internal && code>0) {
		s << " code='" << code << "'";
	}
}
bool KRewriteRule::parse(std::map<std::string,std::string> &attribute)
{
	if (attribute["nc"] == "1") {
		nc = true;
	} else {
		nc = false;
	}
	if (attribute["qsa"]=="1") {
		qsa = true;
	}
	if (attribute["internal"] == "1") {
		internal = true;
	} else {
		internal = false;
	}
	code = atoi(attribute["code"].c_str());
	if (proxy) {
		xfree(proxy);
		proxy = NULL;
	}
	if (!attribute["proxy"].empty()) {
		proxy = strdup(attribute["proxy"].c_str());
	}
	const char *path = attribute["path"].c_str();
	if (*path=='!') {
		//��ת
		revert = true;
		path ++;
	} else {
		revert = false;
	}
	reg.setModel(path, (nc ? PCRE_CASELESS : 0));
	if(dst){
		xfree(dst);
	}
	dst = xstrdup(attribute["dst"].c_str());
	return true;
}
bool KRewriteRule::mark(KHttpRequest *rq, KHttpObject *obj,
						std::list<KRewriteCond *> *conds,const std::string &prefix,const char *rewriteBase, int &jumpType) {
	size_t len = strlen(rq->url->path);
	if (len < prefix.size()) {
		return false;
	}
	//����path
	KRegSubString *subString = reg.matchSubString(rq->url->path + prefix.size(), (int)(len - prefix.size()), 0);
	bool match_result = (subString!=NULL);
	if (revert==match_result) {
		if (subString) {
			delete subString;
		}
		return false;
	}
	KRegSubString *lastCond = NULL;
	bool result = true;
	if (conds) {
		//��������
		std::list<KRewriteCond *>::iterator it;
		for (it = conds->begin(); it != conds->end(); it++) {
			if (result && (*it)->is_or) {
				continue;
			}
			if (!result && !(*it)->is_or) {
				break;
			}
			KStringBuf *str = KRewriteMarkEx::getString(NULL, (*it)->str, rq, NULL, subString);
			result = (*it)->testor->test(str->getString(), &lastCond);
			if ((*it)->revert) {
				result = !result;
			}
			delete str;
		}
	}
	if (!result || dst == NULL || strcmp(dst, "-") == 0) {
		if (subString) {
			delete subString;
		}
		if (lastCond) {
			delete lastCond;
		}
		return result;
	}
	KStringBuf *url = KRewriteMarkEx::getString(
		NULL,
		dst,
		rq,
		lastCond, 
		subString
		);
	if (url) {
		char *param_buf = NULL;
		const char *param = rq->url->getParam(&param_buf);		
		if (param) {
			if (qsa) {
				//append the query string
				if (strchr(url->getString(), '?')) {
					*url << "&" << param;
				} else {
					*url << "?" << param;
				}
			} else if (strchr(url->getString(), '?')==NULL) {
				*url << "?" << param;
			}
		}
		if (param_buf) {
			free(param_buf);
		}
		if (proxy && *proxy=='-') {
			rq->closeFetchObject();
			rq->rewriteUrl(url->getString(),0,(rewriteBase?rewriteBase:prefix.c_str()));
			const char *ssl = NULL;
			if (TEST(rq->url->flags, KGL_URL_SSL)) {
				ssl = "s";
			}
			rq->fetchObj = cdnContainer.get(NULL,rq->url->host,rq->url->port,ssl,0);
			jumpType = JUMP_ALLOW;
		} else {
			bool internal_flag = internal;
			char *u = url->getString();
			if (internal_flag && proxy==NULL) {
				if (strncasecmp(u,"http://",7)==0
					|| strncasecmp(u,"https://",8)==0
					|| strncasecmp(u,"ftp://",6)==0
					|| strncasecmp(u,"mailto:",7)==0) {
					internal_flag = false;
				}
			}
			if (internal_flag) {
				rq->rewriteUrl(u,0,(rewriteBase?rewriteBase:prefix.c_str()));
				if (proxy) {
					KStringBuf *proxy_host = KRewriteMarkEx::getString(
						NULL,
						proxy,
						rq,
						lastCond,
						subString
						);
					if (proxy_host) {
						rq->closeFetchObject();
						rq->fetchObj = cdnContainer.get(proxy_host->getString());
						jumpType = JUMP_ALLOW;
						delete proxy_host;
					}
				}
			} else {
				push_redirect_header(rq,u,(int)strlen(u),code);
				jumpType = JUMP_DENY;
			}
		}
		delete url;
	}
	if (subString) {
		delete subString;
	}
	if (lastCond) {
		delete lastCond;
	}
	return true;
}
bool KFileAttributeTestor::test(const char *str, KRegSubString **lastSubString) {
	struct _stat64 buf;
	bool exsit = (_stati64(str, &buf) == 0);
	if (!exsit && conf.path_info && type!='d') {
		//���path_info���
		exsit = check_path_info(str,&buf);
	}
	if (!exsit) {
		return false;
	}
	switch (type) {
	case 'd':
		return S_ISDIR(buf.st_mode) > 0;
	case 'f':
		return S_ISREG(buf.st_mode) > 0;
	case 's':
		if (!S_ISREG(buf.st_mode)) {
			return false;
		}
		return buf.st_size > 0;
#ifndef _WIN32
	case 'l':
		return S_ISLNK(buf.st_mode) > 0;
	case 'x':
		return TEST(buf.st_mode,S_IXOTH);
#endif
	}
	return false;
}

KRewriteMarkEx::KRewriteMarkEx(void) {
}
KRewriteMarkEx::~KRewriteMarkEx(void) {
	std::list<KRewriteCond *>::iterator it;
	for (it = conds.begin(); it != conds.end(); it++) {
		delete (*it);
	}
	std::list<KRewriteRule *>::iterator it2;
	for (it2 = rules.begin(); it2 != rules.end(); it2++) {
		delete (*it2);
	}
}
bool KRewriteMarkEx::mark(KHttpRequest *rq, KHttpObject *obj,
		const int chainJumpType, int &jumpType) {
	bool result = true;
	std::list<KRewriteRule *>::iterator it2;
	for (it2 = rules.begin(); it2 != rules.end(); it2++) {
		std::list<KRewriteCond *> *conds = NULL;
		if (it2==rules.begin()) {
			conds = &this->conds;
		}
		if (!(*it2)->mark(rq, obj, conds, prefix,(rewriteBase.size()>0?rewriteBase.c_str():NULL),jumpType)) {
			result = false;
			break;
		}
	}
	return result;
}
KMark *KRewriteMarkEx::newInstance() {
	return new KRewriteMarkEx();
}
const char *KRewriteMarkEx::getName() {
	return "rewritex";
}
std::string KRewriteMarkEx::getHtml(KModel *model) {
	return "not support in manage model";
}
std::string KRewriteMarkEx::getDisplay() {
	return "not support in manage model";
}
void KRewriteMarkEx::editHtml(std::map<std::string, std::string> &attribute)
		throw (KHtmlSupportException) {
	
}
bool KRewriteMarkEx::startCharacter(KXmlContext *context, char *character,
		int len) {
	if (context->qName == "cond") {
		KRewriteCond *cond = new KRewriteCond;
		cond->str = xstrdup(context->attribute["str"].c_str());
		if (context->attribute["or"] == "1") {
			cond->is_or = true;
		} else {
			cond->is_or = false;
		}
		if (context->attribute["nc"] == "1") {
			cond->nc = true;
		} else {
			cond->nc = false;
		}
		char *test = character;
		if (*test == '!') {
			cond->revert = true;
			test++;
		}
		if (*test == '-') {
			//it is file attribute
			cond->testor = new KFileAttributeTestor;
		} else if (*test == '>' || *test == '<' || *test == '=') {
			//compare string
			cond->testor = new KCompareTestor;
		} else {
			cond->testor = new KRegexTestor;
			//reg
		}
		if (!cond->testor->parse(test, cond->nc)) {
			delete cond;
			return false;
		}
		conds.push_back(cond);
	}
	return true;
}
void KRewriteMarkEx::buildXML(std::stringstream &s) {
	if (prefix.size() > 0) {
		s << " prefix='" << prefix << "'";
	}
	if (rewriteBase.size()>0) {
		s << " rewrite_base='" << rewriteBase << "'";
	}
	s << ">\n";
	std::list<KRewriteCond *>::iterator it;
	for (it = conds.begin(); it != conds.end(); it++) {
		s << "<cond str='" << (*it)->str << "'";
		if (it != conds.begin() && (*it)->is_or) {
			s << " or='1'";
		}
		s << " nc='" << ((*it)->nc ? "1" : "0") << "'>";
		s << CDATA_START;
		if ((*it)->revert) {
			s << "!";
		}
		s << (*it)->testor->getString() << CDATA_END;
		s << "</cond>\n";
	}
	std::list<KRewriteRule *>::iterator it2;
	for (it2 = rules.begin(); it2 != rules.end(); it2++) {
		s << "<rule ";
		(*it2)->buildXml(s);
		s << "/>\n";
	}
}
bool KRewriteMarkEx::startElement(KXmlContext *context, std::map<std::string,
		std::string> &attribute) {
	if (context->qName == "mark_rewritex") {
		prefix = attribute["prefix"];
		rewriteBase = attribute["rewrite_base"];
	} else if (context->qName == "rule") {
		KRewriteRule *rule = new KRewriteRule;
		rule->parse(attribute);
		rules.push_back(rule);
	}
	return true;
}
void KRewriteMarkEx::getEnv(KHttpRequest *rq, char *env, KStringBuf &s) {
	if (strncasecmp(env, "LA-U:", 5) == 0 || strncasecmp(env, "LA-F:", 5) == 0) {
		env += 5;
	}
	if (strcasecmp(env, "REQUEST_FILENAME") == 0 || strcasecmp(env,
			"SCRIPT_FILENAME") == 0) {
		if (rq->file==NULL && rq->svh) {
			bool exsit;
			rq->svh->bindFile(rq,exsit,true,true);
		}
		if (rq->file) {
			s << rq->file->getName();
		}
		return;
	}
	if (strcasecmp(env, "DOCUMENT_ROOT") == 0) {
		if (rq->svh) {
			s << rq->svh->doc_root;
		}
		return;
	}
	if (strcasecmp(env, "SERVER_PORT") == 0) {
		s << rq->c->socket->get_self_port();
		return;
	}
	if (strcasecmp(env, "SCHEMA") == 0) {
		if (TEST(rq->raw_url.flags,KGL_URL_SSL)) {
			s << "https";
		} else {
			s << "http";
		}
		return;
	}
	if (strcasecmp(env, "SERVER_PROTOCOL") == 0) {
		s << "HTTP/1.1";
		return;
	}
	if (strcasecmp(env, "SERVER_SOFTWARE") == 0) {
		s << PROGRAM_NAME << "/" << VERSION;
		return;
	}
	if (strcasecmp(env,"SERVER_NAME") == 0) {
		s << rq->url->host;
		return;
	}
	if (strcasecmp(env, "REMOTE_ADDR") == 0 || strcasecmp(env, "REMOTE_HOST")
			== 0) {
		s << rq->getClientIp();
		return;
	}
	if (strcasecmp(env, "REMOTE_PORT") == 0) {
		s << rq->c->socket->get_remote_port();
		return;
	}
	if (strcasecmp(env, "REQUEST_METHOD") == 0) {
		s << rq->getMethod();
		return;
	}
	if (strcasecmp(env, "PATH_INFO") == 0 || strcasecmp(env, "REQUEST_URI")
			== 0) {
		s << rq->url->path;
		return;
	}
	if (strcasecmp(env, "QUERY_STRING") == 0) {
		char *param_buf = NULL;
		const char *param = rq->url->getParam(&param_buf);
		if (param) {
			s << param;
		}
		if (param_buf) {
			free(param_buf);
		}
		return;
	}
	if (strcasecmp(env, "THE_REQUEST") == 0) {
		s << rq->getMethod() << " " << rq->url->path;
		if (rq->url->param) {
			s << "?" << rq->url->param;
		}
		s << " HTTP/1.1";
		return;
	}
	if (strcasecmp(env, "HTTPS") == 0) {
		if (TEST(rq->workModel,WORK_MODEL_SSL)) {
			s << "on";
		} else {
			s << "off";
		}
		return;
	}
	if (strncasecmp(env, "HTTP_", 5) == 0 || strncasecmp(env, "HTTP:", 5) == 0) {
		if (env[4] == '_') {
			env += 5;
			char *p = env;
			while (*p) {
				if (*p == '_') {
					*p = '-';
				}
				p++;
			}
		} else {
			env += 5;
		}
		KHttpHeader *av = rq->parser.getHeaders();
		while (av) {
			if (av->attr && strcasecmp(av->attr, env) == 0) {
				s << av->val;
				return;
			}
			av = av->next;
		}
	}
}
void KRewriteMarkEx::getString(const char *prefix, const char *str,KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2,KStringBuf *s)
{
	KExtendProgramString ds(NULL,(rq && rq->svh?rq->svh->vh:NULL));	
	char *buf = xstrdup(str);
	char *hot = buf;
	if (prefix) {
		*s << prefix;
	}
	bool slash = false;
	for (;;) {
		register const char c = *hot;
		hot++;
		if (c == '\0') {
			break;
		}
		if (slash) {
			*s << c;
			slash = false;
			continue;
		}
		if (c=='\\') {
			slash = true;
			continue;
		}
		if (c == '%') {
			if (*hot == '\0') {
				break;
			}
			if (*hot == '{') {
				//it is env
				char *p = strchr(hot, '}');
				if (p == NULL) {
					*s << c;
					continue;
				}
				*p = '\0';
				getEnv(rq, hot + 1, *s);
				hot = p + 1;
				continue;
			}
			if (isdigit(*hot) && s1) {
				char *ss = s1->getString(atoi(hot));
				if (ss) {
					*s << ss;
				}
			}
			hot++;
		} else if (c == '$') {
			if (*hot == '\0') {
				break;
			}
			if (*hot == '{') {
				char *p = strchr(hot, '}');
				if (p == NULL) {
					*s << c;
					continue;
				}
				*p = '\0';
				const char *tmp = ds.interGetValue(hot+1);
				if (tmp!=NULL) {
					*s << tmp;
				}
				hot = p + 1;
				continue;
			}
			if (isdigit(*hot) && s2) {
				char *ss = s2->getString(atoi(hot));
				if (ss) {
					*s << ss;
				}
			}
			hot++;
		} else {
			*s << c;
		}
	}
	xfree(buf);
}
KStringBuf * KRewriteMarkEx::getString(const char *prefix, const char *str,
		KHttpRequest *rq, KRegSubString *s1, KRegSubString *s2) {
	KStringBuf *s = new KStringBuf;
	getString(prefix,str,rq,s1,s2,s);
	return s;
}
