/*
 * KHttpHeadPull.cpp
 *
 *  Created on: 2010-6-11
 *      Author: keengo
 */
#include <string.h>
#include "KHttpHeadPull.h"

KHttpHeadPull::KHttpHeadPull() {
	buf = new KStringBuf(2048);
}
KHttpHeadPull::~KHttpHeadPull() {
	if (buf) {
		delete buf;
	}
}
int KHttpHeadPull::pull(const char *str, int len, KHttpProtocolParserHook *hook) {
	if (buf == NULL) {
		return HTTP_PARSE_FAILED;
	}
	if (!buf->write_all(str, len)) {
		return HTTP_PARSE_FAILED;
	}
	int buf_size = buf->getSize();
	int ret = parse(buf->getBuf(), buf_size, hook);
	if (ret != HTTP_PARSE_SUCCESS && buf_size >= MAX_HTTP_HEAD_SIZE) {
		return HTTP_PARSE_FAILED;
	}
	return ret;
}
