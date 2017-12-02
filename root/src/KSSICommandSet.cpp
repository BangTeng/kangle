/*
 * KSSICommandSet.cpp
 *
 *  Created on: 2010-8-5
 *      Author: keengo
 */

#include "KSSICommandSet.h"
using namespace std;
KSSICommandSet::KSSICommandSet() {
}
KSSICommandSet::~KSSICommandSet() {
}
Process_status KSSICommandSet::process(KSSIContext *context, char *cmd, std::map<char *,
		char *, lessp_icase> &attribute, KWStream *out) {
	std::map<char *, char *, lessp_icase>::iterator it;
	it = attribute.find((char *) "var");
	if (it == attribute.end()) {
		*out << "<!-- set command must have var attribute -->";
		return Process_success;
	}
	char *val = (*it).second;
	char *value = NULL;
	std::map<char *, char *, lessp_icase>::iterator it2;
	it2 = attribute.find((char *) "value");
	if (it2 != attribute.end()) {
		value = context->parseString((*it2).second);
	}
	context->setValue(val, value);
	if (value) {
		xfree(value);
	}
	return Process_success;
}
