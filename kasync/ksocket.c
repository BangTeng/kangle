#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "ksocket.h"
#include "kfeature.h"
#include "kfile.h"
#include "kmalloc.h"
#ifdef _WIN32
LPFN_ACCEPTEX lpfnAcceptEx = NULL;
LPFN_CONNECTEX lpfnConnectEx = NULL;
fCancelIoEx pCancelIoEx = NULL;
#else
#include <poll.h>
#endif

#ifndef SUN_LEN
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif
void ksocket_library_startup()
{

#ifdef _WIN32
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	DWORD dwBytes;
	GUID m_guid = WSAID_CONNECTEX;
	int dwErr = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guid, sizeof(m_guid), &lpfnConnectEx, sizeof(lpfnConnectEx), &dwBytes, NULL, NULL);
	if (lpfnConnectEx == NULL) {
		//klog(KLOG_ERR,"Cann't find ConnectEx function\n");
	}
	GUID m_guid2 = WSAID_ACCEPTEX;
	dwErr = WSAIoctl(sock, SIO_GET_EXTENSION_FUNCTION_POINTER, &m_guid2, sizeof(m_guid2), &lpfnAcceptEx, sizeof(lpfnAcceptEx), &dwBytes, NULL, NULL);
	if (lpfnAcceptEx == NULL) {
		//klog(KLOG_ERR,"Cann't find AcceptEx function\n");
	}
	closesocket(sock);
	//windows vista开始才有CancelIoEx,所以要用动态
	pCancelIoEx = (fCancelIoEx)GetProcAddress(GetModuleHandle("kernel32.dll"), "CancelIoEx");
#endif

}
void ksocket_startup()
{
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2, 0);
	err = WSAStartup(wVersionRequested, &wsaData);
#endif
	ksocket_library_startup();
}
void ksocket_clean()
{
#ifdef _WIN32
	WSACleanup();
#endif
}
bool ksocket_ipaddr_ip(const ip_addr *ia, char *ip, int ip_len)
{
	sockaddr_i a;
	memset(&a, 0, sizeof(a));
#ifdef KSOCKET_IPV6
	a.v4.sin_family = ia->sin_family;
#else
	a.v4.sin_family = PF_INET;
#endif	
#ifdef KSOCKET_IPV6
	if (ia->sin_family == PF_INET6) {
		kgl_memcpy(&a.v6.sin6_addr, ia, sizeof(a.v6.sin6_addr));
	} else
#endif
		kgl_memcpy(&a.v4.sin_addr, ia, sizeof(a.v4.sin_addr));
	return ksocket_sockaddr_ip(&a, ip, ip_len);
}
bool ksocket_get_ipaddr(const char *host, ip_addr *ip) {
	sockaddr_i addr;
	if (!ksocket_getaddr(host, 0, AF_UNSPEC, AI_NUMERICHOST, &addr)) {
		return false;
	}
	ksocket_ipaddr(&addr, ip);
	return true;
}
void ksocket_ipaddr(const sockaddr_i *addr, ip_addr *to) {
#ifdef KSOCKET_IPV6
	to->sin_family = addr->v4.sin_family;
	if (addr->v4.sin_family == PF_INET) {
		to->addr32[0] = addr->v4.sin_addr.s_addr;
	} else {
		kgl_memcpy(&to->data, &addr->v6.sin6_addr, MIN(sizeof(to->data), sizeof(addr->v6.sin6_addr)));
	}
#else
	*to = addr->v4.sin_addr.s_addr;
#endif
}
SOCKET ksocket_listen_udp(const sockaddr_i *addr)
{
	SOCKET sockfd;
	if ((sockfd = socket(addr->v4.sin_family, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		//printf("errno=%d\n",WSAGetLastError());
		return INVALID_SOCKET;
	}
	kfile_close_on_exec((FILE_HANDLE)sockfd, true);
	int n = 1;
#ifndef _WIN32
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&n, sizeof(int));
#endif
	socklen_t addr_len = ksocket_addr_len(addr);
#if defined(KSOCKET_IPV6) 
	if (addr->v4.sin_family == PF_INET6) {	
#ifdef IPV6_V6ONLY
		setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&n, sizeof(int));
#endif
	}
#endif
	if (bind(sockfd, (struct sockaddr *)addr, addr_len) < 0) {
		ksocket_close(sockfd);
		return INVALID_SOCKET;
	}
	return sockfd;
}
SOCKET ksocket_listen(const sockaddr_i *addr,int flag)
{
#ifdef KSOCKET_UNIX	
	if (addr->v4.sin_family==AF_UNIX) {
		const char *unix_path = ksocket_unix_path((struct sockaddr_un *)addr);
		struct stat buf;
		int ret = stat(unix_path, &buf);
		if (ret==0 && S_ISSOCK(buf.st_mode)) {
			unlink(unix_path);
		}
	}
#endif
	SOCKET sockfd;
#ifdef SOCK_CLOEXEC
	sockfd = socket(addr->v4.sin_family, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (!ksocket_opened(sockfd)) {
		return sockfd;
	}
#else
	sockfd = socket(addr->v4.sin_family, SOCK_STREAM, 0);
	if (!ksocket_opened(sockfd)) {
		return sockfd;
	}
	kfile_close_on_exec((FILE_HANDLE)sockfd,true);
#endif
	int n = 1;
#ifndef _WIN32
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *)&n, sizeof(int));
#endif
#ifdef SO_REUSEPORT
	if (TEST(flag, KSOCKET_REUSEPORT)) {
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char *)&n, sizeof(int));
	}
#endif
#ifdef IPV6_V6ONLY
	if (TEST(flag, KSOCKET_ONLY_IPV6)) {
		setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *)&n, sizeof(int));
	}
#endif
#ifdef TCP_FASTOPEN
	if (TEST(flag, KSOCKET_FASTOPEN)) {
		setsockopt(sockfd, IPPROTO_TCP, TCP_FASTOPEN, (const void *)&n, sizeof(n));
	}
#endif
#ifdef IP_TRANSPARENT
#ifdef KSOCKET_TPROXY
	if (TEST(flag, KSOCKET_TPROXY)) {
		int value = 1;
		if (setsockopt(sockfd, SOL_IP, IP_TRANSPARENT, &value, sizeof(value)) < 0) {
			//debug("setsockopt IP_TRANSPARENT failed,errno=%d\n",errno);
		}
	}
#endif
#endif
	if (bind(sockfd, (struct sockaddr *)addr, ksocket_addr_len(addr)) < 0) {
		ksocket_close(sockfd);
		return INVALID_SOCKET;
	}
	if (listen(sockfd, -1) < 0) {
		ksocket_close(sockfd);
		return INVALID_SOCKET;
	}
	return sockfd;
}
#ifdef KSOCKET_UNIX	
int ksocket_unix_addr(const char *path,struct sockaddr_un *addr)
{
	memset(addr, 0, sizeof(struct sockaddr_un));
	addr->sun_family = AF_UNIX;
	strncpy(addr->sun_path, path, sizeof(addr->sun_path));
	return 0;
}
#endif
SOCKET ksocket_accept(SOCKET s, sockaddr_i *addr,bool no_block)
{
	socklen_t addr_size = sizeof(sockaddr_i);
#ifdef HAVE_ACCEPT4
	int flag = SOCK_CLOEXEC;
	if (no_block) {
		flag |= SOCK_NONBLOCK;
	}
	return accept4(s, (struct sockaddr *)addr, &addr_size, flag);
#else

	SOCKET sockfd = accept(s, (struct sockaddr *)addr, &addr_size);
	if (sockfd == INVALID_SOCKET) {
		return sockfd;
	}
	kfile_close_on_exec((FILE_HANDLE)sockfd, true);
	if (no_block) {
		ksocket_no_block(sockfd);
	}
	return sockfd;
#endif
}
void ksocket_addrinfo_sockaddr(struct addrinfo *ai, uint16_t port,sockaddr_i *addr)
{
	addr->v4.sin_family = ai->ai_family;
#ifdef KSOCKET_IPV6
	if (ai->ai_family == PF_INET6) {
		((struct sockaddr_in6 *)ai->ai_addr)->sin6_port = htons(port);
	} else
#endif
		((struct sockaddr_in *)ai->ai_addr)->sin_port = htons(port);
	int copy_len = MIN((socklen_t)ai->ai_addrlen, sizeof(sockaddr_i));
	kgl_memcpy(addr, ai->ai_addr, copy_len);
}
bool ksocket_getaddr(const char *host, uint16_t port,int ai_family, int ai_flags, sockaddr_i *addr)
{
	struct addrinfo *res;
#ifndef KSOCKET_IPV6
	ai_family = PF_INET;
#endif
	struct addrinfo f;
	memset(&f, 0, sizeof(f));
	f.ai_family = ai_family;
	f.ai_flags = ai_flags;
	int ret = getaddrinfo(host, NULL, &f, &res);
	if (ret != 0 || res == NULL) {
		//debug("ret=%d,res=%x,errno=%d %s\n", ret, res, errno, strerror(errno));
		return false;
	}
	ksocket_addrinfo_sockaddr(res, port, addr);
	freeaddrinfo(res);
	return true;
}
bool ksocket_sockaddr_ip(const sockaddr_i *sockaddr, char *ip, int ip_len)
{
	if (getnameinfo((struct sockaddr *)sockaddr, ksocket_addr_len(sockaddr), ip, ip_len,NULL, 0, NI_NUMERICHOST) != 0) {
		*ip = '\0';
		return false;
	}
	return true;
}
bool wait_socket_event(SOCKET sockfd, bool is_write, int tmo) {
	if (tmo <= 0) {
		return true;
	}
#ifdef HAVE_POLL
	struct pollfd poll_list;
	poll_list.fd = sockfd;
	if (is_write) {
		poll_list.events = POLLOUT;
	} else {
		poll_list.events = POLLIN | POLLPRI;
	}
	if (poll(&poll_list, 1, tmo * 1000) <= 0) {
		return false;
	}
	if (TEST(poll_list.revents, POLLERR)) {
		return false;
	}
#else
	if (sockfd < 0) {
		return false;
	}
	struct timeval tm;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);
	tm.tv_sec = tmo;
	tm.tv_usec = 0;
	if (select(sockfd + 1, ((!is_write) ? &fds : NULL),
		(is_write ? &fds : NULL), NULL, &tm) <= 0) {
		return false;
	}
#endif
	return true;
}
SOCKET ksocket_connect(const sockaddr_i *addr,const sockaddr_i *bind_addr,int tmo)
{
	SOCKET sockfd;
#ifdef SOCK_CLOEXEC
	if ((sockfd = socket(addr->v4.sin_family, SOCK_STREAM | SOCK_CLOEXEC , 0)) == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}
#else
	if ((sockfd = socket(addr->v4.sin_family, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}
	kfile_close_on_exec((FILE_HANDLE)sockfd, true);
#endif
	if (bind_addr && bind(sockfd, (struct sockaddr *)bind_addr, ksocket_addr_len(bind_addr)) < 0) {
		ksocket_close(sockfd);
		return INVALID_SOCKET;
	}
	if (connect(sockfd, (struct sockaddr *)addr, ksocket_addr_len(addr))<0) {
		ksocket_close(sockfd);
		return INVALID_SOCKET;
	}
	if (!wait_socket_event(sockfd, true, tmo)) {
		ksocket_close(sockfd);
		return INVALID_SOCKET;
	}
	return sockfd;
}
SOCKET ksocket_half_connect(const sockaddr_i *addr, const sockaddr_i *bind_addr, int tproxy_mask)
{
	SOCKET sockfd;
#ifdef SOCK_CLOEXEC
	if ((sockfd = socket(addr->v4.sin_family, SOCK_STREAM | SOCK_CLOEXEC| SOCK_NONBLOCK, 0)) == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}
#else
	if ((sockfd = socket(addr->v4.sin_family, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return INVALID_SOCKET;
	}
	kfile_close_on_exec((FILE_HANDLE)sockfd, true);
#endif
	if (bind_addr) {
#ifdef IP_TRANSPARENT
#ifdef KSOCKET_TPROXY
		if (tproxy_mask>0) {
			int value = 1;
			setsockopt(sockfd, SOL_IP, IP_TRANSPARENT, &value, sizeof(value));
#ifdef SO_MARK
			setsockopt(sockfd, SOL_SOCKET, SO_MARK, (const void *)&tproxy_mask, sizeof(int));
#endif
		}
#endif
#endif
		if (bind(sockfd, (struct sockaddr *)bind_addr,ksocket_addr_len(bind_addr)) < 0) {
			ksocket_close(sockfd);
			return INVALID_SOCKET;
		}
	}
#ifdef _WIN32
	else {
		sockaddr_i bindaddr;
		memset(&bindaddr, 0, sizeof(bindaddr));
		bindaddr.v4.sin_family = addr->v4.sin_family;
		if (bind(sockfd, (struct sockaddr *) &bindaddr, ksocket_addr_len(&bindaddr)) < 0) {
			ksocket_close(sockfd);
			return INVALID_SOCKET;
		}
	}
#else
#ifndef SOCK_CLOEXEC
	ksocket_no_block(sockfd);
#endif
	int rc = connect(sockfd, (struct sockaddr *)addr, ksocket_addr_len(addr));
	if (rc == -1) {
		int err = errno;
		if (err != EINPROGRESS) {
			ksocket_close(sockfd);
			return INVALID_SOCKET;
		}
	}
#endif
	return sockfd;
}