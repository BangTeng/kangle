/*
 * KSSICommandCondition.h
 *
 *  Created on: 2010-8-6
 *      Author: keengo
 */

#ifndef KSSICOMMANDCONDITION_H_
#define KSSICOMMANDCONDITION_H_

#include "KSSICommand.h"

class KSSICommandCondition: public KSSICommand {
public:
	KSSICommandCondition();
	virtual ~KSSICommandCondition();
	Process_status process(KSSIContext *context, char *cmd, std::map<char *, char *,
			lessp_icase> &attribute, KWStream *out);
	bool isConditionCommand() {
		return true;
	}
private:
	bool do_exp(KSSIContext *context,char *cmd,
			std::map<char *, char *, lessp_icase> &attribute, KWStream *out);
};

#endif /* KSSICOMMANDCONDITION_H_ */
