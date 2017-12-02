/*
 * KSSIProcess.cpp
 *
 *  Created on: 2010-8-3
 *      Author: keengo
 */
#include "utils.h"
#include "KSSIProcess.h"
#include "KSSICommandInclude.h"
#include "KSSICommandSet.h"
#include "KSSICommandEcho.h"
#include "KSSICommandPrintEnv.h"
#include "KSSICommandCondition.h"
#include "KSSICommandExec.h"
#include "KSSICommandConfig.h"
#include "http.h"
#include "malloc_debug.h"
#ifdef ENABLE_TCMALLOC
#include "google/heap-checker.h"
#endif
using namespace std;
std::map<char *, KSSICommand *, lessp_icase> KSSIProcess::commands;

KSSIProcess::KSSIProcess() {
	//this->rq = rq;
	//context.setRequest(rq);
	context.setProcessor(this);
	//this->st = st;
	//this->autoDelete = autoDelete;
}
KSSIProcess::~KSSIProcess() {

}

void KSSIProcess::init() {
#ifdef ENABLE_TCMALLOC
	HeapLeakChecker::Disabler disabler;
#endif
	KSSICommand *command = new KSSICommandInclude;
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "include", command));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
		(char *) "flastmod", command));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
		(char *) "fsize", command));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "set", new KSSICommandSet));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "echo", new KSSICommandEcho));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "printenv", new KSSICommandPrintEnv));
	command = new KSSICommandCondition;
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "if", command));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "elif", command));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "else", command));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "endif", command));
	KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
			(char *) "config", new KSSICommandConfig));
	//KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
	//		(char *) "exec", new KSSICommandExec));
	//KSSIProcess::commands.insert(pair<char *, KSSICommand *> (
	//			(char *) "einc", new KSSICommandExec));

}
void KSSIProcess::readBody(KHttpRequest *rq)
{
	int len;
	bool cmd;
	st = rq->ctx->st;
	if (st==NULL) {
		stageEndRequest(rq);
		return;
	}
	for (;;) {
		char *buf = context.curFile->getBlockBuffer(len,cmd);
		if (buf==NULL) {
			if (!context.popFileContext()) {
				stage_rdata_end(rq,STREAM_WRITE_SUCCESS);
				return;
			}
			continue;
		}
		if (cmd) {
			Process_status status = processCommand(buf,len);
			switch(status){
				case Process_sub_request:
					return;
				case Process_failed:
					stage_rdata_end(rq,STREAM_WRITE_FAILED);
					return ;
				default:
					break;
			}
		} else {
			if (context.isCondition() && len > 0) {
				if (st->write_all(buf, len) != STREAM_WRITE_SUCCESS) {
					stage_rdata_end(rq,STREAM_WRITE_FAILED);
					return ;
				}
			}
		}
		if(try_send_request(rq)){
			return;
		}
	}
}
Process_status KSSIProcess::processCommand(char *data,int len)
{
	assert(len> (int)sizeof(SSI_START_STRING)-1);
	data += sizeof(SSI_START_STRING)-1;
	char *hot = data;
	while (*hot && !isspace((unsigned char)*hot))
		hot++;
	*hot = '\0';
	map<char *, KSSICommand *, lessp_icase>::iterator it;
	it = commands.find(data);
	if (it == commands.end()) {
		//st->write_all("<!-- unknow command ")
		*st << "<!-- unknow ssi command [" << data << "] -->";
		return Process_success;
	}
	KSSICommand *commandor = (*it).second;
	if (!commandor->isConditionCommand() && !context.isCondition()) {
		return Process_success;
	}
	map<char *, char *, lessp_icase> attribute;
	buildAttribute(hot + 1, attribute);
	return commandor->process(&context, data, attribute, st);
}
