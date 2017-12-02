#include <vector>
#include "lib.h"
#include "KHttpObjectHash.h"
#include "KObjectList.h"
#include "cache.h"
#include "KReg.h"
#ifdef ENABLE_DB_DISK_INDEX
#include "KDiskCacheIndex.h"
#endif
#include "malloc_debug.h"
#include "KCache.h"
#define CLEAN_CACHE		0
#define DROP_DEAD		1
#define MAX_LOCK_MOVE_SIZE 1
struct KTempHttpObject
{
	KHttpObject *obj;
	INT64 decSize;
	bool is_dead;
	KTempHttpObject *next;
};
KObjectList::KObjectList()
{
	l_head = l_end = NULL;
	this->list_state = 0;
	cache_model = CLEAN_CACHE;
}
void KObjectList::add(KHttpObject *m_list)
{	 
	assert(m_list->list_state == LIST_IN_NONE);
	m_list->list_state = list_state;
	if (l_end == NULL) {
		l_end = m_list;
		l_head = m_list;
		m_list->lnext = NULL;
		m_list->lprev = NULL;
	} else {
		l_end->lnext = m_list;
		m_list->lprev = l_end;
		l_end = m_list;
		l_end->lnext = NULL;
	}
#ifndef NDEBUG
	checkList();
#endif
}
int KObjectList::save_disk_index(KFile *fp)
{
	int save_count = 0;
	KHttpObject *obj = l_end;
	while(obj){
		if(TEST(obj->index.flags,FLAG_IN_DISK)){
			if(!obj->saveIndex(fp)){
				return -1;
			}
			save_count++;
		}
		obj = obj->lprev;
	}
	return save_count;
}
void KObjectList::remove(KHttpObject *m_list)
{
	assert(m_list->list_state == this->list_state);
	if (m_list == l_head){
		l_head = l_head->lnext;
	}
	if (m_list == l_end) {
		l_end = m_list->lprev;
	}
	if (m_list->lnext){
		m_list->lnext->lprev = m_list->lprev;
	}
	if (m_list->lprev){
		m_list->lprev->lnext = m_list->lnext;
	}
#ifndef NDEBUG	
	m_list->list_state = LIST_IN_NONE;
	checkList();
#endif
}
void KObjectList::dump_refs_obj(std::stringstream &s)
{
	int max_dump = 50;
	KHttpObject *obj = l_head;
	while (obj) {
		if (obj->refs>1) {
			s << obj->refs << " " << obj->url->host << obj->url->path;
			if (obj->url->param) {
				s << "?" << obj->url->param;
			}
			s << "\r\n";
			if (max_dump--<=0) {
				break;
			}
		}
		obj = obj->lnext;
	}
}
void KObjectList::dead(KHttpObject *m_list)
{
	assert(m_list->list_state == this->list_state);
	cache_model = DROP_DEAD;
	if (m_list == l_head)
		return;
	if (m_list == l_end) {
		l_end = m_list->lprev;
	}
	if (m_list->lnext)
		m_list->lnext->lprev = m_list->lprev;
	if (m_list->lprev)
		m_list->lprev->lnext = m_list->lnext;
	l_head->lprev = m_list;
	m_list->lnext = l_head;
	l_head = m_list;
	l_head->lprev = NULL;
#ifndef NDEBUG
	checkList();
#endif
}
void KObjectList::swap_all_obj()
{
#ifdef ENABLE_DISK_CACHE
	cache.lock();
	KHttpObject *obj = l_head;
	while (obj) {
		if (!TEST(obj->index.flags,FLAG_DEAD)) {
			obj->swapout();
		}
		obj = obj->lnext;
	}
	cache.unlock();
#endif
}
#ifdef MALLOCDEBUG
void KObjectList::free_all_cache()
{
	KHttpObject *obj = l_head;
	while (obj) {
		assert(obj->refs==1);
		KHttpObject *next = obj->lnext;
		CLR(obj->index.flags,FLAG_IN_DISK);
		cache.objHash[obj->h].remove(obj);
		obj->release();
		obj = next;
	}
}
#endif
void KObjectList::dead_all_obj()
{
	cache_model = DROP_DEAD;
	KHttpObject *obj = l_head;
	while (obj) {
		SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
		obj = obj->lnext;
	}
}
void KObjectList::dead_count(int &count)
{
	cache_model = DROP_DEAD;
	KHttpObject *obj = l_head;
	while (obj && count>0) {
		SET(obj->index.flags, FLAG_DEAD | OBJ_INDEX_UPDATE);
		obj = obj->lnext;
		count--;
	}
}
void KObjectList::move(INT64 m_size,bool swapout)
{
	KMutex * lock = NULL;
	bool is_dead = true;
	if (m_size<=0 && cache_model==CLEAN_CACHE) {
		return;
	}
	KTempHttpObject *thead = NULL;
	KTempHttpObject *tend = NULL;
	time_t startTime = kgl_current_sec;
	cache.lock();
	KHttpObject * obj = l_head;
	while (obj) {	
		if (obj->getRefs() > 1) {
			obj = obj->lnext;
			continue;
		}
		is_dead = (TEST(obj->index.flags,FLAG_DEAD)>0);
		if (is_dead || m_size>0) {
			INT64 decSize;	
			
				decSize = obj->index.have_length;
				if (swapout && TEST(obj->index.flags,FLAG_NO_DISK_CACHE)) {
					//标记为不使用磁盘缓存的，直接dead
					is_dead = true;
				}
				
			m_size -= decSize;
			//加入到临时链表中
			KTempHttpObject *to = new KTempHttpObject;
			to->next = NULL;
			assert(obj->list_state == this->list_state);
			to->obj = obj;
			to->decSize = decSize;
			to->is_dead = is_dead;
			if (tend) {
				assert(thead);
				tend->next = to;
			} else {
				thead = to;
			}
			tend = to;
			obj = obj->lnext;
			continue;
		}
		cache_model = CLEAN_CACHE;
		break;
	}
	cache.unlock();
	//实际发生磁盘IO
	while (thead) {
		KHttpObject *obj = thead->obj;	
#ifdef ENABLE_DISK_CACHE
		if (swapout && 
			!thead->is_dead && 
			kgl_current_sec - startTime < GC_SLEEP_TIME &&
			obj->swapout()) {
			cache.lock();
			lock = obj->getLock();
			lock->Lock();
			SET(obj->index.flags,FLAG_IN_DISK);
			if (obj->list_state==this->list_state && obj->refs<=1) {
				remove(obj);
				CLR(obj->index.flags,FLAG_IN_MEM);
				cache.objHash[obj->h].decSize(thead->decSize);
				cache.objList[LIST_IN_DISK].add(obj);
				delete obj->data;
				obj->data = NULL;
			}
			lock->Unlock();
			cache.unlock();
		} else {
#endif
			bool result = false;
			cache.lock();
			lock = &cache.objHash[obj->h].lock;
			lock->Lock();
			//这里为什么要判断一下list_state呢？因为有可能在中间，obj被其它请求使用，如调用swap_in而改变了list_state.
			if (obj->list_state==this->list_state && obj->getRefs()<=1) {
				remove(obj);
				result = cache.objHash[obj->h].remove(obj);
				cache.count--;
			}
			lock->Unlock();
			cache.unlock();
			if (result) {
				obj->release();
			}
#ifdef ENABLE_DISK_CACHE
		}
#endif
		KTempHttpObject *tnext = thead->next;
		delete thead;
		thead = tnext;
	}
}

int KObjectList::clean_cache(KReg *reg,int flag)
{
	cache_model = DROP_DEAD;
	KHttpObject *obj = l_head;
	int result = 0;
	while (obj) {
		if (!TEST(obj->index.flags,FLAG_DEAD)) {
			KStringBuf url;
			if (obj->url->getUrl(url) && reg->match(url.getString(),url.getSize(),flag)>0) {
				result++;
				SET(obj->index.flags,FLAG_DEAD|OBJ_INDEX_UPDATE);
			}			
		}
		obj = obj->lnext;
	}
	return result;
}
void KObjectList::getSize(INT64 &csize,INT64 &cdsiz)
{
	KHttpObject *obj = l_head;
	while (obj) {
		if (TEST(obj->index.flags,FLAG_IN_MEM)) {
			
				csize += obj->index.have_length;
			
		}
		if (TEST(obj->index.flags,FLAG_IN_DISK)) {
			cdsiz += obj->index.have_length;
		}		
		obj = obj->lnext;
	}
}
