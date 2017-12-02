/*
 * KChildListen.cpp
 *
 *  Created on: 2010-7-10
 *      Author: keengo
 */
#include <vector>
#include "KChildListen.h"
#include "log.h"
//#include "KSelectorManager.h"
#include "api_child.h"

KChildListen::KChildListen() {
	server = NULL;
}
KChildListen::~KChildListen() {
	if(server){
		delete server;
	}
}
ReadState KChildListen::canRead()
{
	KClientSocket *client = new KClientSocket;
	if (!server->accept(client,false)) {
		delete client;
		debug("cann't accept error=%d\n",GetLastError());
		return READ_CONTINUE;
	}
	if(!m_thread.start(client,api_child_thread,false)){
		delete client;
	}
	return READ_CONTINUE;
}

