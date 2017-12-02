/*
 * KSSICommandEcho.cpp
 *
 *  Created on: 2010-8-5
 *      Author: keengo
 */

#include "KSSICommandEcho.h"

KSSICommandEcho::KSSICommandEcho() {

}

KSSICommandEcho::~KSSICommandEcho() {
}

Process_status KSSICommandEcho::process(KSSIContext *context, char *cmd, std::map<char *,
		char *, lessp_icase> &attribute, KWStream *out) {
	std::map<char *, char *, lessp_icase>::iterator it;
	it = attribute.find((char *) "var");
	if (it == attribute.end()) {
		*out << "<!-- set command must have var attribute -->";
		return Process_success;
	}
	const char *value = context->getValue((*it).second);
	if (value) {
		*out << value;
	}
	return Process_success;
}
