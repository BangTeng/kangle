#include "KCache.h"
KCache cache;
void init_cache() {
	cache.init();
}
KCache::KCache()
{
	count = 0;
	clean_blocked = false;
	disk_shutdown = true;
}
void KCache::init(bool firstTime)
{
	if (firstTime) {
		for(u_short i=0;i<HASH_SIZE;i++){
			objHash[i].id = i;
		}
		for(unsigned char i=0;i<2;i++){
			objList[i].list_state = i;
		}
	}
#ifdef ENABLE_DISK_CACHE
	if (conf.disk_cache>0) {
		init_disk_cache(firstTime);
	}
#endif
}
void handle_purge_object(KHttpObject *obj,void *param)
{
	//char *url = obj->url->getUrl();
	//klog(KLOG_NOTICE,"purge obj [%s]\n",url);
	//free(url);
	SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
}
void handle_cache_info(KHttpObject *obj,void *param)
{
	KCacheInfo *ci = (KCacheInfo *)param;
	obj->count_size(ci->mem_size,ci->disk_size);	
}
