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
#ifndef KSERVER_H_
#define KSERVER_H_
#include "KSelectable.h"
#include "KSSLSocket.h"
#include "KVirtualHostContainer.h"
#include "KIpVirtualHost.h"
#include "KCountable.h"
class KHttpRequest;
class KVirtualHost;
class KServer;
class KServerSelectable : public KSelectable {
public:
	KServerSelectable(KServer *server, KServerSocket *server_socket);
	~KServerSelectable();
	KServer *server;
	KServerSocket *server_socket;
	KServerSelectable *next;
	void remove_socket_event();
	void listen_event();
	
};
class KServer :  public KAtomCountable {
public:
	KServer();
	bool is_opened()
	{
		return server_selectable != NULL;
	}
	bool open();
	void close();
	bool start();
	bool is_multi_selectale()
	{
		assert(server_selectable);
		return (server_selectable->next!=NULL);
	}
	KServerSelectable *server_selectable;
	char ip[MAXIPLEN];
	u_short model;
	u_short port;
	bool ipv4;
	//是否是动态的，即可删除的
	bool dynamic;
	bool static_flag;
	bool remove_static_flag;
	//是否已经开始
	bool started;
#ifdef KSOCKET_SSL
	bool http2;
	std::string certificate;
	std::string certificate_key;
	std::string cipher;
	std::string protocols;
	SSL_CTX *ssl_ctx;
	bool load_ssl();
	bool is_ssl_loaded()
	{
		return ssl_ctx != NULL;
	}
#endif

public:
	void clear();
	inline bool isClosed()
	{
		return closed;
	}
	void setClosed()
	{
		closed = true;
	}
	bool isEmpty();
	KVirtualHostContainer *vhc;
	void add_static(KVirtualHost *vh);
	void remove_static(KVirtualHost *vh);
	void removeVirtualHost(KVirtualHost *vh);
	void bindVirtualHost(KVirtualHost *vh,bool high);
	void unbindAllVirtualHost();
private:
	static int failedTries;
	bool internal_open(int flag);
	void add_server_socket(KServerSocket *socket);
	virtual ~KServer();
	volatile bool closed;
};
#endif /* KSERVER_H_ */
