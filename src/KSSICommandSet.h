/*
 * KSSICommandSet.h
 *
 *  Created on: 2010-8-5
 *      Author: keengo
 */

#ifndef KSSICOMMANDSET_H_
#define KSSICOMMANDSET_H_

#include "KSSICommand.h"

class KSSICommandSet: public KSSICommand {
public:
	KSSICommandSet();
	virtual ~KSSICommandSet();
	Process_status process(KSSIContext *context, char *cmd, std::map<char *, char *,
			lessp_icase> &attribute, KWStream *out);
};

#endif /* KSSICOMMANDSET_H_ */
