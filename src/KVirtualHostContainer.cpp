#include "KVirtualHostContainer.h"
#include "KSubVirtualHost.h"
#include "KVirtualHost.h"
#include "KList.h"

static int vh_container_find_cmp(void *k, void *k2)
{
	unsigned char *s1 = (unsigned char *)k;
	KVirtualHostContainer *s2 = (KVirtualHostContainer *)k2;
	return memn2cmp(s1, (unsigned char *)s2->name);
}
KVirtualHostContainer::KVirtualHostContainer()
{
	memset(this,0,sizeof(KVirtualHostContainer));
}
KVirtualHostContainer::~KVirtualHostContainer()
{
	clear();
	if (name) {
		xfree(name);
	}
}
void KVirtualHostContainer::removeVirtualHost(KVirtualHost *vh)
{
	std::list<KSubVirtualHost *>::iterator it2;	
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		unbindVirtualHost((*it2));
	}
}
void KVirtualHostContainer::addVirtualHost(KVirtualHost *vh,kgl_bind_level level)
{
	std::list<KSubVirtualHost *>::iterator it2;
	for (it2 = vh->hosts.begin(); it2 != vh->hosts.end(); it2++) {
		if(!bindVirtualHost((*it2),level) ) {
			(*it2)->allSuccess = false;
		}
	}
}
static iterator_ret clearVirtualHostIterator(void *data,void *argv)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)data;
	delete vhc;
	return iterator_remove_continue;
}
void KVirtualHostContainer::clear()
{
	KBindVirtualHost *next;
	while (list) {
		next = list->next;
		delete list;
		list = next;
	}
	while (wide_list) {
		next = wide_list->next;
		delete wide_list;
		wide_list = next;
	}
	if (tree) {
		rbtree_iterator(tree,clearVirtualHostIterator,NULL);
		rbtree_destroy(tree);
		tree = NULL;
	}
}
static iterator_ret listDomainIterator(void *data,void *argv)
{
	KVirtualHostContainer *vhc = (KVirtualHostContainer *)data;
	inter_domain_iterator_arg *it = (inter_domain_iterator_arg*)argv;
	vhc->iterator(it);
	return iterator_continue;
}
char *build_domain_name(domain_t src,int len,bool wide)
{
	const unsigned char *stack[256];
	int stack_size = 0;
	const unsigned char **hot = stack;
	char *buf = (char *)malloc(len + 4);
	char *dst = buf;
	if (wide) {
		*dst++ = '*';
	}
	while (len>0) {
		unsigned char lable_len = src[0];		
		if (lable_len == 0) {
			break;
		}
		len -= (lable_len + 1);
		if (len <0) {
			xfree(buf);
			return NULL;
		}
		(*hot) = src;
		hot++;
		stack_size++;
		src += lable_len + 1;
	}
	while (stack_size-->0) {
		hot--;
		if (dst != buf) {
			*dst++ = '.';
		}
		const unsigned char *lable = *hot;		
		unsigned char lable_len = lable[0];
		lable++;
		while (lable_len-->0) {
			*dst++ = *lable++;
		}
	}
	*dst = '\0';
	return buf;
}
void iterator_call_back(inter_domain_iterator_arg *it,bool wide,void *vh)
{
	char *domain = build_domain_name(it->domain,it->domain_len,wide);
	it->it(it->arg,domain,vh);
	free(domain);
}
void KVirtualHostContainer::iterator(inter_domain_iterator_arg *it)
{	
	domain_t domain = NULL;
	domain_t orig_domain = it->domain;
	int orig_domain_len = it->domain_len;
	if (name) {
		int new_len = it->domain_len + *name + 1;
		domain = (domain_t)malloc(new_len + 1);
		if (it->domain) {
			memcpy(domain,it->domain,it->domain_len);
		}
		memcpy(domain+it->domain_len,name,*name+1);
		it->domain_len = new_len;
		it->domain = domain;
	}
	KBindVirtualHost *next = list;
	while (next) {
		iterator_call_back(it,false,next->svh);
		next = next->next;
	}
	next = wide_list;
	while (next) {
		iterator_call_back(it,true,next->svh);
		next = next->next;		
	}
	if (tree) {	
		rbtree_iterator(tree,listDomainIterator,it);		
	}
	if (domain) {
		free(domain);
	}
	it->domain = orig_domain;
	it->domain_len = orig_domain_len;
}
void KVirtualHostContainer::iterator(domain_iterator it,void *arg)
{
	inter_domain_iterator_arg it_arg;
	memset(&it_arg,0,sizeof(inter_domain_iterator_arg));
	it_arg.it = it;
	it_arg.arg = arg;
	this->iterator(&it_arg);
}
bool convert(const char *domain,bool &wide,domain_t buf,int buf_size) {
	if (*domain=='*') {
		wide = true;
		domain++;
	} else {
		wide = false;
	}
	return revert_hostname(domain,strlen(domain),buf,buf_size);
}
bool KVirtualHostContainer::bind(const char *domain,void *vh,kgl_bind_level level)
{
	unsigned char hostname[256];
	bool wide;
	if (!convert(domain,wide,hostname,sizeof(hostname))) {
		return false;
	}
	return add(hostname,wide,level,vh);
}
kgl_del_result KVirtualHostContainer::unbind(const char *domain,void *vh)
{
	unsigned char hostname[256];
	bool wide;
	if (!convert(domain,wide,hostname,sizeof(hostname))) {
		return kgl_del_failed;
	}
	return del(hostname,wide,vh);
}
void *KVirtualHostContainer::find(const char *domain)
{
	unsigned char hostname[256];
	bool wide;
	if (!convert(domain,wide,hostname,sizeof(hostname))) {
		return NULL;
	}
	return find(hostname);
}
query_vh_result KVirtualHostContainer::findVirtualHost(KSubVirtualHost **rq_svh,domain_t hostname)
{
	KSubVirtualHost *svh = (KSubVirtualHost *)find(hostname);
	if (svh==NULL) {
		return query_vh_host_not_found;
	}
	if ((*rq_svh)!=svh) {
		//���������仯��?���ϵ��ͷţ������µ�
		if (*rq_svh) {
			(*rq_svh)->release();
			(*rq_svh) = NULL;
		}
		svh->vh->addRef();
		(*rq_svh) = svh;
#ifdef ENABLE_VH_RS_LIMIT
		if (!svh->vh->addConnection()) {
			return query_vh_connect_limit;
		}
#endif
	}
	return query_vh_success;	
}
kgl_del_result KVirtualHostContainer::unbindVirtualHost(KSubVirtualHost *svh) {
	assert(svh->bind_host);
	return this->del(svh->bind_host,svh->wide,svh);
}
bool KVirtualHostContainer::bindVirtualHost(KSubVirtualHost *svh,kgl_bind_level level) {
	
	assert(svh->bind_host);
	return this->add(svh->bind_host,svh->wide,level,svh);
}
void *KVirtualHostContainer::find(domain_t name)
{
	if (!*name) {
		//@������
		if (list == NULL) {
			return NULL;
		}
		return list->svh;
	}
	if (tree) {
		rb_node *node = rbtree_find(tree, name, vh_container_find_cmp);
		if (node) {
			//��ȷ����
			name = name + *name + 1;
			KVirtualHostContainer *rn = (KVirtualHostContainer *)node->data;
			void *ret = rn->find(name);
			if (ret) {
				return ret;
			}
		}
	}
	if (wide_list) {
		//������
		return wide_list->svh;
	}
	return NULL;
}
bool KVirtualHostContainer::add(domain_t name,bool wide,kgl_bind_level level,void *svh)
{
	if (!*name) {		
		KBindVirtualHost *bl = new KBindVirtualHost(svh);
		if (level==kgl_bind_high) {
			if (wide) {
				bl->next = wide_list;
				wide_list = bl;
			} else {
				bl->next = list;
				list = bl;
			}
		} else {
			KBindVirtualHost **l;
			if (wide) {
				l = &wide_list;
			} else {
				l = &list;
			}
			if (level==kgl_bind_unique && *l!=NULL) {
				delete bl;
				return false;
			}
			KBindVirtualHost *last = *l;
			while (last && last->next) {
				last = last->next;
			}
			if (last) {
				last->next = bl;
			} else {
				*l = bl;
			}
		}
		return true;
	}
	KVirtualHostContainer *rn;
	int new_flag = 0;
	if (tree == NULL) {
		tree = rbtree_create();
	}
	rb_node *node = rbtree_insert(tree, name, &new_flag, vh_container_find_cmp);
	assert(node);
	if (new_flag) {
		rn = new KVirtualHostContainer;
		rn->name = (domain_t)malloc(*name + 1);
		memcpy(rn->name, name, *name + 1);
		node->data = rn;
	} else {
		rn = (KVirtualHostContainer *)node->data;
	}
	return rn->add(name + *name + 1,wide,level,svh);
}
kgl_del_result KVirtualHostContainer::del(domain_t name,bool wide,void *svh)
{
	if (!*name) {
		KBindVirtualHost **bl;
		if (wide) {
			bl = &wide_list;
		} else {
			bl = &list;
		}
		KBindVirtualHost *last = NULL;
		KBindVirtualHost *cur = *bl;
		while (cur) {
			if (cur->svh==svh) {
				if (last) {
					last->next = cur->next;
				} else {
					*bl = cur->next;
				}
				delete cur;
				if (this->isEmpty()) {
					return kgl_del_empty;
				}
				return kgl_del_success;
			}
			last = cur;
			cur = cur->next;
		}
		return kgl_del_failed;
	}
	if (tree == NULL) {
		return kgl_del_failed;
	}
	rb_node *node = rbtree_find(tree, name, vh_container_find_cmp);
	if (node == NULL) {
		return kgl_del_failed;
	}
	KVirtualHostContainer *rn = (KVirtualHostContainer *)node->data;
	kgl_del_result ret = rn->del(name + *name + 1, wide,svh);
	if (ret == kgl_del_empty) {
		delete rn;
		rbtree_remove(tree, node);
		if (tree->root.rb_node == NULL) {
			rbtree_destroy(tree);
			tree = NULL;
		}
		if (!isEmpty()) {
			ret = kgl_del_success;
		}
	}
	return ret;
}
