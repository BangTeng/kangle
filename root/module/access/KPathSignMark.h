#ifndef KPATHSIGNMARK_H
#define KPATHSIGNMARK_H
class KPathSignMark : public KMark
{
public:
	KPathSignMark()
	{
		sign="_KS";
		expire="_KE";
		file = false;
	}
	~KPathSignMark()
	{

	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType)
	{
		if (rq->url->param==NULL) {
			return false;
		}
		char *hot = rq->url->param;
		const char *sign_value = NULL;
		const char *expire_value = NULL;
		KStringBuf np;
		for (;;) {
			char *p = strchr(hot,'&');
			if (p) {
				*p = '\0';
			}
			char *value = strchr(hot,'=');
			if (value) {
				*value = '\0';
				value ++;
			}
			if (strcmp(hot,sign.c_str())==0) {
				sign_value = value;
			} else if (strcmp(hot,expire.c_str())==0) {
				expire_value = value;
			} else {
				if (np.getSize()>0) {
					np.write_all("&",1);
				}
				np << hot;
				if (value) {
					np.write_all("=",1);
					np << value;
				}
			}
			if (p==NULL) {
				break;
			}
			hot = p+1;
		}
		if (sign_value==NULL) {
			return false;
		}
		INT64 expire_time =0;
		if (expire_value==NULL) {
			expire_value = strchr(sign_value,'-');
			if (expire_value==NULL) {
				return false;
			}
			expire_value++;
		}
		expire_time = string2int(expire_value);		
		if (expire_time>0 && expire_time < kgl_current_sec) {
			return false;
		}
		KStringBuf s;
		if (file) {
			s << rq->raw_url.path;		
		} else {
			const char *e = strrchr(rq->raw_url.path,'/');
			if (e==NULL) {
				return false;
			}
			int len = e - rq->raw_url.path;
			s.write_all(rq->raw_url.path,len+1);
		}
		s << key.c_str() << expire_value;
		if (strstr(expire_value,"ip")!=NULL) {
			s << rq->getClientIp();
		}
		char expected_sign[33];
		KMD5(s.getBuf(),expected_sign,s.getSize());
		if (strncasecmp(expected_sign,sign_value,32)!=0) {
			return false;
		}
		set_url_param(np,rq->url);
		return true;
	}
	const char *getName() {
		return "path_sign";
	}
	std::string getHtml(KModel *model) {
		std::stringstream s;
		KPathSignMark *m = (KPathSignMark *)model;
		s << "sign:<input name='sign' value='";
		if (m) {
			s << m->sign;
		}
		s << "'>";
		s << "expire:<input name='expire' value='";
		if (m) {
			s << m->expire;
		}
		s << "'>";
		s << "key:<input name='key' value='";
		if (m) {
			s << m->key;
		}
		s << "'>";
		s << "<input name='file' value='1' type='checkbox' ";
		if (m && m->file) {
			s << "checked";
		}
		s << ">file";
		return s.str();
	}
	KMark *newInstance() {
		return new KPathSignMark();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << "sign=" << sign << ",expire=" << expire;
		if (file) {
			s << "[F]";
		}
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attibute)
			throw (KHtmlSupportException)
	{
		sign = attibute["sign"];
		expire = attibute["expire"];
		key = attibute["key"];
		file = (attibute["file"]=="1");
	}
	void buildXML(std::stringstream &s)
	{
		s << "sign='" << sign << "' expire='" << expire << "' key='" << key << "' ";
		if (file) {
			s << "file='1'";
		}
		s << ">";
	}
private:
	void set_url_param(KStringBuf &np,KUrl *url) {
		free(url->param);
		if (np.getSize()>0) {
			url->param = np.stealString();
		} else {
			url->param = NULL;
		}
	}
	std::string sign;
	std::string expire;
	std::string key;
	bool file;
};
#endif
