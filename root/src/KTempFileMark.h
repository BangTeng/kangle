#ifndef KTEMPFILEMARK_H
#define KTEMPFILEMARK_H
#include "KMark.h"
#ifdef ENABLE_TF_EXCHANGE
class KTempFileMark : public KMark
{
public:
	KTempFileMark()
	{
		tf = false;
	}
	bool mark(KHttpRequest *rq, KHttpObject *obj,
				const int chainJumpType, int &jumpType)
	{
		SET(rq->flags,RQ_TEMPFILE_HANDLED);
		if (tf) {
			if (rq->tf) {
				return true;
			}
			rq->tf = new KTempFile;
		} else {
			if (rq->tf) {
				delete rq->tf;
				rq->tf = NULL;
			}
		}
		return true;
	}
	KMark *newInstance()
	{
		return new KTempFileMark;
	}
	const char *getName()
	{
		return "temp_file";
	}
	std::string getHtml(KModel *model)
	{
		KTempFileMark *m = (KTempFileMark *)model;
		std::stringstream s;
		s << "<input type='radio' name='tf' value='1' ";
		if (m && m->tf) {
			s << "checked";
		}
		s << ">on";
		s << "<input type='radio' name='tf' value='0' ";
		if (m && !m->tf) {
			s << "checked";
		}
		s << ">off";
		return s.str();
	}
	std::string getDisplay()
	{
		std::stringstream s;
		s << (tf?"on":"off");
		return s.str();
	}
	void editHtml(std::map<std::string, std::string> &attribute) throw (KHtmlSupportException)
	{
		if (attribute["tf"]=="1") {
			tf = true;
		} else {
			tf = false;
		}
	}
	void buildXML(std::stringstream &s)
	{
		s << " tf='" << (tf?1:0) << "'>";
	}
private:
	bool tf;
};
#endif
#endif
