#include "KTcpUpstream.h"
#include "KPoolableSocketContainer.h"
#include "KHttpRequest.h"
#include "http.h"
void KTcpUpstream::OnPushContainer()
{
#ifndef _WIN32
	if (cn->st.selector) {
		selectable_remove(&cn->st);
		cn->st.selector = NULL;
	}
	kassert(TEST(cn->st.st_flags, STF_READ | STF_WRITE | STF_REV | STF_WEV) == 0);
#endif
}
void KTcpUpstream::Gc(int life_time,time_t base_time)
{
	if (container == NULL) {
		Destroy();
		return;
	}
#ifndef NDEBUG
	if (cn->st.selector) {
		kassert(cn->st.queue.next == NULL);
	}
#endif
	container->gcSocket(this, life_time, base_time);
}
kev_result KTcpUpstream::Connect(void *arg, result_callback result)
{
	return kconnection_connect(cn, result, arg);
}
kev_result KTcpUpstream::Read(void *arg, result_callback result, buffer_callback buffer)
{
	return selectable_read(&cn->st, result, buffer, arg);
}
kev_result KTcpUpstream::Write(void *arg, result_callback result, buffer_callback buffer)
{
	return selectable_write(&cn->st, result, buffer, arg);
}
void KTcpUpstream::BindSelector(kselector *selector)
{
	kassert(cn->st.selector == NULL || cn->st.selector == selector);
	selectable_bind(&cn->st, selector);
}
bool KTcpUpstream::BuildHttpHeader(KHttpRequest *rq, KWStream *s)
{

	char tmpbuff[50];
	KStreamHttpEnv env(s);
	struct KHttpHeader *av;
	const char *meth = rq->getMethod();
	if (meth == NULL) {
		return false;
	}
	int via_inserted = FALSE;
	bool x_forwarded_for_inserted = false;
	int defaultPort = 80;
	if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
		defaultPort = 443;
	}
	KUrl *url = rq->url;
	if (TEST(rq->filter_flags, RF_PROXY_RAW_URL) || !TEST(rq->raw_url.flags, KGL_URL_REWRITED)) {
		url = &rq->raw_url;
	}
	char *path = url->path;
	if (url == rq->url && TEST(url->flags, KGL_URL_ENCODE)) {
		size_t path_len;
		path = url_encode(url->path, strlen(url->path), &path_len);
	}
	*s << meth;
	s->WSTR(" ");
	if (TEST(rq->filter_flags, RF_PROXY_FULL_URL)) {
		if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
			s->WSTR("https");
		} else {
			s->WSTR("http");
		}
		s->WSTR("://");
		*s << url->host;
		if (url->port != defaultPort) {
			s->WSTR(":");
			*s << url->port;
		}
	}
	*s << path;
	char *param_buf = NULL;
	const char *param = url->getParam(&param_buf);
	if (param) {
		s->WSTR("?");
		*s << param;
	}
	if (param_buf) {
		free(param_buf);
	}
	s->WSTR(" HTTP/1.1\r\nHost: ");
	*s << url->host;
	if (url->port != defaultPort) {
		s->WSTR(":");
		*s << url->port;
	}
	s->WSTR("\r\n");
	av = rq->GetHeader();
	while (av) {
#ifdef HTTP_PROXY
		if (strncasecmp(av->attr, "Proxy-", 6) == 0) {
			goto do_not_insert;
		}
#endif

		if (strcasecmp(av->attr, X_REAL_IP_SIGN) == 0) {
			goto do_not_insert;
		}
		if (TEST(rq->filter_flags, RF_X_REAL_IP)) {
			if (is_attr(av, "X-Real-IP") || is_attr(av, "X-Forwarded-Proto")) {
				goto do_not_insert;
			}
		}
		if (!TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && is_attr(av, "X-Forwarded-For")) {
			if (x_forwarded_for_inserted) {
				goto do_not_insert;
			}
			x_forwarded_for_inserted = true;
			s->WSTR("X-Forwarded-For: ");
			*s << av->val;
			s->WSTR(",");
			*s << rq->getClientIp();
			s->WSTR("\r\n");
			goto do_not_insert;
		}
		if (is_attr(av, "Via") && TEST(rq->filter_flags, RF_VIA)) {
			if (via_inserted) {
				goto do_not_insert;
			}
			insert_via(rq, *s, av->val);
			via_inserted = true;
			goto do_not_insert;
		}
		if (is_attr(av, kgl_expand_string("Connection"))) {
			goto do_not_insert;
		}
		if (TEST(rq->flags, RQ_HAVE_EXPECT) && is_attr(av, "Expect")) {
			goto do_not_insert;
		}
		
		if (is_internal_header(av)) {
			goto do_not_insert;
		}
		s->write_all(av->attr, av->attr_len);
		s->WSTR(": ");
		s->write_all(av->val, av->val_len);
		s->WSTR("\r\n");
	do_not_insert: av = av->next;
	}
	if (TEST(rq->flags, RQ_HAS_CONTENT_LEN)) {
		s->WSTR("Content-Length: ");
		int2string(rq->content_length, tmpbuff);
		*s << tmpbuff;
		s->WSTR("\r\n");
	} else if (rq->fetchObj->IsChunkPost() || TEST(rq->flags, RQ_INPUT_CHUNKED)) {
		s->WSTR("Transfer-Encoding: chunked\r\n");
	}
	if (rq->ctx->lastModified != 0) {
		mk1123time(rq->ctx->lastModified, tmpbuff, sizeof(tmpbuff));
		if (rq->ctx->mt == modified_if_range_date) {
			s->WSTR("If-Range: ");
		} else {
			s->WSTR("If-Modified-Since: ");
		}
		*s << tmpbuff;
		s->WSTR("\r\n");
	} else if (rq->ctx->if_none_match != NULL) {
		if (rq->ctx->mt == modified_if_range_etag) {
			s->WSTR("If-Range: ");
		} else {
			s->WSTR("If-None-Match: ");
		}
		s->write_all(rq->ctx->if_none_match->data, (int)rq->ctx->if_none_match->len);
		s->WSTR("\r\n");
	}
	
	
	if (rq->ctx->internal) {
		s->WSTR("User-Agent: " PROGRAM_NAME "/" VERSION "\r\n");
	}
	if (TEST(rq->flags, RQ_HAS_CONNECTION_UPGRADE)) {
		s->WSTR("Connection: upgrade\r\n");
	} else if (TEST(rq->filter_flags, RF_UPSTREAM_NOKA) || GetLifeTime() <= 0) {
		s->WSTR("Connection: close\r\n");
	}
	if (!TEST(rq->filter_flags, RF_NO_X_FORWARDED_FOR) && !x_forwarded_for_inserted) {
		*s << "X-Forwarded-For: " << rq->getClientIp();
		s->WSTR("\r\n");
	}
	if (TEST(rq->filter_flags, RF_VIA) && !via_inserted) {
		insert_via(rq, *s, NULL);
	}
	if (TEST(rq->filter_flags, RF_X_REAL_IP)) {
		s->WSTR(X_REAL_IP_HEADER);
		s->WSTR(": ");
		*s << rq->getClientIp();
		s->WSTR("\r\n");
		if (TEST(rq->raw_url.flags, KGL_URL_SSL)) {
			s->WSTR("X-Forwarded-Proto: https\r\n");
		} else {
			s->WSTR("X-Forwarded-Proto: http\r\n");
		}
	}
	s->WSTR("\r\n");
	if (path != url->path) {
		xfree(path);
	}
	return true;
}
