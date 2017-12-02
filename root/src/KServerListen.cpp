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
#include "KServerListen.h"
#include "log.h"
#include "KSelectorManager.h"
#include "KThreadPool.h"
#include "malloc_debug.h"

using namespace std;
void handleServerListen(void *arg,int got);
inline void serverListenWork(KServer *server)
{
#ifdef _WIN32
	bool noblocking = false;
#endif
	bool ssl_model = false;
	KClientSocket *socket;
#ifdef KSOCKET_SSL
	if(TEST(server->model,WORK_MODEL_SSL)){
		socket = new KSSLSocket(server->ssl_ctx);
		ssl_model = true;
#ifdef _WIN32
		//windows ssl 要工作在非阻塞模式
		noblocking = true;
#endif
	} else 
#endif
		socket = new KClientSocket;

#ifdef _WIN32
	if (!server->server->accept(socket,noblocking)) {
#else
	if (!server->server->accept(socket,true)) {
#endif
		klog(KLOG_ERR, "cann't accept connect,errno=%s\n", strerror(errno));		
		delete socket;
	} else {
#ifndef NDEBUG
		//klog(KLOG_DEBUG,"new client %s:%d connect to %s:%d sockfd=%d\n", socket->get_remote_ip().c_str(), socket->get_remote_port(),socket->get_self_ip().c_str(),socket->get_self_port(),socket->get_socket());
#endif
		KConnectionSelectable *c = new KConnectionSelectable(socket);
		c->ls = server;
		if (ssl_model) {
			c->set_flag(STF_SSL | STF_ET);
		}
		server->addRef();
		selectorManager.startRequest(c);
	}
}
void handleServerListen(void *arg,int got)
{
	KServer *server = (KServer *)arg;
#ifdef _WIN32
	server->handleAcceptEx(got);
	return;
#endif
#ifndef _WIN32
  	if (server->isClosed()) {
		server->removeSocket();
		server->started = false;
		server->release();
		return ;
	}	
#endif
	serverListenWork(server);
#ifdef SOLARIS
	//solaris port need every time call addSocket
	server->selector->listen(server,handleServerListen);
#endif
}
KServerListen::KServerListen() {
}
KServerListen::~KServerListen() {
}
void KServerListen::start(std::vector<KServer *> &serverList) {
	std::vector<KServer *>::iterator it;
	for (it = serverList.begin(); it != serverList.end();) {

		if (!start(*it)) {
			it = serverList.erase(it);
		} else {
			it++;
		}
	}
}
bool KServerListen::start(KServer *server) {
	bool result = false;
	server->addRef();
	server->started = true;
	result = selectorManager.listen(server,handleServerListen);
	if (!result) {
		klog(KLOG_ERR,"event listen failed. switch use thread to accept.\n");
	}
	if (!result) {
		//启动线程侦听
		if (!m_thread.start((void *) server, KServerListen::serverThread)) {
			klog(KLOG_ERR, "cann't start serverThread,errno=%d\n", errno);
			server->release();
			server->started = false;
			return false;
		}
	}
	return true;
}
FUNC_TYPE FUNC_CALL KServerListen::serverThread(void *param) {
	KServer *server = (KServer *) param;
	for (;;) {
		if (server->isClosed()) {
			server->started = false;
			server->release();
			break;
		}
		serverListenWork(server);
	}
	KTHREAD_RETURN;
}
