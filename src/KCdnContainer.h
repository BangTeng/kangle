#ifndef KCDNCONTAINER_H
#define KCDNCONTAINER_H
#include <map>
#include "global.h"
#include "KList.h"
#include "KSingleAcserver.h"
#include "KMultiAcserver.h"
#include "KMutex.h"
#include "rbtree.h"
struct KRedirectNode
{
	char *name;
	KRedirect *rd;
	rb_node *node;
	time_t lastActive;
	KRedirectNode *next;
	KRedirectNode *prev;
};
class KCdnContainer
{
public:
	KCdnContainer();
	~KCdnContainer();
	KFetchObject *get(const char *ip,const char *host,int port,const char *ssl,int life_time,Proto_t proto=Proto_http,bool isIp=false);
	KFetchObject *get(const char *name);
	KRedirect *refsRedirect(const char *ip,const char *host,int port,const char *ssl,int life_time,Proto_t proto,bool isIp=false);
	KRedirect *refsRedirect(const char *val);
	KMultiAcserver *refsMultiServer(const char *name);
	void flush(time_t nowTime);
private:
	KRedirect *findRedirect(const char *name);
	void addRedirect(KRedirectNode *rn);
	KMutex lock;
	KRedirectNode rd_list;
	rb_tree *rd_map;
};
extern KCdnContainer cdnContainer;
#endif

