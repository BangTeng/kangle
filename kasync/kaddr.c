#include <assert.h>
#include <time.h>
#include "kfeature.h"
#include "kaddr.h"
#include "krbtree.h"
#include "ksync.h"
#include "ksocket.h"
#include "klist.h"
#include "kasync_worker.h"
#include "kselector_manager.h"
#include "katom.h"
#include "kmalloc.h"

#define KADDR_FRESH_TIME       30
#define KADDR_MAX_CACHE_COUNT  1048576
#define KADDR_MAX_IDLE_TIME    3600
#define KADDR_MAX_ERROR_COUNT  10

typedef struct kgl_addr_queue_s kgl_addr_queue;
typedef struct kgl_addr_node_s kgl_addr_node;

struct kgl_addr_queue_s {
	kgl_addr_call_back cb;
	kselector *selector;
	void *arg;
	kgl_addr *addr;
	kgl_addr_queue *next;
};

typedef struct {
	char *hostname;
	kgl_addr_type addr_type;
} kgl_addr_key;

struct  kgl_addr_node_s  {
	kgl_addr_key key;
	time_t last_time;
	time_t last_verify;
	int error_count;
	struct krb_node *node;
	kgl_addr *addr;
	kgl_addr *last_success;
	union {
		kgl_addr_node *next;
		kgl_addr_queue *queue;
	};
	kgl_addr_node *prev;
};

static struct krb_tree *addr_tree = NULL;
static kgl_addr_node addr_list;
static kmutex addr_lock;
static int addr_count = 0;
static kasync_worker *addr_worker = NULL;
bool kgl_addr_build(kgl_addr *addr, uint16_t port, sockaddr_i *sockaddr)
{
	memcpy(sockaddr, addr->addr->ai_addr, MIN(addr->addr->ai_addrlen, sizeof(sockaddr_i)));
#ifdef KSOCKET_IPV6
	if (addr->addr->ai_family == PF_INET6) {
		sockaddr->v6.sin6_port = htons(port);
		return true;
	}
#endif
	sockaddr->v4.sin_port = htons(port);
	return true;
}
static kgl_addr *kgl_addr_new(struct addrinfo *ai)
{
	kgl_addr *addr = (kgl_addr *)xmalloc(sizeof(kgl_addr));
	memset(addr, 0, sizeof(kgl_addr));
	addr->refs = 1;
	addr->addr = ai;
	return addr;
}
static void kgl_addr_refs(kgl_addr *addr)
{
	katom_inc((void *)&addr->refs);
}
void kgl_addr_release(kgl_addr *addr)
{
	if (katom_dec((void *)&addr->refs) == 0) {
		if (addr->addr) {
			freeaddrinfo(addr->addr);
		}
		xfree(addr);
	}
}
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
static bool get_addr(const char *hostname, kgl_addr_type addr_type, struct addrinfo **res)
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
static kgl_addr_queue *update_addr_node_cache(kgl_addr_node *cn, kgl_addr *addr) {
	kmutex_lock(&addr_lock);
	kgl_addr_queue *queue = cn->queue;
	kassert(cn->addr == NULL);
	kassert(cn->last_success == NULL);
	cn->addr = addr;
	if (addr) {
		kgl_addr_refs(addr);
	}
	cn->last_success = addr;
	klist_append(&addr_list, cn);
	addr_count++;
	kmutex_unlock(&addr_lock);
	return queue;
}
static void update_addr_cache(kgl_addr_key *key, struct addrinfo *addr) {
	int new_flag = 0;
	kmutex_lock(&addr_lock);
	struct krb_node *node = rbtree_insert(addr_tree, (void *)key, &new_flag, addr_key_cmp);
	kassert(node);
	kgl_addr_node *cn;
	if (new_flag) {
		addr_count++;
		cn = (kgl_addr_node *)xmalloc(sizeof(kgl_addr_node));
		memset(cn, 0, sizeof(kgl_addr_node));
		cn->key.hostname = strdup(key->hostname);
		cn->key.addr_type = key->addr_type;
		cn->last_time = cn->last_verify = time(NULL);
		cn->node = node;
		node->data = cn;
	} else {
		cn = (kgl_addr_node *)node->data;
		klist_remove(cn);
	}
	if (cn->addr != NULL) {
		kgl_addr_release(cn->addr);
		cn->addr = NULL;
	}
	if (addr) {
		cn->error_count = 0;
		cn->addr = kgl_addr_new(addr);
		if (cn->last_success) {
			kgl_addr_release(cn->last_success);
		}
		kgl_addr_refs(cn->addr);
		cn->last_success = cn->addr;
	} else {
		cn->error_count++;
		if (cn->error_count >= KADDR_MAX_ERROR_COUNT && cn->last_success) {
			kgl_addr_release(cn->last_success);
			cn->last_success = NULL;
		}
	}
	klist_append(&addr_list, cn);
	kmutex_unlock(&addr_lock);
}

static kev_result addr_next_call(void *arg, int len)
{
	kgl_addr_queue *queue = (kgl_addr_queue *)arg;
	queue->cb(queue->arg, queue->addr);
	if (queue->addr) {
		kgl_addr_release(queue->addr);
	}
	xfree(queue);
	return kev_ok;
}
static void addr_result(kgl_addr_queue *queue, kgl_addr *addr) {
	kgl_addr_queue *next;
	while (queue) {
		next = queue->next;
		queue->addr = addr;
		if (addr) {
			kgl_addr_refs(addr);
		}
		kgl_selector_module.next(queue->selector, addr_next_call, queue, 0);
		queue = next;
	}
}
static void kgl_addr_node_destroy(kgl_addr_node *cn)
{
	if (cn->key.hostname) {
		xfree(cn->key.hostname);
	}
	if (cn->addr) {
		kgl_addr_release(cn->addr);
	}
	if (cn->last_success) {
		kgl_addr_release(cn->last_success);
	}
	xfree(cn);
}
static void find_addr_result(kgl_addr_node *cn, struct addrinfo *addr, bool success)
{
	kassert(cn->prev == NULL);
	kgl_addr_queue *queue;
	if (!success) {
		kassert(addr == NULL);
		kmutex_lock(&addr_lock);
		queue = cn->queue;
		rbtree_remove(addr_tree, cn->node);
		kmutex_unlock(&addr_lock);
		kgl_addr_node_destroy(cn);
		addr_result(queue, NULL);
		return;
	}
	kgl_addr *addr2 = NULL;
	if (addr) {
		addr2 = kgl_addr_new(addr);
	}
	queue = update_addr_node_cache(cn, addr2);
	addr_result(queue, addr2);
}

kev_result async_addr_worker(void *data, int msec)
{
	kgl_addr_node *cn = (kgl_addr_node *)data;
	struct addrinfo *res = NULL;
	kassert(cn->queue->selector);
	if (msec < 0 || msec > 10000) {
		//10 second time out
		find_addr_result(cn, NULL, false);
		return kev_ok;
	}
	if (get_addr(cn->key.hostname, cn->key.addr_type, &res)) {
		find_addr_result(cn, res, true);
		return kev_ok;
	}
	find_addr_result(cn, NULL, false);
	return kev_ok;
}
kev_result async_addr_cache_worker(void *data, int msec)
{
	kgl_addr_key *key = (kgl_addr_key *)data;
	struct addrinfo *res = NULL;
	if (get_addr(key->hostname, key->addr_type, &res)) {
		update_addr_cache(key, res);
	}
	xfree(key->hostname);
	xfree(key);
	return kev_ok;
}

kgl_addr_node * find_addr_in_tree(const char *hostname, kgl_addr_type addr_type, bool *create_flag)
{
	int new_flag = 0;
	struct krb_node *node = NULL;
	kgl_addr_key key;
	key.hostname = (char *)hostname;
	key.addr_type = addr_type;
	if (*create_flag) {
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
			cn->last_time = time(NULL);
			if (cn->last_time - cn->last_verify > KADDR_FRESH_TIME) {
				cn->last_verify = cn->last_time;
				kgl_addr_key *k = (kgl_addr_key *)xmalloc(sizeof(kgl_addr_key));
				k->hostname = strdup(hostname);
				k->addr_type = addr_type;
				if (!kasync_worker_try_start(addr_worker, k, async_addr_cache_worker, false)) {
					xfree(k->hostname);
					xfree(k);
				}
			}
		}
		*create_flag = false;
		return cn;
	}
	if (*create_flag) {
		cn = (kgl_addr_node *)xmalloc(sizeof(kgl_addr_node));
		memset(cn, 0, sizeof(kgl_addr_node));
		cn->key.hostname = strdup(hostname);
		cn->key.addr_type = addr_type;
		cn->last_time = cn->last_verify = time(NULL);
		cn->node = node;
		node->data = cn;
	}
	return cn;
}
kev_result kgl_find_addr(const char *hostname, kgl_addr_type addr_type, kgl_addr_call_back cb, void *arg, kselector *selector)
{
	bool create_flag = true;
	kmutex_lock(&addr_lock);
	kgl_addr_node *cn = find_addr_in_tree(hostname, addr_type, &create_flag);
	kassert(cn);
	if (cn->prev != NULL) {
		kgl_addr *addr = cn->addr;
		if (addr == NULL) {
			addr = cn->last_success;
		}
		if (addr != NULL) {
			kgl_addr_refs(addr);
		}
		kmutex_unlock(&addr_lock);
		kev_result ret = cb(arg, addr);
		if (addr) {
			kgl_addr_release(addr);
		}
		return ret;
	}
	kgl_addr_queue *addr_queue = (kgl_addr_queue *)xmalloc(sizeof(kgl_addr_queue));
	memset(addr_queue, 0, sizeof(kgl_addr_queue));
	addr_queue->cb = cb;
	addr_queue->arg = arg;
	addr_queue->selector = selector;
	addr_queue->next = cn->queue;
	cn->queue = addr_queue;
	kmutex_unlock(&addr_lock);
	if (create_flag) {
		if (!kasync_worker_try_start(addr_worker,cn, async_addr_worker, true)) {
			async_addr_worker(cn, -1);
		}
	}
	return kev_ok;
}
kgl_addr *kgl_find_cache_addr(const char *hostname, kgl_addr_type addr_type)
{
	bool create_flag = false;
	kgl_addr *addr = NULL;
	kmutex_lock(&addr_lock);
	kgl_addr_node *cn = find_addr_in_tree(hostname, addr_type, &create_flag);
	if (cn && cn->prev != NULL) {
		addr = cn->addr;
		if (addr) {
			kgl_addr_refs(addr);
		}
	}
	kmutex_unlock(&addr_lock);
	return addr;
}

void kgl_flush_addr_cache(time_t now_time)
{
	kmutex_lock(&addr_lock);
	for (;;) {
		kgl_addr_node *cn = klist_head(&addr_list);
		if (cn == &addr_list) {
			break;
		}
		if (now_time - cn->last_time < KADDR_MAX_IDLE_TIME
			&& addr_count < KADDR_MAX_CACHE_COUNT) {
			break;
		}
		rbtree_remove(addr_tree, cn->node);
		klist_remove(cn);
		kgl_addr_node_destroy(cn);
		addr_count--;
	}
	kmutex_unlock(&addr_lock);
}
int kgl_get_addr_cache_count()
{
	kmutex_lock(&addr_lock);
	int addr_count2 = addr_count;
	kmutex_unlock(&addr_lock);
	return addr_count2;
}
kev_result kgl_addr_timer(void *arg, int got)
{
	kgl_flush_addr_cache(time(NULL));
	kselector *selector = (kselector *)arg;
#ifdef MALLOCDEBUG
	if (selector->closed) {
		kgl_flush_addr_cache(time(NULL) + 360000);
		return kev_ok;
	}
#endif
	kselector_add_timer(selector, kgl_addr_timer, arg, 2000, NULL);
	return kev_ok;
}
static void when_selector_manager_ready(void *arg)
{
	kselector *selector = get_perfect_selector();
	kassert(selector);
	kgl_selector_module.next(selector, kgl_addr_timer, selector, 0);
}
void kgl_addr_init()
{
	kmutex_init(&addr_lock, NULL);
	addr_tree = rbtree_create();
	klist_init(&addr_list);
	addr_count = 0;
	addr_worker = kasync_worker_init(4, 64);
	selector_manager_on_ready(when_selector_manager_ready, NULL);
}
