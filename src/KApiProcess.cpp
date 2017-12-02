/*
 * KApiProcess.cpp
 *
 *  Created on: 2010-10-24
 *      Author: keengo
 */
#include <vector>
#include <string.h>
#include "KApiProcess.h"
#include "malloc_debug.h"
#include "lang.h"
KUpstreamSelectable *KApiProcess::poweron(KVirtualHost *vh,KExtendProgram *erd,bool &success)
{
	bool unix_socket = false;
#ifdef KSOCKET_UNIX
	unix_socket = conf.unix_socket;
#endif
	sp_info pi;
	KApiRedirect *rd = static_cast<KApiRedirect *> (erd);
	stLock.Lock();
	if (st == NULL) {
		st = new KApiPipeStream;
		if (!rd->createProcess(vh, st) || !st->init(vh, WORK_TYPE_SP)
				|| !st->listen(0, &pi,unix_socket)) {
			delete st;
			st = NULL;
			stLock.Unlock();
			success = false;
			return NULL;
		}
	}
	stLock.Unlock();
#ifdef KSOCKET_UNIX	
	if (unix_socket) {
		std::stringstream s;
		s << "/tmp/extworker." << pi.port << ".sock";
		s.str().swap(unix_path);
	} else {
#endif
		if(!KSocket::getaddr("127.0.0.1",pi.port,&addr)){
			klog(KLOG_ERR,"cann't get 127.0.0.1 addr\n");
			success = false;
			return NULL;
		}
#ifdef KSOCKET_UNIX	
	}
#endif
	success = true;
	return NULL;
}
