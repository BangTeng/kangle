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
//事件
#define STF_READ        0x1
#define STF_WRITE       0x2
#define STF_RDHUP       0x4 //3
#define STF_EV          0x8 //4
#define STF_ALWAYS_READ 0x10//5
#define STF_ONE_SHOT    0x20//6
//类型
#define STF_SSL         0x40//7
#define STF_FILE        0x80//8
#define STF_CLOSED      0x100//9
//锁
#ifdef _WIN32
#define STF_RLOCK       STF_READ
#define STF_WLOCK       STF_WRITE
#else
#define STF_RLOCK       0x200//10
#define STF_WLOCK       0x400//11
#endif
#define STF_LOCK        (STF_RLOCK|STF_WLOCK)
#define STF_REVENT      (STF_READ|STF_RLOCK)
#define STF_WEVENT      (STF_WRITE|STF_RDHUP|STF_WLOCK)
#define STF_EVENT       (STF_REVENT|STF_WEVENT)

//应用层
#define STF_RQ_OK       0x800//12
#define STF_RQ_PER_IP   0x1000//13
#define STF_APP_HTTP2   0x2000//14
#define STF_NO_KA       0x4000//15



struct kgl_event
{
#ifdef _WIN32
	WSAOVERLAPPED lp;
#endif
	void *arg;
	bufferEvent buffer;
	resultEvent result;
};
class KHttpRequest;
class KHttp2;
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
	bool isClosed()
	{
		return TEST(st_flags,STF_CLOSED)>0;
	}
	bool isSSL()
	{
		return TEST(st_flags,STF_SSL)>0;
	}
	bool is_evset()
	{
		return TEST(st_flags,STF_EV)>0;
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
	uint16_t st_flags;
	u_char tmo_left;
	u_char tmo;
	union {
		KHttpRequest *rq;
		KHttp2 *http2;
	} app_data;
	kgl_list queue;
	INT64 active_msec;
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
	bool asyncRead(void *arg,resultEvent result,bufferEvent buffer,int list = KGL_LIST_RW);
	void asyncWrite(void *arg, resultEvent result, bufferEvent buffer);

protected:
	void eventRead(void *arg,resultEvent result,bufferEvent buffer);
	void eventWrite(void *arg,resultEvent result,bufferEvent buffer);
#ifdef ENABLE_KSSL_BIO
	bool sslRead(void *arg, resultEvent result, bufferEvent buffer);
	void sslWrite(void *arg, resultEvent result, bufferEvent buffer);
	void lowEventRead(void *arg,resultEvent result,bufferEvent buffer);
	void lowEventWrite(void *arg,resultEvent result,bufferEvent buffer);
#endif
};
class KHttpRequest;
struct KBlockRequest
{
	KSelectable *rq;
	void *arg;
	int op;
	timer_func func;
	INT64 active_msec;
	KBlockRequest *next;
	KBlockRequest *prev;
};
#endif /* KSELECTABLE_H_ */
