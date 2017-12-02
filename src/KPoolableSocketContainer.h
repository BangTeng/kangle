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
 * 连接池容器类
 */
class KPoolableSocketContainer: public KCountableEx {
public:
	KPoolableSocketContainer();
	virtual ~KPoolableSocketContainer();
	KUpstreamSelectable *getPoolSocket(KHttpRequest *rq);
	/*
	回收连接
	close,是否关闭
	lifeTime 连接时间
	*/
	virtual void gcSocket(KUpstreamSelectable *st,int lifeTime);
	void bind(KUpstreamSelectable *st);
	void unbind(KUpstreamSelectable *st);
	int getLifeTime() {
		return lifeTime;
	}
	/*
	 * 设置连接超时时间
	 */
	void setLifeTime(int lifeTime);
	/*
	 * 定期刷新删除过期连接
	 */
	virtual void refresh(time_t nowTime);
	/*
	 * 清除所有连接
	 */
	void clean();
	/*
	 * 得到连接数
	 */
	unsigned getSize() {
		lock.Lock();
		unsigned size = pools.size();	
		lock.Unlock();
		return size;
	}
	//isBad,isGood用于监控连接情况
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
	 * 把连接真正放入池中
	 */
	void putPoolSocket(KUpstreamSelectable *st);
	/*
		通知事件.
		ev = 0 关闭
		ev = 1 放入pool
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
