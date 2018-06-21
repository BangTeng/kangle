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
#ifndef KSocket_h_sdlkfjs8723khj1k2j3
#define KSocket_h_sdlkfjs8723khj1k2j3
#include "global.h"
//#ifndef DISABLE_IPV6
//#define KSOCKET_IPV6	1
//#endif
#if     !defined(HAVE_SOCKLEN_T)
#if     defined(_AIX41)
typedef size_t socklen_t;
#else
typedef int socklen_t;
#endif
#endif  /* !HAVE_SOCKLEN_T */
#ifndef MIN
#define MIN(a,b)        (((a)<(b))?(a):(b))
#endif
#ifdef _WIN32 //for win32
//#define FD_SETSIZE	8192
//#include <Winsock2.h>
#include<ws2tcpip.h>
#include <mswsock.h>
//#include<tpipv6.h>
#define u_int8_t	unsigned char
#define u_int16_t	unsigned short
#define u_int32_t	unsigned int
#define SHUT_RD     0
#define SHUT_WR     1
#define SHUT_RDWR   2

#ifndef bzero
#define bzero(X,Y)      memset(X,0,Y)
#endif
#define close2(X)       closesocket(X)
#else   //for linux
#define SOCKET  int

#define INVALID_SOCKET  -1
#ifndef HTTP_PROXY
#define KSOCKET_UNIX   1
#endif
#include <stdlib.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#define BSD_COMP
#include <sys/ioctl.h>
#include <syslog.h>
#include <errno.h>
#define close2(X)       close(X)
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#endif
#ifdef SOLARIS
#include <sys/filio.h>
#endif
#include <string>
#include <time.h>
#include <string.h>
#include <stdio.h>
#include "KStream.h"
#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN	46
#endif
#define MAXIPLEN			INET6_ADDRSTRLEN
#ifdef _WIN32
typedef BOOL (WINAPI *fCancelIoEx)(
  __in      HANDLE hFile,
  __in_opt  LPOVERLAPPED lpOverlapped
);
extern fCancelIoEx pCancelIoEx;
inline bool setCloseOnExec(HANDLE fd, bool closeExec) {
#ifndef _WIN32
	//return fcntl(fd, F_SETFD, (closeExec ? FD_CLOEXEC : 0)) == 0;
#else
	if (SetHandleInformation(fd, HANDLE_FLAG_INHERIT, (closeExec?0:HANDLE_FLAG_INHERIT)) == 0) {
		return false;
	}
#endif
	return true;
}
#endif
#ifdef KSOCKET_IPV6
struct ip_addr {
	union {
		u_int8_t addr8[16];
		u_int16_t addr16[8];
		u_int32_t addr32[4];
	} data;
	u_short sin_family;
#define addr8				data.addr8
#define addr16              data.addr16
#define addr32              data.addr32

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
		for(int i=0;i<4;i++){
			h^=addr32[i];
		}
		return h;
	}
};
#else
#define ip_addr		u_int32_t
#endif

union sockaddr_i {
	struct sockaddr_in v4;
#ifdef KSOCKET_IPV6
	struct sockaddr_in6 v6;
#endif
#ifdef _WIN32
	SOCKADDR_STORAGE vwin;
#endif
	int get_addr_len()
	{
#ifdef KSOCKET_IPV6
		if (v4.sin_family == PF_INET)
			return sizeof(v4);
		return sizeof(v6);
#endif
		return sizeof(v4);
	}
	bool operator <(const sockaddr_i &a) const {
#ifdef KSOCKET_IPV6
		if (v4.sin_family < a.v4.sin_family) {
			return true;
		} else if (v4.sin_family > a.v4.sin_family) {
			return false;
		}
		if (v4.sin_family == PF_INET)
#endif
			return v4.sin_addr.s_addr < a.v4.sin_addr.s_addr;
#ifdef KSOCKET_IPV6
		return memcmp(&v6.sin6_addr, &a.v6.sin6_addr, sizeof(in6_addr)) < 0;
#endif
	}
	bool operator ==(const sockaddr_i &a) const {
#ifdef KSOCKET_IPV6
		if (v4.sin_family != a.v4.sin_family) {
			return false;
		}
		if (v4.sin_family == PF_INET)
#endif
			return v4.sin_addr.s_addr == a.v4.sin_addr.s_addr;
#ifdef KSOCKET_IPV6
		return memcmp(&v6.sin6_addr, &a.v6.sin6_addr, sizeof(in6_addr)) == 0;
#endif
	}
	unsigned get_hash()
	{
#ifdef KSOCKET_IPV6
		if (v4.sin_family == PF_INET)
#endif
			return v4.sin_addr.s_addr;
#ifdef KSOCKET_IPV6
		ip_addr *addr = (ip_addr *)(&v6.sin6_addr);
		return addr->get_hash();
#endif
	}
	u_short get_port() {
#ifdef KSOCKET_IPV6
		if (v4.sin_family == PF_INET6) {
			return ntohs(v6.sin6_port);
		}
#endif
		return ntohs(v4.sin_port);
	}
};

inline void setnoblock(SOCKET sockfd) {
	int iMode = 1;
#ifdef _WIN32
	ioctlsocket(sockfd, FIONBIO, (u_long *)&iMode);
#else
	ioctl(sockfd, FIONBIO, &iMode);
#endif
}
inline void setblock(SOCKET sockfd) {
	int iMode = 0;
#ifdef _WIN32
	ioctlsocket(sockfd, FIONBIO, (u_long*) &iMode);
#else
	ioctl(sockfd, FIONBIO, &iMode);
#endif
}
class KSocket {
public:
	inline KSocket() {
		sockfd = INVALID_SOCKET;
	#ifndef NDEBUG
		blockFlag = true;
		shutdownFlag = false;
	#endif
	}
	inline ~KSocket() {
		close();
	}
	inline void close() {
		::close2(sockfd);
		sockfd = INVALID_SOCKET;
	}
	int send(const char *str, int len) {
		return ::send(sockfd, str, len, 0);
	}
	int sendev(iovec *v,int vc)
	{
#ifdef HAVE_WRITEV
		return ::writev(sockfd,v,vc);
#else
		int got = 0;
		for (int i=0;i<vc;i++) {
			char *hot = (char *)v[i].iov_base;
			int len = v[i].iov_len;
			while (len>0) {
				int this_write = this->send(hot,len);
				if (this_write<=0) {
					return (got>0?got:this_write);
				}
				got += this_write;
				len -= this_write;
				hot += this_write;
			}
		}
		return got;
#endif
	}
	int recv(char *str, int len) {
		return ::recv(sockfd, str, len, 0);
	}

	inline bool shutdown(int how) {
#ifndef NDEBUG
		shutdownFlag = true;
#endif
		return ::shutdown(sockfd, how) == 0;
	}
#ifdef _WIN32
	inline BOOL cancelIo()
	{
		if (pCancelIoEx) {
			return pCancelIoEx((HANDLE)sockfd,NULL);
		} else {
			return CancelIo((HANDLE)sockfd);
		}
	}
#endif
	int set_mask(int mask)
	{
#ifdef SO_MARK
		return setsockopt(sockfd, SOL_SOCKET,SO_MARK,(const void *) &mask, sizeof(int));
#endif
		return -1;
	}
	inline SOCKET get_socket() {
		return sockfd;
	}
	void set_socket(SOCKET sockfd)
	{
		this->sockfd = sockfd;
	}
	void update_remote_addr()
	{
		socklen_t addr_len = sizeof(sockaddr_i);
		::getpeername(sockfd, (struct sockaddr *) &addr, &addr_len);
	}
	void get_self_ip(char *ips,size_t ips_len);
	//@deprecated
	//std::string get_self_ip();
	u_short get_self_port();
	void get_self_addr(sockaddr_i *m_adr);

	inline 	void setCloseOnExec() {
	#ifdef _WIN32
		::setCloseOnExec((HANDLE)sockfd, true);
	#endif
	}
#ifndef NDEBUG
	bool isBlock()
	{
		return blockFlag;
	}
#endif
public:
	sockaddr_i addr;
	friend class KServerSocket;
public:
	static void get_addr(const sockaddr_i *addr, ip_addr *to);
	inline void set_nodelay() {
#ifndef NDEBUG
		delay = false;
#endif
#ifdef LINUX
		int flag = 0;
		setsockopt(sockfd, IPPROTO_TCP, TCP_CORK,(const void *) &flag, sizeof(int));
#elif BSD_OS
		int flag = 0;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NOPUSH,(const void *) &flag, sizeof(int));
#else
		int flag = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,(const char *) &flag, sizeof(int));
#endif
	}
	inline void set_delay()
	{
#ifndef NDEBUG
		delay = true;
#endif
#ifdef LINUX
		int flag = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_CORK,(const void *) &flag, sizeof(int));
#elif BSD_OS
		int flag = 1;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NOPUSH,(const void *) &flag, sizeof(int));
#else
		int flag = 0;
		setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,(const char *) &flag, sizeof(int));
#endif
	}
	inline void setnoblock() {
#ifndef NDEBUG
		blockFlag = false;
#endif
		::setnoblock(sockfd);
	}
	inline void setblock() {
#ifndef NDEBUG
		blockFlag = true;
#endif
		::setblock(sockfd);
	}

//	static int setnoblock(SOCKET sockfd);
//	static int setblock(SOCKET sockfd);
//	static void setnodelay(SOCKET sockfd);
	/*
	 * flag = 0 wait read
	 * flag = 1 wait write
	 */
	//	static bool WaitForReadWrite(SOCKET sockfd, int flag, int timeo);
	static struct addrinfo *getaddr(const char *host, int ai_family = AF_UNSPEC, int ai_flags = 0);
	static bool getaddr(const char *host, int port, sockaddr_i *m_a,int ai_family=AF_UNSPEC,int ai_flags=0);
	static bool getaddr(const char *host, ip_addr *ip);
	static u_short getportinfo(sockaddr_i *m_a);
	/*
	static std::string getipinfo(sockaddr_i *m_a);
	
	static std::string make_ip(sockaddr_i *ip);
	*/
	static bool make_ip(ip_addr *ip,char *ips,int ips_len);
	static bool make_ip(sockaddr_i *ip, char *ips, int ips_len);
	static void init_socket();
	static void clean_socket();
#ifndef NDEBUG
	bool blockFlag;
	bool shutdownFlag;
	bool delay;
#endif
protected:
	SOCKET sockfd;
};
class KClientSocket: public KSocket
{
public:
	virtual ~KClientSocket() {

	}
	inline bool set_time(int tmo) {
		return set_time(tmo, tmo);
	}
	inline bool set_time(int snd_tmo, int recv_tmo) {
		struct timeval msec;
#ifdef _WIN32
		msec.tv_sec= snd_tmo * 1000;
#else
		msec.tv_sec = snd_tmo;
#endif
		msec.tv_usec = 0;
		int ret = setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (char *) &msec,
				sizeof(msec));
		if (ret != 0) {
			//debug("set send time_out error errno=%d\n", errno);
			return false;
		}
#ifdef _WIN32
		msec.tv_sec= recv_tmo * 1000;
#else
		msec.tv_sec = recv_tmo;
#endif

		ret = setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *) &msec,
				sizeof(msec));
		if (ret != 0) {
			//debug("set recv time_out error errno=%d\n", errno);
			return false;
		}
		return true;

	}
	inline bool read_all(char *buf,int len)
	{
		while (len > 0) {
			int r = read(buf, len);
			if (r <= 0)
				return false;
			len -= r;
			buf += r;
		}
		return true;
	}
	inline StreamState write_all(const char *buf, int len) {
		while (len > 0) {
			int r = write(buf, len);
			if (r <= 0) {
				return STREAM_WRITE_FAILED;
			}
			len -= r;
			buf += r;
		}
		return STREAM_WRITE_SUCCESS;
	}
	inline StreamState write_all(const char *buf)
	{
		return write_all(buf,(int)strlen(buf));
	}
	u_short get_remote_port();
	inline void get_remote_ip(char *ips,int ips_len)
	{
		make_ip(&addr,ips,ips_len);
	}
	void get_remote_addr(ip_addr *to);
	bool connect(sockaddr_i &m_adr, int tmo, sockaddr_i *bind_addr = NULL);
	bool connect(const char *host, int port, int tmo, sockaddr_i *bind_addr = NULL);
	/**
	* 半连接，即非阻socket，连接没有完成，要测试可写才算连上。
	*/
	bool halfconnect(const char *host, int port,int ai_family=0,sockaddr_i *bind_addr=NULL,bool tproxy=false);
	bool halfconnect(sockaddr_i &m_addr,sockaddr_i *bind_addr=NULL,bool tproxy=false);
	bool halfconnect(sockaddr_i *bind_addr=NULL,bool tproxy=false);
#ifdef KSOCKET_UNIX	
	bool connect(const char *unixfile,int tmo);
	bool halfconnect(const char *unixfile);
#endif
public:
#ifdef KSOCKET_SSL
	virtual int write(const char *str, int len);
	virtual int read(char *str, int len);
#else
	int write(const char *str, int len);
	int read(char *str, int len);
#endif
	int writev(iovec *v,int vc,bool isSSL=false);
	static int connect(SOCKET sockfd, const struct sockaddr *serv_addr,
			socklen_t addrlen, int tmo);
};
#define KClientPoolSocket KClientSocket
#define KSOCKET_ONLY_IPV4         0
#define KSOCKET_PROTO_AUTO        1
#define KSOCKET_ONLY_IPV6         2
#define KSOCKET_REUSEPORT         4
#ifdef ENABLE_TPROXY
#define KSOCKET_TPROXY            8
#endif
#define KSOCKET_PROTO_IPV4        KSOCKET_ONLY_IPV4
#define KSOCKET_PROTO_IPV6        KSOCKET_ONLY_IPV6   
class KServerSocket: public KSocket {
public:
	virtual ~KServerSocket()
	{
	}
	bool open(int port, const char * ip = NULL ,int flag = 0);
	bool open4(int port, const char *ip = NULL,bool tproxy=false);
	bool open6(int port, const char *ip = NULL,bool tproxy=false);
	
	inline KClientSocket *accept(bool noblock=false) {
		KClientSocket *client = new KClientSocket();
		if (!accept(client,noblock)) {
			delete client;
			return NULL;
		}
		return client;
	}
	inline bool accept(KClientSocket *client,bool noblock) {
		socklen_t sin_size = sizeof(client->addr);
#ifdef HAVE_ACCEPT4
#ifndef NDEBUG
		client->blockFlag = !noblock;
#endif
		int flag = SOCK_CLOEXEC;
		if(noblock){
			flag|=SOCK_NONBLOCK;
		}
		client->sockfd = ::accept4(sockfd, (struct sockaddr *) &client->addr,&sin_size,flag);
		return client->sockfd!=INVALID_SOCKET;
#else
		client->sockfd = ::accept(sockfd, (struct sockaddr *) &client->addr,&sin_size);
		if (client->sockfd == INVALID_SOCKET) {
			return false;
		}
		client->setCloseOnExec();
		if (noblock) {
			client->setnoblock();
		}
		return true;
#endif
	}

	const char *getIpVer() {
#ifdef KSOCKET_IPV6
		if (addr.v4.sin_family == PF_INET6) {
			return "6";
		}
#endif
		return "4";
	}
private:
	bool listen(int flag = 0);
};
#ifdef KSOCKET_UNIX	
class KUnixServerSocket: public KServerSocket
{
public:
	KUnixServerSocket()
	{
	}
	~KUnixServerSocket()
	{
	}
	bool open(const char *path);
};
#endif
#endif
