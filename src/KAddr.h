#ifndef KDNS_H_LSJ1JJJ
#define KDNS_H_LSJ1JJJ
#include "global.h"
#include "KMutex.h"
#include "KString.h"
#include "KSocket.h"
class KSelector;
enum kgl_addr_type {
	kgl_addr_ip,
	kgl_addr_cname
};
class KAddr {
public:
	KAddr(struct addrinfo *addr)
	{
		refs = 1;
		this->addr = addr;
	}
	void release()
	{
		lock.Lock();
		refs--;
		if (refs == 0) {
			lock.Unlock();
			delete this;
			return;
		}
		lock.Unlock();
	}
	void add_refs()
	{
		lock.Lock();
		refs++;
		lock.Unlock();
	}
	bool get_addr(int port, sockaddr_i &addr)
	{
		memcpy(&addr, this->addr->ai_addr, MIN(this->addr->ai_addrlen, sizeof(sockaddr_i)));
#ifdef KSOCKET_IPV6
		if (this->addr->ai_family == PF_INET6) {
			addr.v6.sin6_port = htons(port);
			return true;
		}
#endif
		addr.v4.sin_port = htons(port);		
		return true;
	}
	bool get_cname(const char *hostname,KStringBuf &s)
	{
		char *cname = NULL;		
		if (addr->ai_canonname && strcasecmp(addr->ai_canonname, hostname) != 0) {
			cname = addr->ai_canonname;
		}		
		if (cname) {
			s << hostname;
			s.WSTR(".");
			s << cname;
			return true;
		}
		return false;
	}
	struct addrinfo *get_addr()
	{
		return addr;
	}
private:
	struct addrinfo *addr;
	int refs;
	KMutex lock;
	~KAddr()
	{
		if (addr != NULL) {
			freeaddrinfo(addr);
		}
	}
};
typedef void(*addr_call_back)(void *arg, KAddr *addr);
void find_addr(const char *hostname, kgl_addr_type addr_type, addr_call_back cb, void *arg,KSelector *selector);
KAddr *find_cache_addr(const char *hostname, kgl_addr_type addr_type);
void init_addr_worker();
void flush_addr_cache(time_t nowTime);
int get_addr_cache_count();
#endif
