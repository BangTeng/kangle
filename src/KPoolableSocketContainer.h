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
		unsigned size = 0;
		lock.Lock();
		if (imp) {
			size = imp->size;
		}
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

#ifdef HTTP_PROXY
	virtual void addHeader(KHttpRequest *rq,KHttpEnv *s)
	{
	}
#endif
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

	KUpstreamSelectable *internalGetPoolSocket(KHttpRequest *rq);
	int lifeTime;	
	KMutex lock;
private:
	time_t getHttp2ExpireTime()
	{
		int lifeTime = this->lifeTime;
		if (lifeTime <= 10) {
			//http2最少10秒连接时间
			lifeTime = 10;
		}
		return kgl_current_sec + lifeTime;
	}
	KPoolableSocketContainerImp *imp;
};
#endif /* KPOOLABLESTREAMCONTAINER_H_ */
