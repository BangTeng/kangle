/*
 * KSSICommandConfig.cpp
 *
 *  Created on: 2010-8-11
 *      Author: keengo
 */

#include "KSSICommandConfig.h"

KSSICommandConfig::KSSICommandConfig() {

}

KSSICommandConfig::~KSSICommandConfig() {
}
Process_status KSSICommandConfig::process(KSSIContext *context, char *cmd, std::map<
		char *, char *, lessp_icase> &attribute, KWStream *out) {
	std::map<char *, char *, lessp_icase>::iterator it;
	it = attribute.find((char *)"sizefmt");
	if (it != attribute.end()) {
		if (!context->setSizeFmt((*it).second)) {
			*out << "<!-- set sizefmt [" << (*it).second << "] error -->";
		}
	}
	it = attribute.find((char *)"timefmt");
	if (it != attribute.end()) {
		if (!context->setTimeFmt((*it).second)) {
			*out << "<!-- set timefmt [" << (*it).second << "] error -->";
		}
	}
	it = attribute.find((char *)"errmsg");
	if (it != attribute.end()) {
		if (!context->setErrMsg((*it).second)) {
			*out << "<!-- set errmsg [" << (*it).second << "] error -->";
		}
	}
	return Process_success;
}
