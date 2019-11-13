#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "kmalloc.h"
#include "global.h"
#include "KHttpParser.h"

kgl_parse_result khttp_parse_header(khttp_parser *parser, char *header, char *end, khttp_parse_result *rs)
{
	char *val;
	if (rs->is_first && !parser->first_same) {
		val = strchr(header, ' ');
		rs->request_line = 1;
	} else {
		val = strchr(header, ':');
	}
	
	if (val == NULL) {
		return kgl_parse_continue;
		/*
		if (isFirst) {
			//����fastcgiЭ�飬�п��ܻᷢ�͵�һ��HTTP/1.1 xxx xxx������
			//Ϊ�˼����ԣ�Ҫ���Ե�һ�д���
			return true;
		}
		return false;
		*/
	}
	*val = '\0';
	val++;
	while (*val && IS_SPACE(*val)) {
		val++;
	}
	char *header_end = header;
	while (*header_end && !IS_SPACE(*header_end)) {
		header_end++;
	}
	rs->attr = header;
	rs->attr_len = (int)(header_end - header);
	*header_end = '\0';
	rs->val_len = (int)(end - val);
	rs->val = val;
	return kgl_parse_success;
}
kgl_parse_result khttp_parse(khttp_parser *parser, char **start, int *len, khttp_parse_result *rs)
{
restart:
	kassert(*len >= 0);
	if (*len <= 0) {
		kassert(*len == 0);
		return kgl_parse_continue;
	}
	char *pn = (char *)memchr(*start, '\n', *len);
	if (pn == NULL) {
		return kgl_parse_continue;
	}
	if (*start[0] == '\n' || *start[0] == '\r') {
		int checked = (int)(pn + 1 - *start);
		parser->header_len += checked;
		*start += checked;
		*len -= checked;
		if (!parser->started) {			
			goto restart;
		}
		parser->finished = 1;
		//printf("body[0]=%d,bodyLen=%d\n",body[0],bodyLen);
		return kgl_parse_finished;
	}
	if (parser->started) {
		/*
		 * ���ǻ�Ҫ�������http����û�л��У���rfc2616.
		 *        LWS            = [CRLF] 1*( SP | HT )
		 *        ���ǻ�Ҫ������һ�е�һ���ַ��Ƿ��ǿ��С�
		 */
		if (pn - *start == *len - 1) {
			/*
			 * ���\n�������ַ�,��Ҫcontinue.
			 */
			return kgl_parse_continue;
		}
		/*
		 * �����һ�п�ͷ�ַ���SP��HT����Ҫ���д�����\r��\n������SP
		 */
		while (pn[1]==' ' || pn[1]=='\t') {
			*pn = ' ';
			int checked = (int)(pn + 1 - *start);
			char *pr = (char *)memchr(*start, '\r', checked);
			if (pr) {
				*pr = ' ';
			}
			pn = (char *)memchr(pn,'\n', *len - checked);
			if (pn == NULL) {
				return kgl_parse_continue;
			}
		}
	}
	int checked = (int)(pn + 1 - *start);
	parser->header_len += checked;
	char *hot = *start;
	int hot_len = pn - *start;
	*start += checked;
	*len -= checked;
	if (hot_len > 2 && *(pn-1)=='\r') {
		pn--;
	}
	*pn = '\0';
	rs->is_first = !parser->started;
	parser->started = 1;
	return khttp_parse_header(parser, hot, pn, rs);
}
