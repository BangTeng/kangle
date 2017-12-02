#ifndef KCACHE_H_22
#define KCACHE_H_22
#include "global.h"
#include "KMutex.h"
#include "KHttpObject.h"
#include "KObjectList.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#endif
#include "KHttpObjectHash.h"

void handle_purge_object(KHttpObject *obj,void *param);
void handle_cache_info(KHttpObject *obj,void *param);
struct KCacheInfo
{
	INT64 mem_size;
	INT64 disk_size;
	INT64 have_length;
	INT64 content_length;
};
/**
* 缓存接口。采用LRU淘汰算法，由内存和磁盘两级缓存组成
*/
class KCache
{
public:
	KCache();
	//初始化缓存系统
	void init(bool firstTime = false);
	//更新obj
	void rate(KHttpObject *obj)
	{
		if (obj->in_cache == 0) {
			return;
		}
		lock();
		assert(obj->list_state != LIST_IN_NONE);
		objList[obj->list_state].remove(obj);
		if (TEST(obj->index.flags, FLAG_IN_MEM)) {
			objList[LIST_IN_MEM].add(obj);
		}
		else {
			objList[LIST_IN_DISK].add(obj);
		}
		unlock();
#ifdef ENABLE_DB_DISK_INDEX
		if (TEST(obj->index.flags, FLAG_IN_DISK) && dci) {
			dci->start(ci_updateLast, obj);
		}
#endif
	}
	//清除obj
	void dead(KHttpObject *obj)
	{
		if (obj->in_cache == 0) {
			return;
		}
		cacheLock.Lock();
		assert(obj->list_state != LIST_IN_NONE);
		SET(obj->index.flags, FLAG_DEAD | OBJ_INDEX_UPDATE);
		objList[obj->list_state].dead(obj);
		cacheLock.Unlock();
	}
	//查找obj
	KHttpObject * find(KHttpRequest *rq, u_short url_hash)
	{
		return objHash[url_hash].get(rq->url,
			(TEST(rq->flags, RQ_HAS_GZIP) != 0),
			TEST(rq->workModel, WORK_MODEL_INTERNAL) > 0,
			TEST(rq->filter_flags, RF_NO_DISK_CACHE) > 0,
			rq->min_obj_verified);
	}
	//迭代所有的obj，由handler处理。
	void iterator(objHandler handler, void *param);
	//增加obj到缓存中，list_state指定是加到哪一级缓存
	bool add(KHttpObject *obj, int list_state)
	{
		if (obj->h == HASH_SIZE) {
			obj->h = hash_url(obj->url);
		}
		assert(obj->in_cache == 0);
		obj->in_cache = 1;
		cacheLock.Lock();
		if (!objHash[obj->h].put(obj)) {
			cacheLock.Unlock();
			obj->in_cache = 0;
			return false;
		}
		objList[list_state].add(obj);
		count++;
		cacheLock.Unlock();
		return true;
	}
	int save_disk_index(KFile *fp)
	{
		int save_count = 0;
		lock();
		for (int i = 1; i >= 0; i--) {
			int this_save_count = objList[i].save_disk_index(fp);
			if (this_save_count == -1) {
				break;
			}
			save_count += this_save_count;
		}
		unlock();
		return save_count;
	}
	void flush_mem_to_disk()
	{
		objList[LIST_IN_MEM].move(1, true);
	}
	bool check_count_limit(INT64 maxMemSize)
	{
		int max_count = int(maxMemSize / 1024);
		cacheLock.Lock();
		int cache_count = count;
		if (cache_count <= max_count) {
			cacheLock.Unlock();
			return false;
		}
		int kill_count = cache_count - max_count;
		objList[LIST_IN_DISK].dead_count(kill_count);
		objList[LIST_IN_MEM].dead_count(kill_count);
		cacheLock.Unlock();
		return true;
	}
	//刷新缓存
	void flush(INT64 maxMemSize,INT64 maxDiskSize,bool disk_is_radio)
	{
		INT64 total_size = 0;
		INT64 kill_size;
		INT64 total_disk_size = 0;
		bool check_count_result = check_count_limit(maxMemSize);
		getSize(total_size, total_disk_size);
		kill_size = total_size - maxMemSize;
		objList[LIST_IN_MEM].move(kill_size, true);
#ifdef ENABLE_DISK_CACHE
		if (check_count_result || conf.diskWorkTime.checkTime(time(NULL))) {
			if (disk_is_radio) {
				kill_size = get_need_free_disk_size((int)(maxDiskSize));
			} else {
				kill_size = total_disk_size - maxDiskSize;
			}
			objList[LIST_IN_DISK].move(kill_size, false);
		}
#endif
		return;
	}
	//得到缓存大小
	void getSize(INT64 &memSize,INT64 &diskSize)
	{
		int i;
		for (i = 0; i < HASH_SIZE; i++) {
			objHash[i].getSize(memSize, diskSize);
		}
	}
#ifdef MALLOCDEBUG
	void freeAllObject()
	{
		for (int i=0;i<2;i++) {
			objList[i].free_all_cache();
		}
	}
#endif
	int getCount()
	{
		return count;
	}
	int clean_cache(KReg *reg,int flag)
	{
		int result = 0;
		cacheLock.Lock();
		for (int i=0;i<2;i++) {
			result += objList[i].clean_cache(reg,flag);
		}
		cacheLock.Unlock();
		return result;
	}
	int clean_cache(KUrl *url,bool wide)
	{
		int count = 0;
		for (int i=0;i<HASH_SIZE;i++) {
			count += objHash[i].purge(url,wide,handle_purge_object,NULL);
		}
		return count;
	}
	int get_cache_info(KUrl *url,bool wide,KCacheInfo *ci)
	{
		int count = 0;
		for (int i=0;i<HASH_SIZE;i++) {
			count += objHash[i].purge(url,wide,handle_cache_info,ci);
		}
		return count;
	}
	void dead_all_obj()
	{
		cacheLock.Lock();
		for(int i=0;i<2;i++){
			objList[i].dead_all_obj();
		}
		cacheLock.Unlock();
	}
	void syncDisk()
	{
		
		cacheLock.Lock();
#ifdef ENABLE_DB_DISK_INDEX
		//清除死物件
		for (int i=0;i<2;i++) {
			KHttpObject *obj = objList[i].getHead();
			while (obj) {
				if (TEST(obj->index.flags,OBJ_INDEX_UPDATE|FLAG_IN_DISK) == (OBJ_INDEX_UPDATE|FLAG_IN_DISK)) {
					CLR(obj->index.flags,OBJ_INDEX_UPDATE);
					if (dci) {
						dci->start(ci_update,obj);
					}
				}
				obj = obj->lnext;
			}
		}
#endif
		cacheLock.Unlock();
	}
	KHttpObjectHash *getHash(u_short h)
	{
		return &objHash[h];
	}
	u_short hash_url(KUrl *url) {
#ifdef MULTI_HASH
        u_short res = 0;
        res = string_hash(url->host,1);
        res = string_hash(url->path, res) & HASH_MASK;
        return res;
#else
        return 0;
#endif
}
	friend class KObjectList;
private:
	void lock()
	{
		cacheLock.Lock();
	}
	void unlock()
	{
		cacheLock.Unlock();
	}
	int count;
	KMutex cacheLock;
	KObjectList objList[2];
	KHttpObjectHash objHash[HASH_SIZE];
};
extern KCache cache;
#endif
