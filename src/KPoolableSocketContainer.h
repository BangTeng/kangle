/*
 * KPoolableStreamContainer.h
 *
 *  Created on: 2010-8-18
 *      Author: keengo
 */

#ifndef KPOOLABLESTREAMCONTAINER_H_
#define KPOOLABLESTREAMCONTAINER_H_
#include <list>
#include "global.h"
#include "KUpstreamSelectable.h"
#include "KString.h"
#include "KMutex.h"
#include "KCountable.h"
#include "time_utils.h"
#include "KHttpEnv.h"
/*
 * ���ӳ�������
 */
class KPoolableSocketContainerImp {
public:
	KPoolableSocketContainerImp();
	~KPoolableSocketContainerImp();
	void refresh(bool clean);
	void refreshList(kgl_list *l, bool clean);
	kgl_list *getPopList(KConnectionSelectable *st);
	kgl_list *getPushList(KUpstreamSelectable *st);
	unsigned size;
protected:
	kgl_list **l;
};
class KPoolableSocketContainer: public KCountableEx {
public:
	KPoolableSocketContainer();
	virtual ~KPoolableSocketContainer();
	KUpstreamSelectable *getPoolSocket(KHttpRequest *rq);
	/*
	��������
	close,�Ƿ�ر�
	lifeTime ����ʱ��
	*/
	virtual void gcSocket(KUpstreamSelectable *st,int lifeTime);
	void bind(KUpstreamSelectable *st);
	void unbind(KUpstreamSelectable *st);
	int getLifeTime() {
		return lifeTime;
	}
	/*
	 * �������ӳ�ʱʱ��
	 */
	void setLifeTime(int lifeTime);
	/*
	 * ����ˢ��ɾ����������
	 */
	virtual void refresh(time_t nowTime);
	/*
	 * �����������
	 */
	void clean();
	/*
	 * �õ�������
	 */
	unsigned getSize() {
		unsigned size = 0;
		lock.Lock();
		if (imp) {
			size = imp->size;
		}
		lock.Unlock();
		return size;
	}
	//isBad,isGood���ڼ���������
	virtual void isBad(KUpstreamSelectable *st,BadStage stage)
	{
	}
	virtual void isGood(KUpstreamSelectable *st)
	{
	}

#ifdef HTTP_PROXY
	virtual void addHeader(KHttpRequest *rq,KHttpEnv *s)
	{
	}
#endif
protected:
	/*
	 * �����������������
	 */
	void putPoolSocket(KUpstreamSelectable *st);
	/*
		֪ͨ�¼�.
		ev = 0 �ر�
		ev = 1 ����pool
	*/

	KUpstreamSelectable *internalGetPoolSocket(KHttpRequest *rq);
	int lifeTime;	
	KMutex lock;
private:
	time_t getHttp2ExpireTime()
	{
		int lifeTime = this->lifeTime;
		if (lifeTime <= 10) {
			//http2����10������ʱ��
			lifeTime = 10;
		}
		return kgl_current_sec + lifeTime;
	}
	KPoolableSocketContainerImp *imp;
};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
