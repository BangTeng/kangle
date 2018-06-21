/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#include "global.h"
#include "KSocket.h"
#ifdef HAVE_POLL
#include <poll.h>
#endif
#include <string.h>
#include "forwin32.h"
#include "log.h"
//#include "utils.h"
//#include "KSockPoolHelper.h"
//#include "ssl_utils.h"
#include "malloc_debug.h"


#ifndef SUN_LEN
#define SUN_LEN(su) \
	(sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif
bool waitForRW(SOCKET sockfd, bool isWrite, int timeo) {
	if (timeo <= 0)
		return true;
#ifdef HAVE_POLL
	struct pollfd poll_list;
	poll_list.fd = sockfd;
	if (isWrite) {
		poll_list.events = POLLOUT;
	} else {
		poll_list.events = POLLIN | POLLPRI;
	}
	if (poll(&poll_list, 1, timeo * 1000) <= 0) {
		return false;
	}
	if (TEST(poll_list.revents ,POLLERR)) {
		return false;
	}
#else
	if (sockfd < 0) {
		return false;
	}
	struct timeval tm;
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(sockfd,&fds);
	tm.tv_sec = timeo;
	tm.tv_usec = 0;
	if(select(sockfd + 1,((!isWrite) ? &fds : NULL),
					(isWrite ? &fds : NULL), NULL, &tm) <=0) {
		return false;
	}
#endif
	return true;
}
void KSocket::init_socket() {
#ifdef _WIN32
	WORD wVersionRequested;
	WSADATA wsaData;
	int err;
	wVersionRequested = MAKEWORD(2,0);
	err = WSAStartup(wVersionRequested, &wsaData );
#endif


}
void KSocket::clean_socket() {
#ifdef _WIN32
	WSACleanup();
#endif
}
bool KSocket::make_ip(sockaddr_i *ip, char *ips, int ips_len) {
        socklen_t addr_len = sizeof(sockaddr_i);
#if defined(KSOCKET_IPV6)
        if (ip->v4.sin_family == PF_INET) {
                addr_len = sizeof(ip->v4);
        }
#endif
	if (getnameinfo((struct sockaddr *) ip, addr_len, ips, ips_len,
		NULL, 0, NI_NUMERICHOST) != 0) {
		ips[0] = '\0';
		return false;
	}
	return true;
}
bool KSocket::make_ip(ip_addr *ip,char *ips,int ips_len) {
	sockaddr_i a;
	memset(&a, 0, sizeof(a));
#ifdef KSOCKET_IPV6
	a.v4.sin_family = ip->sin_family;
#else
	a.v4.sin_family=PF_INET;
#endif	
#ifdef KSOCKET_IPV6
	if (ip->sin_family == PF_INET6) {
		memcpy(&a.v6.sin6_addr, ip, sizeof(a.v6.sin6_addr));
	} else
#endif
		memcpy(&a.v4.sin_addr, ip, sizeof(a.v4.sin_addr));
	return make_ip(&a, ips, ips_len);
}
u_short KSocket::getportinfo(sockaddr_i *m_a) {
#ifdef KSOCKET_IPV6
	if (m_a->v4.sin_family == PF_INET6) {
		return ntohs(m_a->v6.sin6_port);
	}
#endif
	return ntohs(m_a->v4.sin_port);
}
void KSocket::get_self_ip(char *ips,size_t ips_len)
{
	 sockaddr_i s_sockaddr;
        socklen_t addr_len = sizeof(sockaddr_i);
        ::getsockname(sockfd, (struct sockaddr *) &s_sockaddr, &addr_len);
	make_ip(&s_sockaddr,ips,ips_len);
}
u_short KSocket::get_self_port() {
	sockaddr_i s_sockaddr;
	socklen_t addr_len = sizeof(s_sockaddr);
	::getsockname(sockfd, (struct sockaddr *) &s_sockaddr, &addr_len);
#ifdef KSOCKET_IPV6
	if (s_sockaddr.v4.sin_family == PF_INET6) {
		return ntohs(s_sockaddr.v6.sin6_port);
	}
#endif
	return ntohs(s_sockaddr.v4.sin_port);
}
;
u_short KClientSocket::get_remote_port() {
#ifdef KSOCKET_IPV6
	if (addr.v4.sin_family == PF_INET6) {
		return ntohs(addr.v6.sin6_port);
	}
#endif
	return ntohs(addr.v4.sin_port);
}
void KSocket::get_addr(const sockaddr_i *addr, ip_addr *to) {
#ifdef KSOCKET_IPV6
	to->sin_family = addr->v4.sin_family;
	if (addr->v4.sin_family == PF_INET) {
		to->addr32[0] = addr->v4.sin_addr.s_addr;
	} else {
		memcpy(&to->data, &addr->v6.sin6_addr,MIN(sizeof(to->data),sizeof(addr->v6.sin6_addr)));
	}
#else
	*to = addr->v4.sin_addr.s_addr;
#endif

}
void KClientSocket::get_remote_addr(ip_addr *to) {
	return get_addr(&addr, to);
}
void KSocket::get_self_addr(sockaddr_i *m_adr) {
	socklen_t addr_len = sizeof(addr);
	::getsockname(sockfd, (struct sockaddr *) m_adr, &addr_len);
}
int KClientSocket::writev(iovec *v,int vc,bool isSSL)
{
#ifdef HAVE_WRITEV
	if (!isSSL) {
		return ::writev(sockfd,v,vc);
	}
#endif
	int got = 0;
	for (int i=0;i<vc;i++) {
		char *hot = (char *)v[i].iov_base;
		int len = v[i].iov_len;
		while (len>0) {
			int this_write = this->write(hot,len);
			if (this_write<=0) {
				return (got>0?got:this_write);
			}
			got += this_write;
			len -= this_write;
			hot += this_write;
		}
	}
	return got;
}
int KClientSocket::write(const char *str, int len) {
	return ::send(sockfd, str, len, 0);
}
int KClientSocket::read(char *str, int len) {
	return ::recv(sockfd, str, len, 0);
}
bool KSocket::getaddr(const char *host, ip_addr *ip) {
	struct addrinfo *res;
	struct addrinfo f;
	memset(&f, 0, sizeof(f));
#ifdef KSOCKET_IPV6
	f.ai_family = PF_UNSPEC;
#else
	f.ai_family = PF_INET;
#endif
	f.ai_flags = AI_NUMERICHOST;	
	int ret = getaddrinfo(host, NULL, &f, &res);
	if (ret != 0 || res == NULL) {
		//debug("ret=%d,res=%x,errno=%d %s\n", ret, res, errno, strerror(errno));
		return false;
	}
#ifdef KSOCKET_IPV6	
	if (res->ai_family == PF_INET6) {
		struct sockaddr_in6 *in = (struct sockaddr_in6 *)res->ai_addr;
		memcpy(ip->addr32,&in->sin6_addr,16);
	} else {
		struct sockaddr_in *in = (struct sockaddr_in *)res->ai_addr;
		ip->addr32[0] = in->sin_addr.s_addr;
	}
	ip->sin_family = res->ai_family;
#else
	*ip = ((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr;
#endif
	freeaddrinfo(res);
	return true;
}
static struct addrinfo *getaddr(const char *host, int ai_family, int ai_flags)
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
		return NULL;
	}
	return res;
}
bool KSocket::getaddr(const char *host, int port, sockaddr_i *m_a,int ai_family,int ai_flags) {
	struct addrinfo *res;
#ifndef KSOCKET_IPV6
	ai_family = PF_INET;
#endif
	struct addrinfo f;
	memset(&f, 0, sizeof(f));
	f.ai_family = ai_family;
	f.ai_flags = ai_flags;
	int ret = getaddrinfo(host, NULL,&f, &res);
	if (ret != 0 || res == NULL) {
		//debug("ret=%d,res=%x,errno=%d %s\n", ret, res, errno, strerror(errno));
		return false;
	}
#ifdef KSOCKET_IPV6
	if (res->ai_family == PF_INET6) {
		((sockaddr_in6 *) res->ai_addr)->sin6_port = htons(port);
	} else
#endif
		((sockaddr_in *) res->ai_addr)->sin_port = htons(port);
	memcpy(m_a, res->ai_addr, MIN(res->ai_addrlen,sizeof(sockaddr_i)));
	freeaddrinfo(res);
	return true;
}
bool KClientSocket::connect(sockaddr_i &m_adr, int tmo, sockaddr_i *bind_addr) {
#ifdef SOCK_CLOEXEC
	if ((sockfd = socket(m_adr.v4.sin_family, SOCK_STREAM | SOCK_CLOEXEC, 0)) == INVALID_SOCKET) {
		return false;
	}
#else
	if ((sockfd = socket(m_adr.v4.sin_family, SOCK_STREAM, 0))	== INVALID_SOCKET) {
		return false;
	}
	setCloseOnExec();
#endif
	if (bind_addr) {
		int addr_len = bind_addr->get_addr_len();
		if (::bind(sockfd, (struct sockaddr *) bind_addr, addr_len) < 0) {
			return false;
		}
	}
	memcpy(&addr, &m_adr, sizeof(addr));
	int addr_len = addr.get_addr_len();
	if (connect(sockfd, (struct sockaddr *) (&addr), addr_len, tmo) < 0) {
		return false;
	}
	return true;
}
#ifdef KSOCKET_UNIX
bool KClientSocket::connect(const char *unixfile,int tmo)
{
	struct sockaddr_un sun2;
#ifdef SOCK_CLOEXEC
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC, 0)) < 0) {
                return false;
	}
#else
        if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
                return false;
	}
	setCloseOnExec();
#endif
	memset(&sun2, 0, sizeof(struct sockaddr_un));
        sun2.sun_family = AF_UNIX;
        strncpy(sun2.sun_path, unixfile, sizeof(sun2.sun_path));
	int addr_len = sizeof(sun2);
	if (connect(sockfd, (struct sockaddr *) (&sun2), addr_len, tmo) < 0) {
                return false;
        }
        return true;
}
bool KClientSocket::halfconnect(const char *unixfile)
{
	struct sockaddr_un sun2;
#ifdef SOCK_CLOEXEC
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK, 0)) < 0) {
		return false;
	}
#else
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		return false;
	}
	setCloseOnExec();
	::setnoblock(sockfd);
#endif
	memset(&sun2, 0, sizeof(struct sockaddr_un));
	sun2.sun_family = AF_UNIX;
	strncpy(sun2.sun_path, unixfile, sizeof(sun2.sun_path));
	::connect(sockfd, (struct sockaddr *)&sun2, SUN_LEN(&sun2));
	return true;
}
#endif
bool KClientSocket::halfconnect(sockaddr_i *bind_addr,bool tproxy)
{
#ifdef SOCK_CLOEXEC
	if ((sockfd = socket(addr.v4.sin_family, SOCK_STREAM | SOCK_CLOEXEC, 0)) == INVALID_SOCKET) {
		return false;
	}
#else
	if ((sockfd = socket(addr.v4.sin_family, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return false;
	}
	setCloseOnExec();
#endif
	if (bind_addr) {
		int addr_len = bind_addr->get_addr_len();
#ifdef IP_TRANSPARENT
#ifdef KSOCKET_TPROXY
		if (tproxy) {
			int value = 1;
			setsockopt(sockfd, SOL_IP, IP_TRANSPARENT, &value, sizeof(value));
			set_mask(8);
		}
#endif
#endif
		if (::bind(sockfd, (struct sockaddr *) bind_addr, addr_len) < 0) {
			return false;
		}
	}
#ifdef _WIN32
	else {
		sockaddr_i bindaddr;
		memset(&bindaddr,0,sizeof(bindaddr));
		bindaddr.v4.sin_family = addr.v4.sin_family;
		int addr_len = bindaddr.get_addr_len();
		if (::bind(sockfd, (struct sockaddr *) &bindaddr, addr_len) < 0) {
			int err = WSAGetLastError();
			klog(KLOG_ERR,"cann't bind addr,errno=%d\n",err);
			return false;
		}
	}
#else
	int addr_len = addr.get_addr_len();
	::setnoblock(sockfd);
	int rc = ::connect(sockfd, (struct sockaddr *) (&addr), addr_len);
	if (rc==-1) {
		int err = errno;
		if (err!=EINPROGRESS) {
			klog(KLOG_ERR,"cann't connect sockfd=%d,errno=%d\n",sockfd,errno);
			return false;
		}
	}
#endif
	return true;
}
bool KClientSocket::halfconnect(const char *host, int port,int ai_family,sockaddr_i *bind_addr,bool tproxy)
{
	if (!getaddr(host, port, &addr,ai_family)) {
		return false;
	}
	return halfconnect(bind_addr,tproxy);
}
bool KClientSocket::halfconnect(sockaddr_i &m_adr,sockaddr_i *bind_addr,bool tproxy)
{
	memcpy(&addr, &m_adr, sizeof(addr));
	return halfconnect(bind_addr,tproxy);
	
}
bool KClientSocket::connect(const char *host, int port, int tmo, sockaddr_i *bind_addr) {
	sockaddr_i sock_addr;
	if (!getaddr(host, port, &sock_addr)) {
		return false;
	}
	return connect(sock_addr, tmo, bind_addr);
}
int KClientSocket::connect(SOCKET sockfd, const struct sockaddr *serv_addr,socklen_t addrlen, int tmo) {
	if (tmo == 0) {
		return ::connect(sockfd, serv_addr, addrlen);
	}
	::setnoblock(sockfd);
	::connect(sockfd, serv_addr, addrlen);
	::setblock(sockfd);
	if (!waitForRW(sockfd, true, tmo)) {
		return -1;
	}
	return 0;
}
bool KServerSocket::open4(int port, const char * ip,bool tproxy) {
	int flag = KSOCKET_ONLY_IPV4;
#ifdef KSOCKET_TPROXY
	if (tproxy) {
		flag |= KSOCKET_TPROXY;
	}
#endif
	return open(port,ip,flag);
}
bool KServerSocket::open(int port, const char * ip,int flag) {
	int proto = flag & 0x7;
	if (ip != NULL) {
		if (!getaddr(ip, port, &addr,0,AI_NUMERICHOST)) {
			debug("cann't get addr=%s:%d\n", ip, port);
			return false;
		}
		if (proto == KSOCKET_ONLY_IPV4 && addr.v4.sin_family!=PF_INET) {
			return false;
		}
		if (proto==KSOCKET_ONLY_IPV6 && addr.v4.sin_family!=PF_INET6) {
			return false;
		}
	} else {
		memset(&addr, 0, sizeof(addr));
#ifdef KSOCKET_IPV6
		if (proto!=KSOCKET_ONLY_IPV4) {
			addr.v6.sin6_family = AF_INET6;
			addr.v6.sin6_port = htons(port);
		} else {
			addr.v4.sin_family = AF_INET;
			addr.v4.sin_port = htons(port);
		}
		if (proto==KSOCKET_PROTO_AUTO) {
			if (!listen(flag)) {
				//try ipv4 listen
				addr.v4.sin_family = AF_INET;
				addr.v4.sin_port = htons(port);
				return listen(flag);
			}
			return true;
		}

#else
		addr.v4.sin_family = AF_INET;
		addr.v4.sin_port = htons(port);
#endif
	}
	return listen(flag);
}
#ifdef KSOCKET_IPV6
bool KServerSocket::open6(int port, const char *ip,bool tproxy) {
	int flag = KSOCKET_ONLY_IPV6;
#ifdef KSOCKET_TPROXY
	if (tproxy) {
		flag |= KSOCKET_TPROXY;
	}
#endif
	return open(port,ip,flag);
}
#endif
bool KServerSocket::listen(int flag) {
#ifdef SOCK_CLOEXEC
	if ((sockfd = socket(addr.v4.sin_family, SOCK_STREAM|SOCK_CLOEXEC, 0)) == INVALID_SOCKET) {
		return false;
	}
#else
	if ((sockfd = socket(addr.v4.sin_family, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		return false;
	}
	setCloseOnExec();
#endif
	int n = 1;
#ifndef _WIN32
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &n, sizeof(int));
#endif
#ifdef SO_REUSEPORT
	if (TEST(flag, KSOCKET_REUSEPORT)) {
		setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (const char *)&n, sizeof(int));
	}
#endif
#ifdef IPV6_V6ONLY
	if (TEST(flag,KSOCKET_ONLY_IPV6)) {
		setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *) &n, sizeof(int));
	}
#endif
	int addr_len = addr.get_addr_len();
#ifdef IP_TRANSPARENT
#ifdef KSOCKET_TPROXY
	if (TEST(flag,KSOCKET_TPROXY) ){
		int value = 1;
		if (setsockopt(sockfd, SOL_IP, IP_TRANSPARENT, &value, sizeof(value))<0) {
			//debug("setsockopt IP_TRANSPARENT failed,errno=%d\n",errno);
		}
	}
#endif
#endif
	if (::bind(sockfd, (struct sockaddr *) &addr, addr_len) < 0) {
		//debug("bind socket failed,errno=%d.\n", errno);
		return false;
	}
	if (::listen(sockfd, -1) < 0) {
		//debug("listen socket failed,errno=%d.\n", errno);
		return false;
	}
	return true;
}
#ifdef KSOCKET_UNIX	
bool KUnixServerSocket::open(const char *path)
{
	int flags = 1;
	struct sockaddr_un sun2;
	unlink(path);
#ifdef SOCK_CLOEXEC
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0)) == INVALID_SOCKET) {
		return false;
	}
#else
	if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
		return false;
	}
#endif
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &flags, sizeof(flags));
	memset(&sun2, 0, sizeof(struct sockaddr_un));
	sun2.sun_family = AF_UNIX;
	strncpy(sun2.sun_path, path, sizeof(sun2.sun_path));
	int addr_len = sizeof(sun2);
	if (::bind(sockfd, (struct sockaddr *) &sun2, addr_len) < 0) {
		return false;
	}
	return ::listen(sockfd, 4096) >= 0;
}
#endif
