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
		lock.Lock();
		unsigned size = pools.size();	
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
	virtual void addHeader(KHttpEnv *s)
	{
	}
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
	//virtual void noticeEvent(int ev,KPoolableSocket *st)
	//{

	//}

	KUpstreamSelectable *internalGetPoolSocket(KHttpRequest *rq);
	int lifeTime;
	std::list<KUpstreamSelectable *> pools;
	KMutex lock;
private:
	void refreshPool(std::list<KUpstreamSelectable *> *pools)
	{
		std::list<KUpstreamSelectable *>::iterator it2;
		for (it2 = pools->end(); it2 != pools->begin();) {
			it2--;
			if ((*it2)->expireTime <= kgl_current_msec) {
				assert((*it2)->container == NULL);
				(*it2)->destroy();
				it2 = pools->erase(it2);
			} else {
				break;
			}
		}
	}

};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
