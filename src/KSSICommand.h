/*
 * KSSICommand.h
 *
 *  Created on: 2010-8-2
 *      Author: keengo
 */

#ifndef KSSICOMMAND_H_
#define KSSICOMMAND_H_
#include <map>
#include <string>
#include "utils.h"
#include "KStream.h"
#include "KSSIContext.h"
#include "global.h"
enum Process_status
{
	Process_success,
	Process_failed,
	Process_sub_request
};
class KSSICommand {
public:
	KSSICommand();
	virtual ~KSSICommand();
	virtual Process_status process(KSSIContext *context, char *cmd,std::map<char *, char *,
			lessp_icase> &attribute, KWStream *out) = 0;
	virtual bool isConditionCommand() {
		return false;
	}
};
#endif /* KSSICOMMAND_H_ */
