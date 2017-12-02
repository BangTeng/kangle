#ifndef KREPLACEIPMARK_H
#define KREPLACEIPMARK_H
#include "KMark.h"
class KReplaceIPMark : public KMark
{
public:
	KReplaceIPMark()
	{
		header = "X-Real-Ip";
		val = NULL;
	}
	~KReplaceIPMark()
	{
		if (val) {
			delete val;
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
					const int chainJumpType, int &jumpType)
	{
		KHttpHeader *h = rq->parser.headers;
		KHttpHeader *prev = NULL;
		while (h) {
			if (strcasecmp(h->attr, header.c_str()) == 0) {
				KRegSubString *sub = NULL;
				if (val) {
					sub = val->matchSubString(h->val, strlen(h->val), 0);
				}
				if (val == NULL || sub) {
					if (prev) {
						prev->next = h->next;
					} else {
						rq->parser.headers = h->next;
					}
					if (rq->client_ip) {
						free(rq->client_ip);
					}
					if (val == NULL) {
						rq->client_ip = h->val;
					} else {
						free(h->val);
						char *ip = sub->getString(1);
						if (ip) {
							rq->client_ip = strdup(ip);
						}
					}
					free(h->attr);
					free(h);
					if (TEST(rq->raw_url.flags,KGL_URL_ORIG_SSL)) {
						SET(rq->raw_url.flags,KGL_URL_SSL);
					} else {
						CLR(rq->raw_url.flags,KGL_URL_SSL);
					}
					return true;
				}
			}
			prev = h;
			h = h->next;
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KReplaceIPMark;
	}
	const char *getName()
	{
		return "replace_ip";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "header:<input name='header' value='";
		KReplaceIPMark *m = (KReplaceIPMark *)model;
		if (m) {
			s << m->header;
		} else {
			s << "X-Real-Ip";
		}
		s << "'>";
		s << "val(regex):<input name='val' value='";
		if (m && m->val) {
			s << m->val->getModel();
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << header;
		if (val) {
			s << ":" << val->getModel();
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		header = attribute["header"];
		std::string val = attribute["val"];
		if (this->val) {
			delete this->val;
			this->val = NULL;
		}
		if (!val.empty()) {
			this->val = new KReg;
			this->val->setModel(val.c_str(), PCRE_CASELESS);
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " header='" << header << "' ";
		if (val) {
			s << "val='" << KXml::param(val->getModel()) << "' ";
		}
		s << "> ";
	}
private:
	std::string header;
	KReg *val;
};
class KParentMark : public KMark
{
public:
	KParentMark()
	{
		memset(&upstream_sign, 0, sizeof(upstream_sign));
	}
	~KParentMark()
	{
		if (upstream_sign.data) {
			free(upstream_sign.data);
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
					const int chainJumpType, int &jumpType)
	{
		if (upstream_sign.data) {
			KHttpHeader *h = rq->parser.removeHeader("x-real-ip-sign");
			if (h == NULL) {
				return false;
			}
			bool matched = false;
			if (h->val_len == upstream_sign.len && memcmp(h->val,upstream_sign.data,h->val_len)==0) {
				matched = true;
			}
			free(h->attr);
			free(h->val);
			free(h);
			if (!matched) {
				return false;
			}
		}
		KHttpHeader *h = rq->parser.removeHeader("X-Real-Ip");
		if (h == NULL) {
			return true;
		}
		if (rq->client_ip) {
			free(rq->client_ip);
		}
		//steal
		rq->client_ip = h->val;
		free(h->attr);
		free(h);
		if (TEST(rq->raw_url.flags, KGL_URL_ORIG_SSL)) {
			SET(rq->raw_url.flags, KGL_URL_SSL);
		} else {
			CLR(rq->raw_url.flags, KGL_URL_SSL);
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KParentMark;
	}
	const char *getName()
	{
		return "parent";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "parent sign:<input name='upstream_sign' value='";
		KParentMark *m = (KParentMark *)model;
		if (m && m->upstream_sign.data) {
			s.write(m->upstream_sign.data,m->upstream_sign.len);
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		if (upstream_sign.data) {
			s.write(upstream_sign.data, upstream_sign.len);
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		std::string upstream_sign = attribute["upstream_sign"];
		if (this->upstream_sign.data) {
			free(this->upstream_sign.data);
			this->upstream_sign.data = NULL;
		}
		this->upstream_sign.len = upstream_sign.size();
		if (this->upstream_sign.len > 0) {
			this->upstream_sign.data = strdup(upstream_sign.c_str());
		}
	}
	void buildXML(std::stringstream &s)
	{
		if (upstream_sign.data) {
			s << " upstream_sign='";
			s.write(upstream_sign.data, upstream_sign.len);
			s << "'";
		}
		s << "> ";
	}
private:
	kgl_str_t upstream_sign;
};
class KSelfIPMark : public KMark
{
public:
	KSelfIPMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
					const int chainJumpType, int &jumpType)
	{
		if (rq->bind_ip) {
			free(rq->bind_ip);
			rq->bind_ip = NULL;
		}
		if (ip.empty()) {
			char ip[MAXIPLEN];
			rq->c->socket->get_self_ip(ip,sizeof(ip));
			rq->bind_ip = strdup(ip);
		} else if (ip[0] == '$') {
			rq->bind_ip = strdup(rq->getClientIp());
		} else if (ip[0] != '-') {
			rq->bind_ip = strdup(ip.c_str());
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KSelfIPMark;
	}
	const char *getName()
	{
		return "self_ip";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "ip:<input name='ip' value='";
		KSelfIPMark *m = (KSelfIPMark *)model;
		if (m) {
			s << m->ip;
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		return ip;
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		ip = attribute["ip"];
	}
	void buildXML(std::stringstream &s)
	{
		s << " ip='" << ip << "'>";
	}
private:
	std::string ip;
};
#endif

