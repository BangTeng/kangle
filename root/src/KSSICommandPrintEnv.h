/*
 * KSSICommandPrintEnv.h
 *
 *  Created on: 2010-8-5
 *      Author: keengo
 */

#ifndef KSSICOMMANDPRINTENV_H_
#define KSSICOMMANDPRINTENV_H_

#include "KSSICommand.h"

class KSSICommandPrintEnv: public KSSICommand {
public:
	KSSICommandPrintEnv();
	virtual ~KSSICommandPrintEnv();
	Process_status process(KSSIContext *context, char *cmd, std::map<char *, char *,
			lessp_icase> &attribute, KWStream *out);
};

#endif /* KSSICOMMANDPRINTENV_H_ */
