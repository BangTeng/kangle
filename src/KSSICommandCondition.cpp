/*
 * KSSICommandCondition.cpp
 *
 *  Created on: 2010-8-6
 *      Author: keengo
 */

#include "KSSICommandCondition.h"
#include "KExpressionParseTree.h"

KSSICommandCondition::KSSICommandCondition() {

}

KSSICommandCondition::~KSSICommandCondition() {
}
bool KSSICommandCondition::do_exp(KSSIContext *context, char *cmd, std::map<
		char *, char *, lessp_icase> &attribute, KWStream *out) {
	assert(context->curFile->parentCondition);
	assert(!context->curFile->haveCondition);
	std::map<char *, char *, lessp_icase>::iterator it;
	it = attribute.find((char *) "expr");
	if (it == attribute.end()) {
		*out << "<!-- " << cmd << " must have exp -->";
		return true;
	}
	char *expr = (*it).second;
	KExpressionParseTree parser;
	parser.setContext(context);
	ExpResult result = parser.evaluate(expr);
	if (result == Exp_failed) {
		*out << "<!-- exp error -->";
		return true;
	}
	if (result == Exp_true) {
		context->curFile->condition = true;
		context->curFile->haveCondition = true;
	} else {
		context->curFile->condition = false;
	}
	return true;
}
Process_status KSSICommandCondition::process(KSSIContext *context, char *cmd, std::map<
		char *, char *, lessp_icase> &attribute, KWStream *out) {
	if (strcasecmp(cmd, "if") == 0) {
		context->curFile->pushCondition();
		if (!context->curFile->condition) {
			return Process_success;
		}
		if(do_exp(context, cmd, attribute, out)){
			return Process_success;
		}
		return Process_failed;
	} else if (strcasecmp(cmd, "elif") == 0) {
		if (!context->curFile->parentCondition
				|| context->curFile->haveCondition) {
			context->curFile->condition = false;
			return Process_success;
		}
		if(do_exp(context, cmd, attribute, out)){
			return Process_success;
		}
		return Process_failed;
	} else if (strcasecmp(cmd, "else") == 0) {
		if (!context->curFile->parentCondition
				|| context->curFile->haveCondition) {
			context->curFile->condition = false;
			return Process_success;
		}
		context->curFile->condition = true;
		context->curFile->haveCondition = true;
		return Process_success;
	} else if (strcasecmp(cmd, "endif") == 0) {
		if (!context->curFile->popCondition()) {
			*out << "<!-- endif not match if -->";
		}
		return Process_success;
	}
	return Process_failed;
}
