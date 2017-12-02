#ifndef KFOOTERFILTER_H
#define KFOOTERFILTER_H
#include "KHttpStream.h"

class KFooterFilter : public KHttpStream
{
public:
	StreamState write_all(const char *buf, int len) 
	{
		if (replace) {
			return STREAM_WRITE_SUCCESS;
		}
		if (head && !added && footer.size()>0) {
			added = true;
			if(KHttpStream::write_all(footer.c_str(),footer.size())==STREAM_WRITE_FAILED) {
				return STREAM_WRITE_FAILED;
			}
		}
		return KHttpStream::write_all(buf,len);
	}
	StreamState write_end() {
		if (footer.size()>0 && (!head||replace)) {
			if (KHttpStream::write_all(footer.c_str(),footer.size())==STREAM_WRITE_FAILED) {
				return STREAM_WRITE_FAILED;
			}
		}
		return KHttpStream::write_end();
	}
	std::string footer;
	bool head;
	bool added;
	bool replace;
private:
};
#endif
