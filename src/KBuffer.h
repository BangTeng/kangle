/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KBUFFER_H
#define KBUFFER_H
#include "malloc_debug.h"
#include "KSocket.h"
#include "KSendable.h"
#include "KStream.h"
#include "forwin32.h"
#define 	CHUNK		NBUFF_SIZE

struct buff {
	char *data;
	buff *next;
	int used;
	union {
		struct {
			int skip_data_free : 1;
		};
		int flags;
	};
};
inline buff * new_buff(char *data,int len)
{
	buff *b = (buff *)xmalloc(sizeof(buff));
	b->used = len;
	b->flags = 0;
	b->data = data;
	return b;
}
inline buff * new_buff(int len)
{
	buff *b = (buff *)xmalloc(sizeof(buff));
	b->used = len;
	b->flags = 0;
	b->data = (char *)xmalloc(len);
	return b;
}
inline void free_buff(buff *buf)
{
	if (buf->data) {
		xfree(buf->data);
	}
	xfree(buf);
}
template<typename T>
StreamState send_buff(T *socket, buff *buf) {
	StreamState result = STREAM_WRITE_FAILED;
	while (buf) {
		if (buf->used > 0) {
			result = socket->write_all(buf->data, buf->used);
			if (result == STREAM_WRITE_FAILED) {
				return result;
			}
		}
		buf = buf->next;
	}
	return result;
}
class KBuffer : public KWStream{
public:
	KBuffer(int chunkSize);
	KBuffer();
	StreamState write_all(const char *buf, int len);
	void add(const char *buf);
	StreamState write_direct(char *buf, int len);
	void end(char *buf, int len, int addBytes = 0);
	buff *getAllBuf();
	/*
	 * Both of stealBuffFast and stealBuff steal the buff data.
	 * they are different.
	 * stealBuff re-alloc the last block data not to waste any memory.
	 * It is suitable for long time stored object.
	 *
	 * stealBuffFast only link the last block data. It may waste some pre-alloced memory.
	 * It is suitable for memory operate.
	 */
	buff *stealBuffFast();
	//see stealBuffFast
	buff *stealBuff();
	StreamState send(KClientSocket *socket);
	/*
	 * send all data to socket
	 */
	StreamState send(KSendable *socket);
	/*
	* 发送部分数据
	*/
	StreamState send(KSendable *socket,INT64 start,INT64 len);
	unsigned getLen();
	~KBuffer();
	void clean();
	static void destroy(buff *buf);
	inline KBuffer & operator <<(const char *str) {
		write_all(str, (int)strlen(str));
		return *this;
	}
	inline KBuffer & operator <<(const int value) {
		char buf[10];
		int len = snprintf(buf, 10, "%d", value);
		write_all(buf, len);
		return *this;
	}
	bool setChunkSize(int chunkSize) {
		if (hotData) {
			return false;
		}
		assert(chunkSize>0);
		this->chunkSize = chunkSize;
		return true;
	}
	void swap(KBuffer *a) {
		KBuffer tmp;
		tmp = *a;
		*a = *this;
		*this = tmp;
	}
	KBuffer & operator =(KBuffer &a) {
		assert(hot_buf==NULL && hotData==NULL && out_buf==NULL);
		hot_buf = a.hot_buf;
		out_buf = a.out_buf;
		hotData = a.hotData;
		used = a.used;
		totalLen = a.totalLen;
		chunkSize = a.chunkSize;
		a.hotData = NULL;
		a.out_buf = NULL;
		a.totalLen = 0;
		a.used = 0;
		a.hot_buf = NULL;
		return *this;
	}
	void dump(FILE *fp);
	friend class KDeChunked;
private:
	//DEPRECATED
	//	buff *getHotBuf();
	char *getHotData(unsigned &len) {
		len = used;
		return hotData;
	}
	/*
	 * DEPRECATED
	 * Warning: use getBuf not return all data.
	 * the last tail data is stored in hotData.
	 * use getHotData to get the tail data.
	 */
	buff *getBuf() {
		return out_buf;
	}
	void init(int chunkSize);
	void internelAdd(char *buf, int len);
	void internelEnd(char *buf, int len, int addBytes = 0);
	buff *hot_buf;
	buff *out_buf;
	char *hotData;
	unsigned used;
	unsigned chunkSize;
	unsigned totalLen;
};

#endif

