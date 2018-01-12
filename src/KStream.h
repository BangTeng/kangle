/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef KSTREAM_H_
#define KSTREAM_H_
#include <time.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include "global.h"
#ifdef _WIN32
#include "forwin32.h"
#define INT64_FORMAT     "%I64d"
#define INT64_FORMAT_HEX "%I64x"
#else
#define INT64_FORMAT     "%lld"
#define INT64_FORMAT_HEX "%llx"
#endif
#define KBuffedWStream KWStream
#define WSTR(x) write_all(x,sizeof(x)-1)

#define INT2STRING_LEN	32
inline INT64 string2int(const char *buf) {
#ifdef _WIN32
	return _atoi64(buf);
#else
	return atoll(buf);
#endif
}
inline const char * int2string(INT64 value, char *buf,bool hex=false) {
	const char *formatString = INT64_FORMAT;
	if (hex) {
		formatString = INT64_FORMAT_HEX;
	}
	int size = snprintf(buf, INT2STRING_LEN - 1, formatString, value);
	buf[size] = '\0';
	return buf;
}


inline char *upstrdup(const char *val)
{
	char *buf = strdup(val);
	char *v = buf;
	while(*v){
		*v = toupper(*v);
		v++;
	}
	return buf;
}
enum StreamState {
	STREAM_WRITE_FAILED, STREAM_WRITE_SUCCESS, STREAM_WRITE_END
};
class KRStream {
public:
	virtual ~KRStream() {
	}
	virtual int read(char *buf, int len) = 0;
	bool read_all(char *buf, int len);
	char *readLine();
};
class KWStream {
public:
	virtual ~KWStream() {

	}
	KWStream() {
		preventWriteEnd = false;
	}
	virtual bool support_sendfile()
	{
		return false;
	}
	virtual StreamState write_all(const char *buf, int len);
	virtual StreamState flush() {
		return STREAM_WRITE_SUCCESS;
	}
	virtual StreamState write_end() {
		return flush();
	}
	/*
	 直接写，buf由被调用者自行处理。
	 注意:在串流的情况下调用这个函数必须要显式调用，即st->write_direct
	 不能使用KUpStream::write_direct，否则有可能死循环。
	 */
	virtual StreamState write_direct(char *buf, int len);
	StreamState write_all(const char *buf);
	inline KWStream & operator <<(const char *str)
	{
		if (!write_all(str, (int)strlen(str))) {
			//fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
	inline bool add(const int c, const char *fmt) {
		char buf[16];
		int len = snprintf(buf, sizeof(buf) - 1, fmt, c);
		if (len > 0) {
			return write_all(buf, len) == STREAM_WRITE_SUCCESS;
		}
		return false;
	}
	inline bool add(const INT64 c,const char *fmt) {
		char buf[INT2STRING_LEN];
		int len = snprintf(buf, sizeof(buf) - 1, fmt, c);
		if (len > 0) {
			return write_all(buf, len) == STREAM_WRITE_SUCCESS;
		}
		return false;
	}
	inline KWStream & operator <<(const std::string str) {
		write_all(str.c_str(), (int)str.size());
		return *this;
	}
	inline KWStream & operator <<(const char c) {
		write_all(&c, 1);
		return *this;
	}
	inline KWStream & operator <<(const int value) {
		char buf[16];
		int len = snprintf(buf, 15, "%d", value);
		if (len <= 0) {
			return *this;
		}
		if (!write_all(buf, len)) {
			fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
	inline KWStream & operator <<(const INT64 value) {
		char buf[INT2STRING_LEN];
		int2string(value,buf,false);
		if (!write_all(buf, (int)strlen(buf))) {
			fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
	inline void addHex(const int value)
	{
		char buf[16];
		int len = snprintf(buf,15,"%x",value);
		write_all(buf,len);
	}
	inline KWStream & operator <<(const unsigned value) {
		char buf[16];
		const char *fmt = "%u";
		//if (sfmt != stream::def) {
		//	fmt = stream::getFormat(sfmt);
		//}
		int len = snprintf(buf, 15, fmt, value);
		if (len <= 0) {
			return *this;
		}
		if (!write_all(buf, len)) {
			fprintf(stderr, "cann't write to stream\n");
		}
		return *this;
	}
	friend class KHttpStream;
	bool preventWriteEnd;
protected:
	virtual int write(const char *buf, int len) {
		return -1;
	}
};
class KConsole: public KWStream {
public:
	int write(const char *buf, int len);
	static KConsole out;
};
class KStream: public KRStream, public KWStream {
};

#endif /* KSTREAM_H_ */
