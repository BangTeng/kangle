#ifndef MSOCKET_KSOCKET_H
#define MSOCKET_KSOCKET_H
#include "kfeature.h"
#include "kforwin32.h"
KBEGIN_DECLS
#if     !defined(HAVE_SOCKLEN_T)
#if 0
#if     defined(_AIX41)
typedef size_t socklen_t;
#else
typedef int socklen_t;
#endif
#endif
#endif
#ifdef _WIN32 //for win32
//#define FD_SETSIZE	8192
//#include <Winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#define SHUT_RD     0
#define SHUT_WR     1
#define SHUT_RDWR   2
extern LPFN_ACCEPTEX lpfnAcceptEx;
extern LPFN_CONNECTEX lpfnConnectEx;
typedef BOOL(WINAPI *fCancelIoEx)(
	__in      HANDLE hFile,
	__in_opt  LPOVERLAPPED lpOverlapped
	);
extern fCancelIoEx pCancelIoEx;
INLINE BOOL ksocket_cancel(SOCKET sockfd) {
	if (pCancelIoEx) {
		return pCancelIoEx((HANDLE)sockfd, NULL);
	} else {
		return CancelIo((HANDLE)sockfd);
	}
}
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#define BSD_COMP
#include <sys/ioctl.h>
#define SOCKET  	int
#define INVALID_SOCKET  -1
#define closesocket close
#endif
#define KSOCKET_ONLY_IPV4         0
#define KSOCKET_PROTO_AUTO        1
#define KSOCKET_ONLY_IPV6         2
#define KSOCKET_REUSEPORT         4
#ifdef ENABLE_TPROXY
#define KSOCKET_TPROXY            8
#endif
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN	46
#endif
#define MAXIPLEN			INET6_ADDRSTRLEN


#ifdef KSOCKET_IPV6
typedef struct ip_addr_s ip_addr;

struct ip_addr_s {
	union {
		uint8_t addr8[16];
		uint16_t addr16[8];
		uint32_t addr32[4];
	} data;
	uint16_t sin_family;

#define addr8				data.addr8
#define addr16              data.addr16
#define addr32              data.addr32

#ifdef __cplusplus
	bool operator <(const ip_addr &a) const {
			if (sin_family < a.sin_family) {
				return true;
		} else if (sin_family > a.sin_family) {
			return false;
		}
		if (sin_family == PF_INET) {
			return addr32[0] < a.addr32[0];
		}
		return (memcmp(addr8, a.addr8, sizeof(addr8)) < 0);
	}
	ip_addr operator &(const ip_addr &a) const {
		ip_addr b;
		b.sin_family = sin_family;
		if (sin_family == PF_INET) {
			b.addr32[0] = addr32[0] & a.addr32[0];
			return b;
		}
		for (int i = 0; i < 4; i++) {
			b.addr32[i] = (addr32[i] & a.addr32[i]);
		}
		return b;
	}
	bool operator==(const ip_addr &a) const {
		if (sin_family != a.sin_family) {
			return false;
		}
		if (sin_family == PF_INET) {
			return addr32[0] == a.addr32[0];
		}
		return memcmp(addr8, a.addr8, sizeof(addr8)) == 0;
	}
	unsigned get_hash()
	{
		unsigned h = 0;
		for (int i = 0; i < 4; i++) {
			h ^= addr32[i];
		}
		return h;
	}
#endif
};
#else
#define ip_addr		uint32_t
#endif

typedef union {
	struct sockaddr_in v4;
#ifdef KSOCKET_IPV6
	struct sockaddr_in6 v6;
#endif
} sockaddr_i;
INLINE int ksocket_addr_compare(const sockaddr_i *a, const sockaddr_i *b)
{
#ifdef KSOCKET_IPV6
	int ret = (int)a->v4.sin_family - (int)b->v4.sin_family;
	if (ret != 0) {
		return ret;
	}
	if (a->v4.sin_family == PF_INET)
#endif
	return (int)a->v4.sin_addr.s_addr - (int)b->v4.sin_addr.s_addr;
#ifdef KSOCKET_IPV6
	return memcmp(&a->v6.sin6_addr, &b->v6.sin6_addr, sizeof(a->v6.sin6_addr));
#endif
}
INLINE void ksocket_ipaddr_and(ip_addr *a, ip_addr *mask,ip_addr *ret)
{
	ret->sin_family = a->sin_family;
	if (ret->sin_family == PF_INET) {
		ret->addr32[0] = a->addr32[0] & mask->addr32[0];
		return;
	}
	for (int i = 0; i < 4; i++) {
		ret->addr32[i] = (a->addr32[i] & mask->addr32[i]);
	}
	return;
}
INLINE unsigned ksocket_addr_hash(const sockaddr_i *addr)
{
#ifdef KSOCKET_IPV6
	if (addr->v4.sin_family == PF_INET)
#endif
		return addr->v4.sin_addr.s_addr;
#ifdef KSOCKET_IPV6
	ip_addr *ia = (ip_addr *)(&addr->v6.sin6_addr);
	unsigned h = 0;
	for (int i = 0; i < 4; i++) {
		h ^= ia->addr32[i];
	}
	return h;
#endif
}
INLINE int ksocket_ipaddr_compare(ip_addr *a, ip_addr *b)
{
#ifdef KSOCKET_IPV6
	int ret = (int)a->sin_family - (int)b->sin_family;
	if (ret!=0) {
		return ret;
	}
	if (a->sin_family == PF_INET) {
		return (int)a->addr32[0] - (int)b->addr32[0];
	}
	return memcmp(a->addr8, b->addr8, sizeof(a->addr8));
#else
	return (int)*a - (int)*b;
#endif
}
INLINE int ksocket_addr_len(const sockaddr_i *addr) {
	switch (addr->v4.sin_family) {
	case  PF_INET:
		return sizeof(addr->v4);
	case PF_INET6:
		return sizeof(addr->v6);
#ifdef KSOCKET_UNIX
	case AF_UNIX:
		return sizeof(struct sockaddr_un);
#endif
	default:
		return sizeof(addr->v6);
	}
}
INLINE uint16_t ksocket_addr_port(const sockaddr_i *addr)
{
	switch (addr->v4.sin_family) {
	case PF_INET:
		return ntohs(addr->v4.sin_port);
	case PF_INET6:
		return ntohs(addr->v6.sin6_port);
	}
	return 0;
}
INLINE int ksocket_no_delay(SOCKET sockfd) {
#ifdef LINUX
	int flag = 0;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, (const void *)&flag, sizeof(int));
#elif BSD_OS
	int flag = 0;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_NOPUSH, (const void *)&flag, sizeof(int));
#else
	int flag = 1;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(int));
#endif
}
INLINE int ksocket_delay(SOCKET sockfd)
{
#ifdef LINUX
	int flag = 1;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, (const void *)&flag, sizeof(int));
#elif BSD_OS
	int flag = 1;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_NOPUSH, (const void *)&flag, sizeof(int));
#else
	int flag = 0;
	return setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (const char *)&flag, sizeof(int));
#endif
}
INLINE void ksocket_no_block(SOCKET sockfd) {
	int iMode = 1;
#ifdef _WIN32
	ioctlsocket(sockfd, FIONBIO, (u_long *)&iMode);
#else
	ioctl(sockfd, FIONBIO, &iMode);
#endif
}
INLINE void ksocket_block(SOCKET sockfd) {
	int iMode = 0;
#ifdef _WIN32
	ioctlsocket(sockfd, FIONBIO, (u_long*)&iMode);
#else
	ioctl(sockfd, FIONBIO, &iMode);
#endif
}
#define KSOCKET_ONLY_IPV4         0
#define KSOCKET_PROTO_AUTO        1
#define KSOCKET_ONLY_IPV6         2
#define KSOCKET_REUSEPORT         4
#ifdef ENABLE_TPROXY
#define KSOCKET_TPROXY            8
#endif
#define KSOCKET_PROTO_IPV4        KSOCKET_ONLY_IPV4
#define KSOCKET_PROTO_IPV6        KSOCKET_ONLY_IPV6   
void ksocket_startup();
void ksocket_library_startup();
void ksocket_clean();
#define ksocket_shutdown(sockfd,how) shutdown(sockfd,how)
#define ksocket_init(sockfd) (sockfd=INVALID_SOCKET)
#define ksocket_opened(sockfd) (sockfd!=INVALID_SOCKET)
#define ksocket_close(sockfd) closesocket(sockfd)
#ifdef KSOCKET_UNIX	
int ksocket_unix_addr(const char *path,struct sockaddr_un *addr);
INLINE const char *ksocket_unix_path(struct sockaddr_un *addr)
{
	return addr->sun_path;
}
#endif

void ksocket_ipaddr(const sockaddr_i *addr, ip_addr *ia);
SOCKET ksocket_listen(const sockaddr_i *addr,int flag);
SOCKET ksocket_listen_udp(const sockaddr_i *addr);
SOCKET ksocket_accept(SOCKET s,sockaddr_i *addr,bool no_block);
SOCKET ksocket_connect(const sockaddr_i *addr, const sockaddr_i *bind_addr, int tmo);
SOCKET ksocket_half_connect(const sockaddr_i *addr,const sockaddr_i *bind_addr, int tproxy_mask);
bool ksocket_getaddr(const char *host, uint16_t port,int ai_family, int ai_flags, sockaddr_i *addr);
void ksocket_addrinfo_sockaddr(struct addrinfo *ai, uint16_t port, sockaddr_i *addr);
bool ksocket_sockaddr_ip(const sockaddr_i *addr, char *ip, int ip_len);
bool ksocket_ipaddr_ip(const ip_addr *ia, char *ip, int ip_len);
bool ksocket_get_ipaddr(const char *host, ip_addr *ip);
bool wait_socket_event(SOCKET sockfd, bool is_write, int tmo);
INLINE bool ksocket_set_time(SOCKET sockfd, int snd_tmo, int recv_tmo)
{
	struct timeval msec;
#ifdef _WIN32
	msec.tv_sec = snd_tmo * 1000;
#else
	msec.tv_sec = snd_tmo;
#endif
	msec.tv_usec = 0;
	int ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *)&msec,sizeof(msec));
	if (ret != 0) {
		//debug("set send time_out error errno=%d\n", errno);
		return false;
	}
#ifdef _WIN32
	msec.tv_sec = recv_tmo * 1000;
#else
	msec.tv_sec = recv_tmo;
#endif
	ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&msec,
		sizeof(msec));
	if (ret != 0) {
		//debug("set recv time_out error errno=%d\n", errno);
		return false;
	}
	return true;

}

KEND_DECLS
#endif
