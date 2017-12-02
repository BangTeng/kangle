/*
 * KSSIProcess.h
 *
 *  Created on: 2010-8-3
 *      Author: keengo
 */

#ifndef KSSIPROCESS_H_
#define KSSIPROCESS_H_
#include "KHttpTransfer.h"
#include "utils.h"

#include "KHttpRequest.h"
#include "KHttpObject.h"
#include "KSSIContext.h"
#include "KSSICommand.h"

class KSSIProcess {
public:
	KSSIProcess();
	virtual ~KSSIProcess();
	/**
	读ssi内容，返回-1,结束,0,已经切换到子请求状态，其它
	*/
	void readBody(KHttpRequest *rq);
	static void init();	
	KSSIContext context;
private:
	Process_status processCommand(char *data,int len);
	KWStream *st;
	bool autoDelete;
	static std::map<char *, KSSICommand *, lessp_icase> commands;
};
#endif /* KSSIPROCESS_H_ */
