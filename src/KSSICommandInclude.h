/*
 * KSSICommandInclude.h
 *
 *  Created on: 2010-8-4
 *      Author: keengo
 */

#ifndef KSSICOMMANDINCLUDE_H_
#define KSSICOMMANDINCLUDE_H_

#include "KSSICommand.h"

class KSSICommandInclude: public KSSICommand {
public:
	KSSICommandInclude();
	virtual ~KSSICommandInclude();
	Process_status process(KSSIContext *context,char *cmd,
			std::map<char *, char *, lessp_icase> &attribute, KWStream *out);
};

#endif /* KSSICOMMANDINCLUDE_H_ */
