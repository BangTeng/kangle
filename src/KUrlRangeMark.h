#ifndef KURLRANGEMARK_H
#define KURLRANGEMARK_H
/*
* url��range����
*/
class KUrlRangeMark : public KMark
{
public:
	KUrlRangeMark()
	{
		
	};
	~KUrlRangeMark()
	{
	};
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType) {
		if (TEST(rq->flags,RQ_HAVE_RANGE)) {
			return false;
		}
		KStringBuf u;
		KUrl *url = rq->url;
		url->getUrl(u);
		KRegSubString *s = range_from.matchSubString(u.getBuf(),u.getSize(),0);
		bool result = false;
		if (s) {
			const char *from = s->getString(1);
			if (from) {
				rq->range_to = -1;
				rq->range_from = string2int(from);
				const char *to = s->getString(2);
				if (to==NULL) {
					KRegSubString *s2 = range_to.matchSubString(u.getBuf(),u.getSize(),0);
					if (s2) {
						to = s2->getString(1);
						if(to){
							rq->range_to = string2int(to);
						}
						delete s2;
					}
				} else {
					rq->range_to = string2int(to);
				}
				KStringBuf v;
				v << "bytes=" << rq->range_from << "-";
				if (rq->range_to>=0) {
					v << rq->range_to;
				}
				SET(rq->flags,RQ_HAVE_RANGE);
				SET(rq->raw_url.flags,KGL_URL_RANGED);
				rq->parser.insertHeader(kgl_expand_string("Range"),v.getString(),v.getSize());
				result = true;
			}
			delete s;
		}
		return result;
	}
	std::string getDisplay() {
		std::stringstream s;
		s << range_from.getModel() << " ";
		s << range_to.getModel();
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute)
		throw(KHtmlSupportException) {
		range_from.setModel(attibute["range_from"].c_str(),PCRE_CASELESS);
		range_to.setModel(attibute["range_to"].c_str(),PCRE_CASELESS);
	}
	std::string getHtml(KModel *model) {
		KUrlRangeMark *m = (KUrlRangeMark *)model;
		std::stringstream s;
		s << "range_from:<input name='range_from' value='";
		if (m) {
			s << m->range_from.getModel();
		}
		s << "'>";
		s << "range_to:<input name='range_to' value='";
		if (m) {
			s << m->range_to.getModel();
		}
		s << "'>";
		return s.str();
	}
	KMark *newInstance() {
		return new KUrlRangeMark;
	}
	const char *getName() {
		return "url_range";
	}
	void buildXML(std::stringstream &s) {
		s << "range_from='" << range_from.getModel() << "' ";
		s << "range_to='" << range_to.getModel() << "' ";
		s << ">";
	}
private:
	KReg range_from;
	KReg range_to;
};
#endif
