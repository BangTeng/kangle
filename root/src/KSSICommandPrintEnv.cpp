/*
 * KSSICommandPrintEnv.cpp
 *
 *  Created on: 2010-8-5
 *      Author: keengo
 */

#include "KSSICommandPrintEnv.h"

KSSICommandPrintEnv::KSSICommandPrintEnv() {
	// TODO Auto-generated constructor stub

}

KSSICommandPrintEnv::~KSSICommandPrintEnv() {
	// TODO Auto-generated destructor stub
}
Process_status KSSICommandPrintEnv::process(KSSIContext *context, char *cmd, std::map<
		char *, char *, lessp_icase> &attribute, KWStream *out) {
	context->printEnv(out);
	return Process_success;
}
