/*
 * KSSICommandExec.h
 *
 *  Created on: 2010-8-11
 *      Author: keengo
 */

#ifndef KSSICOMMANDEXEC_H_
#define KSSICOMMANDEXEC_H_

#include "KSSICommand.h"

class KSSICommandExec: public KSSICommand {
public:
	KSSICommandExec();
	virtual ~KSSICommandExec();
	Process_status process(KSSIContext *context, char *cmd, std::map<char *, char *,
			lessp_icase> &attribute, KWStream *out);
private:
	bool processFetchObject(KHttpRequest *rq,KWStream *out);
};

#endif /* KSSICOMMANDEXEC_H_ */
