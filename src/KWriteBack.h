#ifndef WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#define WRITE_BACK_H_DF7S77SJJJJJssJJJJJJJ
#include <vector>
#include <string.h>
#include <stdlib.h>
#include<list>
#include<string>
#include "global.h"
#include "KMutex.h"
#include "KJump.h"
#include "KXml.h"
#include "kserver.h"
#include "ksapi.h"
#include "KStringBuf.h"
#include "KHttpHeader.h"
#include "KHttpParser.h"
#include "KConfig.h"
#ifdef ENABLE_WRITE_BACK
class KHttpRequest;
class KWriteBackParser : public KHttpHeaderManager
{
public:
	KWriteBackParser()
	{
		memset(this, 0, sizeof(*this));
		status_code = STATUS_OK;
		keep_alive = false;

	}
	~KWriteBackParser()
	{
		if (header) {
			free_header(header);
		}
	}
	void Parse(char **str, int *len)
	{
		khttp_parser parser;
		memset(&parser, 0, sizeof(parser));
		while (*len > 0) {
			khttp_parse_result rs;
			memset(&rs, 0, sizeof(rs));
			switch (khttp_parse(&parser, str, len, &rs)) {
			case kgl_parse_finished:
			case kgl_parse_error:
			case kgl_parse_continue:
				return;
			default:
				parseHeader(rs.attr,rs.attr_len, rs.val, rs.val_len, rs.request_line);
			}
		}
	}
	void parseHeader(const char *attr,int attr_len, char *val, int val_len, bool isFirst)
	{
		if (isFirst) {
			status_code = atoi(val);
			return;
		}
		if (strcasecmp(attr, "Status") == 0) {
			status_code = atoi(val);
			return;
		}
		if (strcasecmp(attr, "Connection") == 0) {
			if (strcasecmp(val, "keep-alive") == 0) {
				keep_alive = true;
			}
			return;
		}
		if (strcasecmp(attr, "Content-Length") == 0
			|| strcasecmp(attr, "Transfer-Encoding") == 0) {
			return;
		}
		KHttpHeader *header = new_http_header(attr, attr_len, val, val_len);
		Append(header);
		return;
	}
	bool keep_alive;
	int status_code;
};
class KWriteBack:public KJump
{
public:
	KWriteBack()
	{
		ext = cur_config_ext;
		header = NULL;
	}
	void buildRequest(KHttpRequest *rq);
	std::string getMsg();
	void setMsg(std::string msg);
	void buildXML(std::stringstream &s) {
		s << "\t<writeback name='" << name << "'>";
		s << CDATA_START << KXml::encode(getMsg()) << CDATA_END << "</writeback>\n";
	}
	bool ext;
	bool keep_alive;
	int status_code;
	KHttpHeader *header;
	KStringBuf body;
};
#endif
#endif
