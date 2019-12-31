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
#include "kselector.h"

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
class KContext;
class KUpstream;
/**
����Դ��(��̬���ݣ���̬���ݣ�������������⣬������������ݾ���Դ������Դ)
��������Դ�Ļ���
*/
class KFetchObject {
public:
	KFetchObject()
	{
		brd = NULL;
		flags = 0;
		next = NULL;
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
	virtual kev_result open(KHttpRequest *rq);
	//��������Դ��body����
	virtual kev_result readBody(KHttpRequest *rq)
	{
		kassert(false);
		return kev_err;
	}
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
	virtual KUpstream *GetUpstream()
	{
		return NULL;
	}
	//�ر�����Դ
	virtual void close(KHttpRequest *rq)
	{
		this->closed = 1;
	}
	bool isClosed()
	{
		return closed;
	}
	void bindRedirect(KRedirect *rd,uint8_t confirmFile)
	{
		kassert(this->brd == NULL);
		this->brd = new KBaseRedirect(rd, confirmFile);
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
	virtual bool NeedTempFile(bool upload, KHttpRequest *rq);
	bool IsChunkPost()
	{
		return chunk_post;
	}
	union {
		struct {
			uint32_t closed : 1;
			uint32_t chunk_post : 1;
			uint32_t filter : 1;
		};
		uint32_t flags;
	};
#ifdef ENABLE_REQUEST_QUEUE
	//������Դ�Ƿ���Ҫ���й��ܡ����ڱ������ݲ��øù��ܡ�
	//�����������ݺͶ�̬���򷵻�true
	virtual bool needQueue()
	{
		return false;
	}
#endif
	StreamState PushBody(KHttpRequest *rq, const char *buf, int len);
	KFetchObject *next;
protected:
	KBaseRedirect *brd;
	/*
	�����Ѿ���upstream���������ݣ�����true,������ȡ��false�򲻼���������ʾ�Ѿ������ݿ��Է���rq
	*/
	kev_result pushHttpBody(KHttpRequest *rq,const char *buf,int len);
	
};

#endif /* KFETCHOBJECT_H_ */
