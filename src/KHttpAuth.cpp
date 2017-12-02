#include <string.h>
#include "KHttpAuth.h"
#include "KHttpRequest.h"
#include "forwin32.h"
#include "malloc_debug.h"
static const char *auth_types[] = { "Basic", "Digest" };
KHttpAuth::~KHttpAuth() {
	//printf("delete auth now\n");
	free_header(add_header);
}
int KHttpAuth::parseType(const char *type) {
	for (unsigned i = 0; i < TOTAL_AUTH_TYPE; i++) {
		if (strcasecmp(type, auth_types[i]) == 0) {
			return i;
		}
	}
	return 0;
}
const char *KHttpAuth::buildType(int type) {
	if (type >= 0 && type < TOTAL_AUTH_TYPE) {
		return auth_types[type];
	}
	return "unknow";
}
void KHttpAuth::insertExtraHeader(KWStream &s)
{
	KHttpHeader *header = add_header;
	while (header) {
		s.write_all(header->attr,header->attr_len);
		s.WSTR(": ");
		s.write_all(header->val,header->val_len);
		s.WSTR("\r\n");
		header = header->next;
	}
}
void KHttpAuth::insertExtraHeader(KHttpRequest *rq)
{
	KHttpHeader *header = add_header;
	while (header) {
		rq->responseHeader(header);
		header = header->next;
	}
}
