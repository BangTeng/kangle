#ifndef KMINOBJVERIFIEDMARK_H
#define KMINOBJVERIFIEDMARK_H
#include "time_utils.h"
class KMinObjVerifiedMark : public KMark
{
public:
	KMinObjVerifiedMark()
	{
		v = kgl_current_sec;
	};
	~KMinObjVerifiedMark()
	{
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{
		rq->min_obj_verified = v;
		return true;
	}
	KMark *newInstance()
	{
		return new KMinObjVerifiedMark;
	}
	const char *getName()
	{
		return "min_obj_verified";
	}
	std::string getHtml(KModel *model)
	{
		std::stringstream s;
		s << "min_obj_verified:<input name='v' value='";
		KMinObjVerifiedMark *m = (KMinObjVerifiedMark *)model;
		if (m) {
			s << (INT64)(m->v);
		}
		s << "'>";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << (int)v;
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		v = (time_t)(string2int(attribute["v"].c_str()));
		if (v > kgl_current_sec) {
			v = kgl_current_sec;
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " v='" << (INT64)v << "'>";
	}
private:
	time_t v;
};
#endif
