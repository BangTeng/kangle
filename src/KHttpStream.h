#ifndef KHTTPSTREAM_H
#define KHTTPSTREAM_H
#include "KStream.h"
/*
 支持串级的流
 */
class KHttpStream: public KWStream {
public:
	KHttpStream() {
		st = NULL;
	}
	KHttpStream(KWStream *st) {
		this->st = st;
		autoDelete = true;
	}
	KHttpStream(KWStream *st, bool autoDelete) {
		this->st = st;
		this->autoDelete = autoDelete;
	}
	void connect(KWStream *st, bool autoDelete) {
		if (this->st && this->autoDelete) {
			delete this->st;
		}
		this->st = st;
		this->autoDelete = autoDelete;
	}
	virtual ~KHttpStream() {
		if (autoDelete && st) {
			delete st;
		}
	}
	virtual StreamState flush() {
		if (st) {
			return st->flush();
		}
		return STREAM_WRITE_FAILED;
	}
	virtual StreamState write_all(const char *buf, int len) {
		if (st) {
			return st->write_all(buf, len);
		}
		return STREAM_WRITE_FAILED;
	}
	virtual StreamState write_end() {
		if (!preventWriteEnd && st) {
			return st->write_end();
		}
		return STREAM_WRITE_SUCCESS;
	}
protected:
	virtual int write(const char *buf, int len) {
		if (st) {
			return st->write(buf, len);
		}
		return -1;
	}
	KWStream *st;
	bool autoDelete;
};
#endif
