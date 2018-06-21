/*
 * KSelectable.h
 *
 *  Created on: 2010-5-5
 *      Author: keengo
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

#ifndef KSELECTABLE_H_
#define KSELECTABLE_H_
#include "forwin32.h"
#include "KSocket.h"
#include "global.h"
#ifdef _WIN32
#include "mswsock.h"
#define ENABLE_IOCP   1
#ifdef  ENABLE_IOCP
#define KSELECTOR_AIO           1
#endif
#endif
#include <list>
#include "log.h"
#include "KMutex.h"
#include "KSelector.h"
#include "ksapi.h"
#include "malloc_debug.h"
#ifdef ENABLE_ATOM
#include "katom.h"
#endif

#define STF_READ        1
#define STF_WRITE       (1<<1)
#define STF_RDHUP       (1<<2)


#define STF_REV         (1<<3)
#define STF_WEV         (1<<4)
#define STF_ET          (1<<5)
#define STF_ERR         (1<<6)

#define STF_RREADY      (1<<7)
#define STF_WREADY      (1<<8)

#define STF_SSL         (1<<9)
#define STF_FILE        (1<<10)
#define STF_RTIME_OUT   (1<<11)
#define STF_REVENT      STF_READ
#define STF_WEVENT      (STF_WRITE|STF_RDHUP)
#define STF_EVENT       (STF_REVENT|STF_WEVENT)

#define STF_RLOCK       STF_READ
#define STF_WLOCK       STF_WRITE
#define STF_LOCK        (STF_RLOCK|STF_WLOCK)

#define STF_RQ_OK       (1<<12)
#define STF_RQ_PER_IP   (1<<13)
#define STF_APP_HTTP2   (1<<14)
#define STF_NO_KA       (1<<15)

#define ST_ERR_TIME_OUT    -2

//#define RQ_LEAK_DEBUG               1
class KSelectable;
class KHttpRequest;
class KHttp2;
struct kgl_event
{
#ifdef _WIN32
	WSAOVERLAPPED lp;
#endif
	void *arg;
	resultEvent result;
	bufferEvent buffer;
};
class KSelectable {
public:
	KSocket *getSocket()
	{
		return sock;
	}
	SOCKET get_handle()
	{
		if (TEST(st_flags, STF_FILE)) {
			return (SOCKET)fd;
		}
		return sock->get_socket();
	}
	void clear_flag(int flag)
	{
		CLR(st_flags, flag);
	}
	void set_flag(int flag)
	{
		SET(st_flags,flag);
	}
	bool is_http2()
	{
		return TEST(st_flags, STF_APP_HTTP2)>0;
	}
	bool isSSL()
	{
		return TEST(st_flags,STF_SSL)>0;
	}
	void bind_socket(KSocket *sock)
	{
		this->sock = sock;
	}
	void bind_file_fd(FILE_HANDLE fd)
	{
		this->fd = fd;
		SET(st_flags, STF_FILE);
	}
	bool is_error()
	{
		return TEST(st_flags, STF_ERR)>0;
	}
	void shutdown_socket();
	uint16_t st_flags;
	uint8_t tmo_left;
	uint8_t tmo;
	union {
		KHttpRequest *rq;
		KHttp2 *http2;
	} app_data;
	kgl_list queue;
#ifdef RQ_LEAK_DEBUG
	kgl_list queue_edge;
#endif
	union {
		INT64 active_msec;
		int next_got;
	};
	KSelector *selector;
	kgl_event e[2];
	union {
		KSocket *sock;
		FILE_HANDLE fd;
	};
	friend class KHttp2;
	friend class KEpollSelector;
	friend class KIOCPSelector;
	friend class KKqueueSelector;
	friend class KPortSelector;
	friend class KAsyncFetchObject;
	friend class KSSLBIO;
	void async_read(void *arg,resultEvent result,bufferEvent buffer);
	void async_write(void *arg, resultEvent result, bufferEvent buffer);

protected:
	void eventRead(void *arg,resultEvent result,bufferEvent buffer);
	void eventWrite(void *arg,resultEvent result,bufferEvent buffer);
#ifdef ENABLE_KSSL_BIO
	void sslRead(void *arg, resultEvent result, bufferEvent buffer);
	void sslWrite(void *arg, resultEvent result, bufferEvent buffer);
	void lowEventRead(void *arg,resultEvent result,bufferEvent buffer);
	void lowEventWrite(void *arg,resultEvent result,bufferEvent buffer);
#endif
};
class KHttpRequest;
struct KBlockRequest
{
	KSelectable *st;
	void *arg;
	resultEvent func;
	INT64 active_msec;
	KHttpRequest *rq;
	KBlockRequest *next;
};
#endif /* KSELECTABLE_H_ */
