#include <string.h>
#include "KAddr.h"
#include "KList.h"
#include "rbtree.h"
#include "KMutex.h"
#include "katom.h"
#include "time_utils.h"
#include "KSelector.h"

#define KADDR_FRESH_TIME       30
#define KADDR_MAX_CACHE_COUNT  1048576
#define KADDR_MAX_IDLE_TIME    3600
#define KADDR_MAX_ERROR_COUNT  10

struct kgl_addr_queue {
	addr_call_back cb;
	KSelector *selector;
	void *arg;
	KAddr *addr;
	kgl_addr_queue *next;
};
struct kgl_addr_key {
	char *hostname;
	kgl_addr_type addr_type;
};
class kgl_addr_node {
public:
	kgl_addr_node() {
		memset(this, 0, sizeof(*this));
	}
	~kgl_addr_node() {
		if (key.hostname) {
			free(key.hostname);
		}
		if (addr) {
			addr->release();
		}
		if (last_success) {
			last_success->release();
		}
	}
	kgl_addr_key key;
	time_t last_time;
	time_t last_verify;
	int error_count;
	rb_node *node;
	KAddr *addr;
	KAddr *last_success;
	union {
		kgl_addr_node *next;
		kgl_addr_queue *queue;
	};
	kgl_addr_node *prev;
};
static rb_tree *addr_tree = NULL;
static kgl_addr_node addr_list;
static KMutex addr_lock;
static int addr_count = 0;
static int addr_key_cmp(void *k1, void *k2) {
	kgl_addr_key *node1 = (kgl_addr_key *)k1;
	kgl_addr_key *node2 = (kgl_addr_key *)k2;
	if (node1->addr_type < node2->addr_type) {
		return -1;
	}
	if (node1->addr_type > node2->addr_type) {
		return 1;
	}
	return strcasecmp(node1->hostname, node2->hostname);
}

static bool get_addr(const char *hostname, kgl_addr_type addr_type , struct addrinfo **res)
{
	struct addrinfo f;
	memset(&f, 0, sizeof(f));
	switch (addr_type) {
	case kgl_addr_cname:
		f.ai_family = PF_INET;
		f.ai_flags = AI_CANONNAME;
	default:
		f.ai_family = PF_UNSPEC;
		f.ai_flags = 0;
	}
#ifndef KSOCKET_IPV6
	f.ai_family = PF_INET;
#endif
	getaddrinfo(hostname, NULL, &f, res);
	return true;
}
static kgl_addr_queue *update_addr_cache(kgl_addr_node *cn, KAddr *addr) {
	addr_lock.Lock();
	kgl_addr_queue *queue = cn->queue;
	assert(cn->addr == NULL);
	assert(cn->last_success == NULL);
	cn->addr = addr;
	if (addr) {
		addr->add_refs();
	}
	cn->last_success = addr;
	klist_append(&addr_list, cn);
	addr_count++;
	addr_lock.Unlock();
	return queue;
}
static void update_addr_cache(kgl_addr_key *key, struct addrinfo *addr) {
	int new_flag = 0;
	addr_lock.Lock();
	rb_node *node = rbtree_insert(addr_tree, (void *)key, &new_flag, addr_key_cmp);
	assert(node);
	kgl_addr_node *cn;
	if (new_flag) {
		addr_count++;
		cn = new kgl_addr_node;
		cn->key.hostname = strdup(key->hostname);
		cn->key.addr_type = key->addr_type;
		cn->last_time = cn->last_verify = kgl_current_sec;
		cn->node = node;
		node->data = cn;
	} else {
		cn = (kgl_addr_node *)node->data;
		klist_remove(cn);
	}
	if (cn->addr != NULL) {
		cn->addr->release();
		cn->addr = NULL;
	}
	if (addr) {
		cn->error_count = 0;
		cn->addr = new KAddr(addr);
		if (cn->last_success) {
			cn->last_success->release();
		}
		cn->addr->add_refs();
		cn->last_success = cn->addr;
	} else {
		cn->error_count++;
		if (cn->error_count >= KADDR_MAX_ERROR_COUNT && cn->last_success) {
			cn->last_success->release();
			cn->last_success = NULL;
		}
	}
	klist_append(&addr_list, cn);
	addr_lock.Unlock();
}

static void addr_next_call(void *arg, int len)
{
	kgl_addr_queue *queue = (kgl_addr_queue *)arg;
	queue->cb(queue->arg, queue->addr);
	if (queue->addr) {
		queue->addr->release();
	}
	delete queue;
}
static void addr_result(kgl_addr_queue *queue, KAddr *addr) {
	kgl_addr_queue *next;
	while (queue) {
		next = queue->next;
		queue->addr = addr;
		if (addr) {
			addr->add_refs();
		}
		queue->selector->next(addr_next_call, queue);
		queue = next;
	}
}
void find_addr_result(kgl_addr_node *cn, struct addrinfo *addr, bool success)
{
	assert(cn->prev == NULL);
	kgl_addr_queue *queue;
	if (!success) {
		assert(addr == NULL);
		addr_lock.Lock();
		queue = cn->queue;
		rbtree_remove(addr_tree, cn->node);
		addr_lock.Unlock();
		delete cn;
		addr_result(queue, NULL);
		return;
	}
	KAddr *addr2 = NULL;
	if (addr) {
		addr2 = new KAddr(addr);
	}
	queue = update_addr_cache(cn, addr2);
	addr_result(queue,addr2);
	
}
KTHREAD_FUNCTION async_addr_worker(void *data, int msec)
{
	kgl_addr_node *cn = (kgl_addr_node *)data;
	struct addrinfo *res = NULL;
	assert(cn->queue->selector);
	if (msec<0 || msec > 10000) {
		//10 second time out
		find_addr_result(cn, NULL, false);
		KTHREAD_RETURN;
	}
	if (get_addr(cn->key.hostname,cn->key.addr_type, &res)) {
		find_addr_result(cn, res, true);
	} else {
		find_addr_result(cn, NULL, false);
	}
	KTHREAD_RETURN;
}
KTHREAD_FUNCTION async_addr_cache_worker(void *data, int msec)
{
	kgl_addr_key *key = (kgl_addr_key *)data;
	struct addrinfo *res = NULL;
	if (get_addr(key->hostname,key->addr_type, &res)) {
		update_addr_cache(key,res);
	}
	free(key->hostname);
	delete key;
	KTHREAD_RETURN;
}

kgl_addr_node * find_addr_in_tree(const char *hostname, kgl_addr_type addr_type, bool &create_flag)
{
	int new_flag = 0;
	rb_node *node = NULL;
	kgl_addr_key key;
	key.hostname = (char *)hostname;
	key.addr_type = addr_type;
	if (create_flag) {
		node = rbtree_insert(addr_tree, (void *)&key, &new_flag, addr_key_cmp);
	} else {
		node = rbtree_find(addr_tree, (void *)&key, addr_key_cmp);
	}
	kgl_addr_node *cn = NULL;
	if (node && !new_flag) {
		cn = (kgl_addr_node *)node->data;
		if (cn->prev) {
			klist_remove(cn);
			klist_append(&addr_list, cn);
			cn->last_time = kgl_current_sec;
			if (kgl_current_sec - cn->last_verify > KADDR_FRESH_TIME) {
				cn->last_verify = kgl_current_sec;
				kgl_addr_key *k = new kgl_addr_key;
				k->hostname = strdup(hostname);
				k->addr_type = addr_type;
				if (!conf.dnsWorker->tryStart(k, async_addr_cache_worker)) {
					free(k->hostname);
					delete k;
				}				
			}
		}
		create_flag = false;
		return cn;
	}
	if (create_flag) {
		cn = new kgl_addr_node;
		cn->key.hostname = strdup(hostname);
		cn->key.addr_type = addr_type;
		cn->last_time = cn->last_verify = kgl_current_sec;
		cn->node = node;
		node->data = cn;
	}
	return cn;
}
KAddr *find_cache_addr(const char *hostname,kgl_addr_type addr_type)
{
	bool create_flag = false;
	KAddr *addr = NULL;
	addr_lock.Lock();
	kgl_addr_node *cn = find_addr_in_tree(hostname, addr_type, create_flag);
	if (cn && cn->prev != NULL) {
		addr = cn->addr;
		if (addr) {
			addr->add_refs();
		}
	}
	addr_lock.Unlock();
	return addr;
}

void find_addr(const char *hostname, kgl_addr_type addr_type, addr_call_back cb, void *arg, KSelector *selector)
{
	bool create_flag = true;
	addr_lock.Lock();
	kgl_addr_node *cn = find_addr_in_tree(hostname, addr_type,create_flag);
	assert(cn);
	if (cn->prev != NULL) {
		KAddr *addr = cn->addr;
		if (addr == NULL) {
			addr = cn->last_success;
		}
		if (addr != NULL) {
			addr->add_refs();
		}
		addr_lock.Unlock();
		cb(arg, addr);
		if (addr) {
			addr->release();
		}
		return;
	}
	kgl_addr_queue *addr_queue = new kgl_addr_queue;
	memset(addr_queue, 0, sizeof(kgl_addr_queue));
	addr_queue->cb = cb;
	addr_queue->arg = arg;
	addr_queue->selector = selector;
	addr_queue->next = cn->queue;
	cn->queue = addr_queue;
	addr_lock.Unlock();
	if (create_flag) {
		if (!conf.dnsWorker->tryStart(cn, async_addr_worker, true)) {
			async_addr_worker(cn, -1);
		}
	}
}
void init_addr_worker()
{
	addr_tree = rbtree_create();
	klist_init(&addr_list);
	addr_count = 0;
}
void flush_addr_cache(time_t nowTime)
{
	addr_lock.Lock();
	for (;;) {
		kgl_addr_node *cn = klist_head(&addr_list);
		if (cn == &addr_list) {
			break;
		}
		if (nowTime - cn->last_time<KADDR_MAX_IDLE_TIME
			&& addr_count<KADDR_MAX_CACHE_COUNT) {
			break;
		}
		rbtree_remove(addr_tree, cn->node);
		klist_remove(cn);
		delete cn;
		addr_count--;
	}
	addr_lock.Unlock();
}
int get_addr_cache_count()
{
	addr_lock.Lock();
	int addr_count2 = addr_count;
	addr_lock.Unlock();
	return addr_count2;
}
