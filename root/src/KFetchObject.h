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
����Դ��(��̬���ݣ���̬���ݣ�������������⣬������������ݾ���Դ������Դ)
��������Դ�Ļ���
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
	 * ������Դ,����������
	 */
	virtual void open(KHttpRequest *rq);	
	//��������Դ��body����
	virtual void readBody(KHttpRequest *rq)
	{
	}
#ifdef WORK_MODEL_TCP
	virtual bool isPipeLine()
	{
		return false;
	}
#endif
	//֪ͨ������body
	virtual void readBodyEnd(KHttpRequest *rq)
	{
	}
	/*
	 *�Ƿ���ͬ������
	*/
	virtual bool isSync()
	{
		return false;
	}
	virtual KUpstreamSelectable *getSelectable()
	{
		return NULL;
	}
	//�ر�����Դ
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
	//������Դ�Ƿ���Ҫ���й��ܡ����ڱ������ݲ��øù��ܡ�
	//�����������ݺͶ�̬���򷵻�true
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
	�����Ѿ���upstream���������ݣ�����true,������ȡ��false�򲻼���������ʾ�Ѿ������ݿ��Է���rq
	*/
	bool pushHttpBody(KHttpRequest *rq,char *buf,int len);
	bool closed;
};

#endif /* KFETCHOBJECT_H_ */
