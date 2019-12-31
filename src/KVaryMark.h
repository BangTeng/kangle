#ifndef KVARYMARK_H
#define KVARYMARK_H
class KVaryMark : public KMark
{
public:
	KVaryMark()
	{
		header = NULL;
	}
	~KVaryMark()
	{
		if (header) {
			xfree(header);
		}
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,const int chainJumpType, int &jumpType) {
		if (header ==NULL) {
			return false;
		}
		kassert(obj);
		if (TEST(obj->index.flags, OBJ_HAS_VARY)) {
			return false;
		}
		if (!obj->AddVary(rq, header, header_len)) {
			obj->insertHttpHeader(kgl_expand_string("Vary"), header, header_len);
		}
		return true;
	}
	std::string getDisplay() {
		std::stringstream s;
		if (header) {
			s << header;
		}
		return s.str();
	}
	void editHtml(std::map<std::string,std::string> &attibute){
		if (header) {
			xfree(header);
		}
		header = strdup(attibute["header"].c_str());
		header_len = strlen(header);
	}
	std::string getHtml(KModel *model) {
		KVaryMark *m = (KVaryMark *)model;
		std::stringstream s;
		s << "header:<input name='header' value='";
		if (m && m->header) {
			s << m->header;
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
		if (header) {
			s << " header='" << header << "' ";
		}
		s << ">";
	}
private:
	char *header;
	int header_len;
};
#endif
