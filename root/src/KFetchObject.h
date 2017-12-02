/*
 * KFetchObject.h
 *
 *  Created on: 2010-4-19
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

#ifndef KFETCHOBJECT_H_
#define KFETCHOBJECT_H_
#include <time.h>
#include "KHttpRequest.h"
#include "KBuffer.h"
#include "KPathRedirect.h"
#define READ_SWITCH_FUNCTION	-3
#define READ_PROTOCOL_ERROR		-2
#define SEND_HEAD_FAILED		0
#define SEND_HEAD_SUCCESS		1
#define SEND_HEAD_PULL_MODEL	2
//#define SEND_HEAD_ASYNC_MODEL   3
#define SEND_HEAD_SYNC_MODEL    4
#define SEND_HEAD_ERROR         5
#ifdef ENABLE_REQUEST_QUEUE
class KRequestQueue;
#endif
class KHttpObject;
class KHttpRequest;
struct buff;
class KContext;
class KUpstreamSelectable;
/**
数据源类(静态数据，动态数据，除缓存的数据外，所有请求的数据均来源于数据源)
所有数据源的基类
*/
class KFetchObject {
public:
	KFetchObject()
	{
		brd = NULL;
		closed = true;
	}
	virtual ~KFetchObject()
	{
		if (brd) {
			brd->release();
		}
	}
	/*
	 * 打开数据源,并处理请求。
	 */
	virtual void open(KHttpRequest *rq);	
	//处理数据源读body请求。
	virtual void readBody(KHttpRequest *rq)
	{
	}
#ifdef WORK_MODEL_TCP
	virtual bool isPipeLine()
	{
		return false;
	}
#endif
	//通知读完了body
	virtual void readBodyEnd(KHttpRequest *rq)
	{
	}
	/*
	 *是否是同步调用
	*/
	virtual bool isSync()
	{
		return false;
	}
	virtual KUpstreamSelectable *getSelectable()
	{
		return NULL;
	}
	//关闭数据源
	virtual void close(KHttpRequest *rq)
	{
		this->closed = true;
	}
	bool isClosed()
	{
		return closed;
	}
	void bindBaseRedirect(KBaseRedirect *brd)
	{
		kassert(this->brd==NULL);
		this->brd = brd;
		if (brd) {
			brd->addRef();
		}
	}
	KBaseRedirect *getBaseRedirect()
	{
		return brd;
	}
	virtual KFetchObject *clone(KHttpRequest *rq);
	virtual bool needTempFile()
	{
		return true;
	}
#ifdef ENABLE_REQUEST_QUEUE
	//此数据源是否需要队列功能。对于本地数据不用该功能。
	//对于上游数据和动态的则返回true
	virtual bool needQueue()
	{
		return false;
	}
#endif
protected:
	void setClosed(bool closed)
	{
		this->closed = closed;
	}
	KBaseRedirect *brd;
	/*
	处理已经从upstream读到的数据，返回true,继续读取，false则不继续读，表示已经有数据可以发到rq
	*/
	bool pushHttpBody(KHttpRequest *rq,char *buf,int len);
	bool closed;
};

#endif /* KFETCHOBJECT_H_ */
