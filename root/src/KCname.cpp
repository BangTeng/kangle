#include "global.h"
#ifdef HAVE_GETADDRINFO_A
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif 
#include <netdb.h>
#endif
#include "KCname.h"
#ifdef ENABLE_CNAME_BIND
#include "KHttpRequest.h"
#include "KAsyncWorker.h"
#include "KSocket.h"
#include "KList.h"
#include "KConfig.h"
#define KCNAME_FRESH_TIME       300
#define KCNAME_MAX_CACHE_COUNT  1048576
#define KCNAME_MAX_IDLE_TIME    86400
static KMutex cname_lock;
static int cname_item_count = 0;
struct kgl_cname_queue {
	cname_call_back cb;
	KHttpRequest *rq;
	kgl_cname_queue *next;
};
class kgl_cname_node {
public:
	kgl_cname_node() {
		memset(this,0,sizeof(*this));
	}
	~kgl_cname_node() {
		if (hostname) {
			free(hostname);
		}
		if (cname.data) {
			free(cname.data);
		}
	}
	char *hostname;
	kgl_str_t cname;
	time_t last_time;
	time_t last_verify;
	rb_node *node;
	union {
		kgl_cname_node *next;
		kgl_cname_queue *queue;
	};
	kgl_cname_node *prev;
};
static rb_tree *cname_tree = NULL;
static kgl_cname_node cname_list;
void init_cname_worker()
{
	cname_tree = rbtree_create();
	klist_init(&cname_list);
	cname_item_count = 0;
}
static int cname_cache_cmp(void *k1,void *k2) {
	const char *hostname = (const char *)k1;
	kgl_cname_node *node = (kgl_cname_node *)k2;
	return strcasecmp(hostname,node->hostname);
}
static bool get_cname_addr(const char *hostname,struct addrinfo **res)
{	
#ifndef KSOCKET_IPV6
	ai_family = PF_INET;
#endif
	struct addrinfo f;
	memset(&f, 0, sizeof(f));
	f.ai_family = PF_INET;
	f.ai_flags = AI_CANONNAME;
	int ret = getaddrinfo(hostname, NULL, &f, res);
	return ret==0;
}
static kgl_cname_queue *update_cname_cache(kgl_cname_node *cn,const char *cname,int len) {
	cname_lock.Lock();
	kgl_cname_queue *queue = cn->queue;
	assert(cn->cname.data==NULL);
	if (cn->cname.data) {
		free(cn->cname.data);
		cn->cname.data = NULL;
	}
	cn->cname.len = len;
	if (cname) {
		cn->cname.data = strdup(cname);
	}
	klist_append(&cname_list,cn);
	cname_item_count++;
	cname_lock.Unlock();
	return queue;
}
static void update_cname_cache(const char *hostname,const char *cname,int len) {
	int new_flag = 0;
	cname_lock.Lock();	
	rb_node *node = rbtree_insert(cname_tree,(void *)hostname,&new_flag,cname_cache_cmp);
	assert(node);
	kgl_cname_node *cn;
	if (new_flag) {
		cname_item_count++;
		cn = new kgl_cname_node;
		cn->hostname = strdup(hostname);
		cn->last_time = cn->last_verify = kgl_current_sec;
		cn->node = node;
		node->data = cn;
	} else {	
		cn = (kgl_cname_node *)node->data;
		if (cn->cname.data) {
			free(cn->cname.data);
			cn->cname.data = NULL;
		}
		klist_remove(cn);
	}
	cn->cname.len = len;
	if (cname) {
		cn->cname.data = strdup(cname);
	}
	klist_append(&cname_list,cn);
	cname_lock.Unlock();
}
static void cname_result(kgl_cname_queue *queue,const char *cname,int len) {
	kgl_cname_queue *next;
	while (queue) {
		next = queue->next;
		queue->cb(queue->rq,cname,len);
		delete queue;
		queue = next;
	}
}
void find_cname_result(kgl_cname_node *cn,const char *cname,bool success)
{
	assert(cn->prev==NULL);
	kgl_cname_queue *queue;
	if (!success) {		
		cname_lock.Lock();
		queue = cn->queue;
		rbtree_remove(cname_tree,cn->node);
		cname_lock.Unlock();
		delete cn;
		cname_result(queue,NULL,0);
		return;
	}
	if (cname) {
		KStringBuf str;
		str << cn->hostname;
		str.WSTR(".");
		str << cname;
		queue = update_cname_cache(cn,str.getString(),str.getSize());
		cname_result(queue,str.getString(),str.getSize());
	} else {
		queue = update_cname_cache(cn,NULL,0);
		cname_result(queue,NULL,0);
	}
}
KTHREAD_FUNCTION cnameAsyncWorkerCallBack(void *data,int msec)
{
	kgl_cname_node *cn  = (kgl_cname_node *)data;
	struct addrinfo *res = NULL;
	assert(cn->queue->rq);
	if (msec > 10000) {
		//10 second time out
		find_cname_result(cn,NULL,false);
		KTHREAD_RETURN;
	}
	if (get_cname_addr(cn->hostname,&res)) {
		char *cname = NULL;
		if (res) {			
			if (res->ai_canonname && strcasecmp(res->ai_canonname,cn->hostname)!=0) {
				cname = res->ai_canonname;
			}
		}
		find_cname_result(cn,cname,true);
	} else {
		find_cname_result(cn,NULL,false);
	}
	if (res) {
		freeaddrinfo(res);
	}
	KTHREAD_RETURN;
}
KTHREAD_FUNCTION cnameCacheAsyncWorkerCallBack(void *data,int msec)
{
	char *hostname = (char *)data;
	struct addrinfo *res = NULL;
	if (get_cname_addr(hostname,&res)) {
		char *cname = NULL;
		if (res) {			
			if (res->ai_canonname && strcasecmp(res->ai_canonname,hostname)!=0) {
				cname = res->ai_canonname;
			}
		}
		if (cname) {
			KStringBuf str;
			str << hostname;
			str.WSTR(".");
			str << cname;
			update_cname_cache(hostname,str.getString(),str.getSize());	
		} else {
			update_cname_cache(hostname,NULL,0);		
		}
	}
	if (res) {
		freeaddrinfo(res);
	}
	free(hostname);
	KTHREAD_RETURN;
}
inline kgl_cname_node *kgl_find_cache_cname(const char *hostname,bool &create_flag) {
	int new_flag = 0;	
	rb_node *node = NULL;
	if (create_flag) {
		node = rbtree_insert(cname_tree,(void *)hostname,&new_flag,cname_cache_cmp);
	} else {
		node = rbtree_find(cname_tree,(void *)hostname,cname_cache_cmp);
	}
	kgl_cname_node *cn = NULL;
	if (node && !new_flag) {
		cn = (kgl_cname_node *)node->data;
		if (cn->prev) {
			klist_remove(cn);
			klist_append(&cname_list,cn);
			cn->last_time = kgl_current_sec;			
			if (kgl_current_sec - cn->last_verify > KCNAME_FRESH_TIME) {
				cn->last_verify = kgl_current_sec;
				char *name = strdup(hostname);
				if (name) {
					conf.dnsWorker->start(name,cnameCacheAsyncWorkerCallBack);
				}
			}
		}
		create_flag = false;
		return cn;
	}
	if (create_flag) {
		cn = new kgl_cname_node;
		cn->hostname = strdup(hostname);
		cn->last_time = cn->last_verify = kgl_current_sec;
		cn->node = node;
		node->data = cn;
	}
	return cn;
}
void kgl_find_cname(const char *hostname,cname_call_back cb,KHttpRequest *rq)
{
	bool create_flag = true;
	cname_lock.Lock();
	kgl_cname_node *cn = kgl_find_cache_cname(hostname,create_flag);
	assert(cn);
	if (cn->prev!=NULL) {
		char cname[256];
		int len = MIN(sizeof(cname),cn->cname.len);
		memcpy(cname,cn->cname.data,len);
		cname_lock.Unlock();
		if (len==0) {
			cb(rq,NULL,0);
			return;
		}
		if (len>0) {
			cb(rq,cname,len);
			return;
		}
		return;
	}
	rq->c->removeRequest(rq,true);
	kgl_cname_queue *arg = new kgl_cname_queue;
	arg->cb = cb;
	arg->rq = rq;
	arg->next = cn->queue;
	cn->queue = arg;
	cname_lock.Unlock();
	if (create_flag) {
		conf.dnsWorker->start(cn,cnameAsyncWorkerCallBack,true);
	}
}
int kgl_find_cache_cname(const char *hostname,char *cname,int cname_size)
{
	int ret_len = -1;
	bool create_flag = false;
	cname_lock.Lock();
	kgl_cname_node *cn = kgl_find_cache_cname(hostname,create_flag);
	if (cn && cn->prev!=NULL) {
		ret_len = MIN(cname_size,(int)cn->cname.len);
		memcpy(cname,cn->cname.data,ret_len);		
	}
	cname_lock.Unlock();
	return ret_len;
}
void flush_cname_cache(time_t nowTime)
{
	cname_lock.Lock();
	for (;;) {
		kgl_cname_node *cn = klist_head(&cname_list);
		if (cn==&cname_list) {
			break;
		}
		if (nowTime - cn->last_time<KCNAME_MAX_IDLE_TIME
			&& cname_item_count<KCNAME_MAX_CACHE_COUNT) {
			break;
		}
		rbtree_remove(cname_tree,cn->node);
		klist_remove(cn);
		delete cn;
		cname_item_count--;
	}
	cname_lock.Unlock();
}
#endif
