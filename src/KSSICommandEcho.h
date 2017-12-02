/*
 * KSSICommandEcho.h
 *
 *  Created on: 2010-8-5
 *      Author: keengo
 */

#ifndef KSSICOMMANDECHO_H_
#define KSSICOMMANDECHO_H_

#include "KSSICommand.h"

class KSSICommandEcho: public KSSICommand {
public:
	KSSICommandEcho();
	virtual ~KSSICommandEcho();
	Process_status process(KSSIContext *context, char *cmd, std::map<char *, char *,
			lessp_icase> &attribute, KWStream *out);
};

#endif /* KSSICOMMANDECHO_H_ */
