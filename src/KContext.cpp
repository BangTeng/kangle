#include "forwin32.h"
#include "KHttpTransfer.h"
#include "KCache.h"
#include "KContext.h"
#include <assert.h>
void KContext::clean()
{
	if (st) {
		delete st;
		st = NULL;
	}
	clean_if_none_match();
	memset(this,0,sizeof(KContext));
}


void KContext::store_obj(KHttpRequest *rq)
{
	if (haveStored) {
		return ;
	}
	haveStored = true;
	//printf("old_obj = %p\n",old_obj);
	if (old_obj) {
		//send from cache
		assert(obj);
		CLR(rq->filter_flags,RQ_SWAP_OLD_OBJ);
		if (obj->data->status_code == STATUS_NOT_MODIFIED) {
			//����obj
			//ɾ����obj
			assert(old_obj->in_cache);
			cache.rate(old_obj);
			return;
		}
		if (obj->in_cache == 0) {
			if (stored_obj(rq, obj, old_obj)) {
				cache.dead(old_obj, __FILE__, __LINE__);
				return;
			}
		}
		if (TEST(rq->filter_flags, RF_ALWAYS_ONLINE) ||
			(TEST(old_obj->index.flags, OBJ_IS_GUEST) && !TEST(rq->filter_flags, RF_GUEST))
			) {
			//�������ߣ���������ҳû�д洢
			//�����ǻ�Ա�������οͻ���
			//����ҳ����ʹ��
			cache.rate(old_obj);
		} else {
			cache.dead(old_obj,__FILE__,__LINE__);
		}
		return;
	}
	if (obj == NULL) {
		return;
	}
	//check can store
	if (obj->in_cache==0) {
		stored_obj(rq, obj,old_obj);
	} else {
		assert(obj->in_cache==1);
		cache.rate(obj);
	}
}
void KContext::clean_obj(KHttpRequest *rq,bool store_flag)
{
	if (store_flag) {
		store_obj(rq);
	}
	if (old_obj) {
		old_obj->release();
		old_obj = NULL;
	}
	if (obj) {
		obj->release();
		obj = NULL;
	}
	haveStored = false;
}
void KContext::pushObj(KHttpObject *obj)
{
	assert(old_obj==NULL);
	if (this->obj==NULL) {
		this->obj = obj;
		return;
	}
	this->old_obj = this->obj;
	this->obj = obj;
}
void KContext::popObj()
{
	if (old_obj) {
		assert(obj);
		obj->release();
		obj = old_obj;
		old_obj = NULL;
	}
}

