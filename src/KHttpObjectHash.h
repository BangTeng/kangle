#ifndef KHTTPOBJECTHASH_H_
#define KHTTPOBJECTHASH_H_
#include <map>
#include "malloc_debug.h"
#include "KHttpObjectNode.h"
#include "KHttpRequest.h"
#include "KMutex.h"
#include "rbtree.h"

typedef void (*objHandler)(KHttpObject *obj,void *param);

inline int cmpurl(KUrl *a,KUrl *b)
{
	return a->operator<(*b);
}
inline int cmpnurl(KUrl *a,KUrl *b,int len)
{
	return a->cmpn(b,len);
}
class KHttpObjectHash {
public:
	KHttpObjectHash() {
		size = 0;
		disk_size = 0;
		nodes.rb_node = NULL;
	}
	/**
	* 清除指定url的物件，wide指示是否泛匹配,即只匹配前面部分
	*/
	inline int purge(KUrl *url,bool wide,objHandler handle,void *param)
	{
		int count = 0;
		lock.Lock();
		if (wide) {	
			int path_len = (int)strlen(url->path);
			rb_node *node = findn(url,path_len);
			while (node) {
				count += purgeObject((KHttpObject *)node->data,handle,param);
				node = rb_next(node);
				if (node) {
					KHttpObject *obj = (KHttpObject *)node->data;
					if (cmpnurl(url,obj->url,path_len)!=0) {
						break;
					}
				}
			}
		} else {
			rb_node *node = find(url);
			if (node) {
				count += purgeObject((KHttpObject *)node->data,handle,param);						
			}
		}
		lock.Unlock();
		return count;
	}
#ifdef ENABLE_DISK_CACHE
	inline bool find(KUrl *url,const char *filename)
	{
		lock.Lock();
		rb_node *node = find(url);
		if (node==NULL) {
			lock.Unlock();
			return false;
		}
		KHttpObject *obj = (KHttpObject *)node->data;
		bool result = false;
		while (obj) {
			if (TEST(obj->index.flags,FLAG_IN_DISK)) {
				char *file = obj->getFileName();
				if (file) {
					result = (strcmp(file,filename)==0);
					free(file);
					if (result) {
						break;
					}
				}
			}
			obj = obj->next;
		}
		lock.Unlock();
		return result;
	}
#endif
	/**
	* 从缓存中查到指定url的物件，gzip,internal指示物件状态
	*/
	inline KHttpObject *get(KUrl *url,u_char accept_encoding, bool internal,bool no_disk_cache,time_t min_obj_verified) {
		assert(url->host);
		lock.Lock();
		rb_node *node = find(url);
		if (node==NULL) {
			lock.Unlock();
			return NULL;
		}
		KHttpObject *hit_obj = NULL;
		KHttpObject *obj = (KHttpObject *)node->data;
		while (obj) {
			if (TEST(obj->index.flags,FLAG_DEAD)) {
				obj = obj->next;
				continue;
			}
			if (min_obj_verified > 0 && obj->index.last_verified < min_obj_verified) {
				//设置了最小验证时间
				SET(obj->index.flags, FLAG_DEAD);
				obj = obj->next;
				continue;
			}
			if ((internal == (TEST(obj->index.flags, FLAG_RQ_INTERNAL)>0)) &&
				obj->url->match_accept_encoding(accept_encoding)) {
				if (!no_disk_cache || (obj->data != NULL && obj->data->type == MEMORY_OBJECT)) {
					//hit cache
					if (hit_obj == NULL || obj->url->encoding > hit_obj->url->encoding) {
						hit_obj = obj;
					}
				}
			}
			obj = obj->next;
		}
		if (hit_obj) {
			hit_obj->addRef();
		}
		lock.Unlock();
		return hit_obj;
	}
	/**
	* 增加内存大小
	*/
	void incSize(INT64 m_size)
	{
		size_lock.Lock();
		size += m_size;
		size_lock.Unlock();
	}
	/**
	* 减少内存大小
	*/
	void decSize(INT64 m_size)
	{
		size_lock.Lock();
		size -= m_size;
		size_lock.Unlock();
	}
	/**
	* 增加磁盘大小
	*/
	void incDiskSize(INT64 m_size)
	{
		size_lock.Lock();
		disk_size += m_size;
		size_lock.Unlock();
	}
	/**
	* 减少磁盘大小
	*/
	void decDiskSize(INT64 m_size)
	{
		size_lock.Lock();
		disk_size -= m_size;
		size_lock.Unlock();
	}
	/**
	* 删除指定物件,此调用由调用者加锁.
	*/
	bool remove(KHttpObject *obj) {
		//std::map<KUrl *, KHttpObjectNode *,lessurl>::iterator it;
		//		printf("KHttpObject:try to remove obj from hash url=%s,this=%x\n",obj->url->host,this);
		//lock.Lock(__FILE__,__LINE__);
		rb_node *node = find(obj->url);
		assert(node!=NULL);
		if (node==NULL) {
			klog(KLOG_ERR,"BUG!!!cache system cann't find obj [%s%s%s%s] to remov\n",
				obj->url->host,
				obj->url->path,
				(obj->url->param?"?":""),
				(obj->url->param?obj->url->param:"")
				);
			return false;
		}	
		KHttpObject *objnode = (KHttpObject *)(node->data);
		assert(objnode);
		KHttpObject *last = NULL;
		bool result = false;
		while (objnode) {
			if (obj==objnode) {
				result = true;
				if (last==NULL) {
					node->data = objnode->next;
				} else {
					last->next = objnode->next;
				}
				break;
			}
			last = objnode;
			objnode = objnode->next;
		}
		assert(result);
		if (node->data==NULL) {
			rb_erase(node,&nodes);
			delete node;
		}
		if (result) {
			size_lock.Lock();
			if (TEST(obj->index.flags,FLAG_IN_MEM)) {
				assert(obj->data);
				
					size -= obj->index.content_length;
			}
			if (TEST(obj->index.flags,FLAG_IN_DISK)) {
				disk_size -= obj->index.content_length;
			}
			size_lock.Unlock();
		}
		return result;
	}
	/**
	* 得到缓存大小
	*/
	void getSize(INT64 &cacheSize, INT64 &diskSize) {
		size_lock.Lock();
		cacheSize += size;
		diskSize += disk_size;
		size_lock.Unlock();
	}
	bool put(KHttpObject *obj) {
		//	assert(rq->url->host && rq->url->path);
		//	assert(obj && obj->url == &rq->url);
		assert(obj->refs==1);
		//此处可确保obj，不会被其它引用，所以不用加锁
		obj->refs++;
		if (!TEST(obj->index.flags,FLAG_URL_FREE)) {
			KUrl *url = obj->url->clone();
			obj->url = url;
			SET(obj->index.flags,FLAG_URL_FREE);
		}
		assert(obj->h == id);
		assert(obj->url->host && obj->url->path);
		lock.Lock();
		insert(obj);
		lock.Unlock();
		size_lock.Lock();
		obj->count_size(size,disk_size);
		size_lock.Unlock();
		return true;
	}
	u_short id;
	KMutex lock;
	KMutex size_lock; /* lock to change size		*/
	INT64 size; /* size of objects in this hash */
	INT64 disk_size;
private:
	/**
	* 清除物件
	*/
	inline int purgeObject(KHttpObject *objnode,objHandler handle,void *param)
	{
		int count = 0;
		while (objnode) {
			if (!TEST(objnode->index.flags,FLAG_DEAD)) {
				handle(objnode,param);
				count++;
			}
			objnode = objnode->next;
		}
		return count;
	}
	inline void insert(KHttpObject *obj)
	{
		struct rb_node **n = &(nodes.rb_node), *parent = NULL;
		KHttpObject *objnode = NULL;
		/* Figure out where to put new node */
		while (*n) {
			objnode = (KHttpObject *)((*n)->data);
			int result = cmpurl(obj->url,objnode->url);
			parent = *n;
			if (result < 0) {
				n = &((*n)->rb_left);
			} else if (result > 0) {
				n = &((*n)->rb_right);
			} else {
				assert((*n)->data);
				obj->next = objnode;
				(*n)->data = obj;
				return;
			}
		}					
		rb_node *node = new rb_node;
		memset(node, 0, sizeof(rb_node));
		node->data = obj;
		obj->next = NULL;
		rb_link_node(node, parent, n);
		rb_insert_color(node, &nodes);
	}
	inline rb_node *findn(KUrl *url,int path_len)
	{
		struct rb_node *last = NULL;
		struct rb_node *node = nodes.rb_node;
		while (node) {
			KHttpObject *data = (KHttpObject *)(node->data);
			int result;
			result = cmpnurl(url,data->url,path_len);
			if (result < 0) {
				node = node->rb_left;
			} else if (result > 0) {
				node = node->rb_right;
			} else {
				for(;;) {
					last = rb_prev(node);
					if (last==NULL) {
						return node;
					}
					data = (KHttpObject *)(last->data);
					if (cmpnurl(url,data->url,path_len)!=0){
						return node;
					}
					node = last;
				}
			}
		}
		return NULL;
	}
	inline rb_node *find(KUrl *url)
	{
		struct rb_node *node = nodes.rb_node;
		while (node) {
			KHttpObject *data = (KHttpObject *)(node->data);
			int result;
			result = cmpurl(url,data->url);
			if (result < 0)
				node = node->rb_left;
			else if (result > 0)
				node = node->rb_right;
			else
				return node;
		}
		return NULL;
	}
	struct rb_root nodes ;
};
#endif /*KHTTPOBJECTHASH_H_*/
