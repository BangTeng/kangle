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
#define STF_READ       0x1
#define STF_WRITE      0x2
#define STF_CLOSED     0x4
#define STF_SSL        0x8
#define STF_ET         0x10
#define STF_EV         0x20
//#define STF_ONE_SHOT   0x40
#define STF_RQ_OK      0x80
#define STF_RQ_PER_IP  0x100
#define STF_APP_HTTP2  0x200
#define STF_MANAGE     0x800
#define STF_ERR        0x1000
#define STF_NO_KA      0x2000
#define STF_RDHUP      0x4000
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
		return fd;
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
	bool is_read_hup()
	{
		return TEST(st_flags,STF_RDHUP)>0;
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
	friend class KHttp2;
	friend class KEpollSelector;
	friend class KIOCPSelector;
	friend class KKqueueSelector;
	friend class KPortSelector;
	friend class KAsyncFetchObject;
	friend class KSSLBIO;
	bool asyncRead(void *arg,resultEvent result,bufferEvent buffer,int list = KGL_LIST_RW);
	void asyncWrite(void *arg, resultEvent result, bufferEvent buffer);
	KSocket *fd;
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
