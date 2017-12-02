#ifndef KVARYMARK_H
#define KVARYMARK_H
class KVaryMark : public KMark
{
public:
	KVaryMark()
	{
		attr = NULL;
	}
	~KVaryMark()
	{
		if (attr) {
			free(attr);
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType) {
		if (attr==NULL) {
			return false;
		}
		bool result = false;
		KHttpHeader *header = rq->parser.getHeaders();
		while (header) {
			if (strcasecmp(header->attr,attr)==0) {
				KRegSubString *sub = vary.matchSubString(header->val,strlen(header->val),0);
				if (sub) {
					if (sub->count>1) {
						KStringBuf np;
						if (rq->url->param) {
							np << rq->url->param;
							free(rq->url->param);
						}
						if (TEST(rq->url->flags,KGL_URL_VARIED) && strrchr(np.getString(),VARY_URL_KEY)) {
							//已经设置过vary,则附加上去
							np << "|";
						} else {
							np << (const char)VARY_URL_KEY;
						}
						for (int i=1;i<sub->count;i++) {
							if (i>1) {
								np << "_";
							}
							np << sub->getString(i);
						}
						rq->url->param = np.stealString();
						SET(rq->url->flags,KGL_URL_VARIED);
					}
					delete sub;
					result = true;
				}
			}
			header = header->next;
		}
		return result;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (attr) {		
			s << attr << ":";
		}
		s << vary.getModel();
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute)
		throw(KHtmlSupportException) {
		if (attr) {
			free(attr);
		}
		attr = strdup(attibute["attr"].c_str());
		vary.setModel(attibute["vary"].c_str(),PCRE_CASELESS);
	}
	std::string getHtml(KModel *model) {
		KVaryMark *m = (KVaryMark *)model;
		std::stringstream s;
		s << "attr:<input name='attr' value='";
		if (m && m->attr) {
			s << m->attr;
		}
		s << "'>";
		s << "vary(regex):<input name='vary' value='";
		if (m) {
			s << m->vary.getModel();
		}
		s << "'>";
		return s.str();
	}
	KMark *newInstance() {
		return new KVaryMark;
	}
	const char *getName() {
		return "vary";
	}
	void buildXML(std::stringstream &s) {
		if (attr) {
			s << "attr='" << KXml::param(attr) << "' ";
		}
		s << "vary='" << KXml::param(vary.getModel()) << "' ";
		s << ">";
	}
private:
	char *attr;
	KReg vary;
};
#endif
