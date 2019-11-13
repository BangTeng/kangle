#include <stdlib.h>
#include "KHttpResponseParser.h"
#include "lib.h"
#include "http.h"
#include "KHttpFieldValue.h"
#include "time_utils.h"
#include "kmalloc.h"
kgl_header_result KHttpResponseParser::InternalParseHeader(KHttpRequest *rq, KHttpObject *obj,const char *attr, int attr_len, const char *val, int *val_len, bool request_line)
{
	if (request_line && proto == Proto_http) {
		if (strncasecmp(attr, "HTTP/", 5) == 0) {
			const char *dot = strchr(attr + 5, '.');
			if (dot == NULL) {
				return kgl_header_failed;
			}
			http_major = *(dot - 1) - 0x30;//major;
			http_minor = *(dot + 1) - 0x30;//minor;
			if (http_major > 1) {
				rq->ctx->upstream_connection_keep_alive = true;
			} else if (http_major == 1 && http_minor == 1) {
				rq->ctx->upstream_connection_keep_alive = true;
			}
			obj->data->status_code = atoi(val);
			return kgl_header_no_insert;
		}
	}	
	if (proto == Proto_spdy && *attr == ':') {
		attr++;
		if (strcasecmp(attr, "version") == 0) {
			return kgl_header_no_insert;
		}
	}
	if (proto != Proto_http) {
		if (!strcasecmp(attr, "Status")) {
			if (obj->data->status_code == 0) {
				obj->data->status_code = atoi(val);
			}
			return kgl_header_no_insert;
		}
		if (!strcasecmp(attr, "Location")) {
			if (obj->data->status_code == 0) {
				obj->data->status_code = STATUS_FOUND;
			}
			return kgl_header_success;
		}
		if (!strcasecmp(attr, "WWW-Authenticate")) {
			obj->data->status_code = STATUS_UNAUTH;
			return kgl_header_success;
		}
	}
	if (!strcasecmp(attr, "Keep-Alive")) {
		const char *data = strstr(val, "timeout=");
		if (data) {
			//确保有效，减掉2秒生存时间
			keep_alive_time_out = atoi(data + 8) - 2;
		}
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Etag")) {
		SET(obj->index.flags, OBJ_HAS_ETAG);
		return kgl_header_insert_begin;
	}
	if (!strcasecmp(attr, "Content-Range")) {
		const char *p = strchr(val, '/');
		if (p) {
			rq->ctx->content_range_length = string2int(p + 1);
			SET(obj->index.flags, ANSW_HAS_CONTENT_RANGE);
		}
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Content-length")) {
		obj->index.content_length = string2int(val);
		//obj->index.content_length = atoi(val);
		SET(obj->index.flags, ANSW_HAS_CONTENT_LENGTH);
		CLR(obj->index.flags, ANSW_CHUNKED);
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Content-Type")) {
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Date")) {
		serverDate = parse1123time(val);
		//SET(obj->index.flags,OBJ_HAS_DATE);
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Last-Modified")) {
		obj->index.last_modified = parse1123time(val);
		if (obj->index.last_modified > 0) {
			obj->index.flags |= ANSW_LAST_MODIFIED;
		}
		return kgl_header_success;
	}
	if (strncasecmp(attr, "Set-Cookie", sizeof("Set-Cookie") - 1) == 0) {
		if (!TEST(obj->index.flags, OBJ_IS_STATIC2)) {
			obj->index.flags |= ANSW_NO_CACHE;
		}
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Pragma")) {
		KHttpFieldValue field(val);
		do {
			if (field.is("no-cache")) {
				obj->index.flags |= ANSW_NO_CACHE;
				break;
			}
		} while (field.next());
		return kgl_header_success;
	}
	if (!strcasecmp(attr, "Cache-Control")) {
		KHttpFieldValue field(val);
		do {
#ifdef ENABLE_STATIC_ENGINE
			if (!TEST(obj->index.flags, OBJ_IS_STATIC2)) {
#endif
				if (field.is("no-store")) {
					obj->index.flags |= ANSW_NO_CACHE;
				} else if (field.is("no-cache")) {
					obj->index.flags |= ANSW_NO_CACHE;
				} else if (field.is("private")) {
					obj->index.flags |= ANSW_NO_CACHE;
				}
#ifdef ENABLE_STATIC_ENGINE
			}
#endif
#ifdef ENABLE_FORCE_CACHE
			if (field.is("static")) {
				//通过http header强制缓存
				obj->force_cache();
			} else
#endif
				if (field.is("public")) {
					CLR(obj->index.flags, ANSW_NO_CACHE);
				} else if (field.is("max-age=", (int *)&obj->index.max_age)) {
					obj->index.flags |= ANSW_HAS_MAX_AGE;
				} else if (field.is("s-maxage=", (int *)&obj->index.max_age)) {
					obj->index.flags |= ANSW_HAS_MAX_AGE;
				} else if (field.is("must-revalidate")) {
					obj->index.flags |= OBJ_MUST_REVALIDATE;
				}
		} while (field.next());
#ifdef ENABLE_FORCE_CACHE
		if (TEST(obj->index.flags, OBJ_IS_STATIC2)) {
			return kgl_header_no_insert;
		}
#endif
		return kgl_header_success;
	}

	if (!strcasecmp(attr, "Age")) {
		age = atoi(val);
		return kgl_header_no_insert;
	}
	if (!strcasecmp(attr, "Connection")) {
		KHttpFieldValue field(val);
		do {
			if (field.is2("keep-alive", 10)) {
				rq->ctx->upstream_connection_keep_alive = true;
			} else if (field.is2("close", 5)) {
				rq->ctx->upstream_connection_keep_alive = false;
			} else if (TEST(rq->flags, RQ_HAS_CONNECTION_UPGRADE) && field.is2("upgrade", 7)) {
				rq->ctx->connection_upgrade = true;
				rq->ctx->upstream_connection_keep_alive = false;
			}
		} while (field.next());
		return kgl_header_no_insert;
	}
	if (*attr == 'x' || *attr == 'X') {
		if (!TEST(rq->filter_flags, RF_NO_X_SENDFILE) &&
			(strcasecmp(attr, "X-Accel-Redirect") == 0 || strcasecmp(attr, "X-Proxy-Redirect") == 0)) {
			SET(obj->index.flags, ANSW_XSENDFILE);
			return kgl_header_insert_begin;
		}
		if (strcasecmp(attr, "X-No-Buffer") == 0) {
			SET(rq->filter_flags, RF_NO_BUFFER);
#ifdef ENABLE_TF_EXCHANGE
			rq->closeTempFile();
#endif
			return kgl_header_no_insert;
		}
		if (strcasecmp(attr, "X-Gzip") == 0) {
			obj->need_gzip = 1;
			return kgl_header_no_insert;
		}
	}
	if (strcasecmp(attr, "Transfer-Encoding") == 0) {
		if (strcasecmp(val, "chunked") == 0) {
			if (!TEST(obj->index.flags, ANSW_HAS_CONTENT_LENGTH)) {
				obj->index.flags |= ANSW_CHUNKED;
			}
			return kgl_header_no_insert;
		}
	}
	if (strcasecmp(attr, "Content-Encoding") == 0) {
		if (strcasecmp(val, "none") == 0) {
			return kgl_header_no_insert;
		}
		if (strcasecmp(val, "gzip") == 0) {
			obj->url->set_content_encoding(KGL_ENCODING_GZIP);
		} else	if (strcasecmp(val, "deflate") == 0) {
			obj->url->set_content_encoding(KGL_ENCODING_DEFLATE);
		} else if (strcasecmp(val, "compress") == 0) {
			obj->url->set_content_encoding(KGL_ENCODING_COMPRESS);
		} else if (strcasecmp(val, "br") == 0) {
			obj->url->set_content_encoding(KGL_ENCODING_BR);
		} else if (strcasecmp(val, "identity") == 0) {
			obj->url->encoding = (u_char)~KGL_ENCODING_YES;
		} else if (*val) {
			obj->url->set_content_encoding(KGL_ENCODING_UNKNOW);
		}
		return kgl_header_success;
	}
	if (!TEST(obj->index.flags, ANSW_HAS_EXPIRES) &&
		!strcasecmp(attr, "Expires")) {
		SET(obj->index.flags, ANSW_HAS_EXPIRES);
		expireDate = parse1123time(val);
		return kgl_header_success;
	}
	
	return kgl_header_success;
}
bool KHttpResponseParser::ParseHeader(KHttpRequest *rq, const char *attr, int attr_len, const char *val, int val_len, bool request_line)
{
	//printf("%s: %s\n", attr, val);
	switch (InternalParseHeader(rq, rq->ctx->obj, attr, attr_len, val, &val_len, request_line)) {
		case kgl_header_failed:
			return false;
		case kgl_header_insert_begin:
			return AddHeader(attr, attr_len, val, val_len, false);
		case kgl_header_success:
			return AddHeader(attr, attr_len, val, val_len, true);
		default:
			return true;
	}
}
void KHttpResponseParser::EndParse(KHttpRequest *rq)
{
	/*
 * see rfc2616
 * 没有 Last-Modified 我们不缓存.
 * 但如果有 expires or max-age  除外
 */
	if (!TEST(rq->ctx->obj->index.flags, ANSW_LAST_MODIFIED | OBJ_HAS_ETAG)) {
		if (!TEST(rq->ctx->obj->index.flags, ANSW_HAS_MAX_AGE | ANSW_HAS_EXPIRES)) {
			SET(rq->ctx->obj->index.flags, ANSW_NO_CACHE);
		}
	}
	if (!TEST(rq->ctx->obj->index.flags, ANSW_NO_CACHE)) {
		if (serverDate == 0) {
			serverDate = kgl_current_sec;
		}
		time_t responseTime = kgl_current_sec;
		/*
		 * the age calculation algorithm see rfc2616 sec 13
		 */
		unsigned apparent_age = 0;
		if (responseTime > serverDate) {
			apparent_age = (unsigned)(responseTime - serverDate);
		}
		unsigned corrected_received_age = MAX(apparent_age, age);

		unsigned response_delay = (unsigned)(responseTime - rq->begin_time_msec / 1000);
		unsigned corrected_initial_age = corrected_received_age	+ response_delay;
		unsigned resident_time = (unsigned)(kgl_current_sec - responseTime);
		age = corrected_initial_age + resident_time;
		if (!TEST(rq->ctx->obj->index.flags, ANSW_HAS_MAX_AGE)
			&& TEST(rq->ctx->obj->index.flags, ANSW_HAS_EXPIRES)) {
			rq->ctx->obj->index.max_age = (unsigned)(expireDate - serverDate) - age;
		}
	}
	if (last) {
		last->next = rq->ctx->obj->data->headers;
		rq->ctx->obj->data->headers = StealHeader();
	}
	first_body_time = kgl_current_sec;
}