#include "KCdnContainer.h"
#include "KHttpProxyFetchObject.h"
#include "KMultiAcserver.h"
#include "KAcserverManager.h"
#ifdef ENABLE_TCMALLOC
#include "google/heap-checker.h"
#endif
KCdnContainer cdnContainer;
using namespace std;
static int redirect_node_cmp(void *k1, void *k2)
{
	const char *name = (const char *)k1;
	KRedirectNode *rd = (KRedirectNode *)k2;
	return strcasecmp(name, rd->name);
}
KCdnContainer::KCdnContainer()
{
#ifdef ENABLE_TCMALLOC
	HeapLeakChecker::Disabler disabler;
#endif
	memset(&rd_list, 0, sizeof(rd_list));
	klist_init(&rd_list);
	rd_map = rbtree_create();
}
KCdnContainer::~KCdnContainer()
{
}
KRedirect *KCdnContainer::refsRedirect(const char *ip, const char *host, int port, const char *ssl, int life_time, Proto_t proto, bool isIp)
{
	KStringBuf s;
	s << "s://";
	if (ip) {
		s << ip << "_";
	}
	if (host) {
		s << host << ":";
	}
	s << port;
	if (ssl) {
		s << ssl;
	}
	s << "_" << (int)proto;
	lock.Lock();
	KRedirect *rd = findRedirect(s.getString());
	if (rd) {
		rd->addRef();
		lock.Unlock();
		return rd;
	}
	KSingleAcserver *server = new KSingleAcserver;
	server->proto = proto;
	server->sockHelper->setHostPort(host,port,ssl);
	server->sockHelper->setLifeTime(life_time);
	server->sockHelper->setIp(ip);
	KRedirectNode *rn = new KRedirectNode;
	rn->name = s.stealString();
	rn->rd = server;
	addRedirect(rn);
	server->addRef();
	lock.Unlock();
	return server;
}
KMultiAcserver *KCdnContainer::refsMultiServer(const char *name)
{
	if (*name == '$') {
		return conf.gam->refsMultiAcserver(name + 1);
	}
	lock.Lock();
	KRedirect *rd = findRedirect(name);
	if (rd) {
		rd->addRef();
	}
	lock.Unlock();
	if (rd == NULL) {
		return NULL;
	}
	if (strcmp(rd->getType(), "mserver") != 0) {
		rd->release();
		return NULL;
	}
	return static_cast<KMultiAcserver *>(rd);
}
KRedirect *KCdnContainer::refsRedirect(const char *name)
{
	if (*name == '$') {
		return conf.gam->refsMultiAcserver(name + 1);
	}
	lock.Lock();
	KRedirect *rd = findRedirect(name);
	if (rd) {
		rd->addRef();
		lock.Unlock();
		return rd;
	}
	char *buf = strdup(name);
	KMultiAcserver *server = new KMultiAcserver();
	int max_error_count = 3;
	int error_try_time = 0;
	char *hot = buf;
	for (;;) {
		char *p = strchr(hot, '/');
		if (p) {
			*p = '\0';
		}
		char *eq = strchr(hot, '=');
		
		if (eq) {
			*eq = '\0';
			char *val = eq + 1;
			if (strcasecmp(hot, "proto") == 0) {
				server->proto = KPoolableRedirect::parseProto(val);
			} else if (strcasecmp(hot, "ip_hash") == 0) {
				server->ip_hash = atoi(val)>0;
			} else if(strcasecmp(hot,"url_hash")==0) {
				server->url_hash = atoi(val)>0;
			} else if (strcasecmp(hot, "cookie_stick") == 0) {
				server->cookie_stick = atoi(val)>0;
			} else if (strcasecmp(hot, "error_try_time") == 0) {
				error_try_time = atoi(val);
				
			} else if (strcasecmp(hot, "error_count") == 0) {
				max_error_count = atoi(val);
			} else if (strcasecmp(hot, "nodes") == 0) {
				server->parseNode(val);
			}
		}
		if (p == NULL) {
			break;
		}
		hot = p + 1;
	}
	server->setErrorTryTime(max_error_count,error_try_time);
	free(buf);
	KRedirectNode *rn = new KRedirectNode;
	rn->name = strdup(name);
	rn->rd = server;
	addRedirect(rn);
	server->addRef();
	lock.Unlock();
	return server;
}
KFetchObject *KCdnContainer::get(const char *ip,const char *host,int port,const char *ssl,int life_time,Proto_t proto,bool isIp)
{
	KRedirect *server = refsRedirect(ip,host,port,ssl,life_time,proto,isIp);
	KBaseRedirect *brd = new KBaseRedirect(server,false);
	KFetchObject *fo = new KHttpProxyFetchObject();
	fo->bindBaseRedirect(brd);
	brd->release();	
	return fo;
}
KFetchObject *KCdnContainer::get(const char *name)
{
	KRedirect *server = refsRedirect(name);
	if (server==NULL) {
		return NULL;
	}
	KBaseRedirect *brd = new KBaseRedirect(server,false);
	KFetchObject *fo = new KHttpProxyFetchObject();
	fo->bindBaseRedirect(brd);
	brd->release();	
	return fo;
}
void KCdnContainer::flush(time_t nowTime)
{
	lock.Lock();
	KRedirectNode *rn = rd_list.next;
	while (rn != &rd_list) {
		if (nowTime - rn->lastActive < 300) {
			break;
		}
		//if (rn->rd->getRefFast()<=1) {
		rn->rd->release();
		KRedirectNode *next = rn->next;
		klist_remove(rn);
		rbtree_remove(rd_map, rn->node);
		free(rn->name);
		delete rn;
		rn = next;
		//}
		//rn = rn->next;
	}
	lock.Unlock();
}
KRedirect *KCdnContainer::findRedirect(const char *name)
{
	rb_node *node = rbtree_find(rd_map, (void *)name, redirect_node_cmp);
	if (node == NULL) {
		return NULL;
	}
	KRedirectNode *rn = (KRedirectNode *)node->data;
	rn->lastActive = kgl_current_sec;
	klist_remove(rn);
	klist_append(&rd_list, rn);
	return rn->rd;
}
void KCdnContainer::addRedirect(KRedirectNode *rn)
{
	int new_flags = 0;
	rn->node = rbtree_insert(rd_map, (void *)rn->name, &new_flags,redirect_node_cmp);
	assert(new_flags);
	rn->lastActive = kgl_current_sec;
	klist_append(&rd_list, rn);
	rn->node->data = rn;
}
