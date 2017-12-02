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
	��ssi���ݣ�����-1,����,0,�Ѿ��л���������״̬������
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
