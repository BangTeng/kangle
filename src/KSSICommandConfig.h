/*
 * KSSICommandConfig.h
 *
 *  Created on: 2010-8-11
 *      Author: keengo
 */

#ifndef KSSICOMMANDCONFIG_H_
#define KSSICOMMANDCONFIG_H_

#include "KSSICommand.h"

class KSSICommandConfig: public KSSICommand {
public:
	KSSICommandConfig();
	virtual ~KSSICommandConfig();
	Process_status process(KSSIContext *context, char *cmd, std::map<char *, char *,
			lessp_icase> &attribute, KWStream *out);
};

#endif /* KSSICOMMANDCONFIG_H_ */
