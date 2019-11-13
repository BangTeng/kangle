#include "KDechunkContext.h"
#include "KHttpSink.h"
kev_result result_dechunk_context(void *arg, int got)
{
	KHttpSink *sink = (KHttpSink *)arg;
	KDechunkContext *dechunk = sink->GetDechunkContext();
	return dechunk->ReadResult(sink, got);
}
int buffer_read_http_sink2(void *arg, LPWSABUF buf, int bufCount)
{
	KHttpSink *sink = (KHttpSink *)arg;
	int bc = ks_get_write_buffers(&sink->buffer, buf, bufCount);
	//printf("buf_len=[%d]\n", buf[0].len);
	return bc;
}
kev_result KDechunkContext::ReadResult(KHttpSink *sink, int got)
{
	//printf("ReadResult got=[%d]\n", got);
	if (got <= 0) {
		return result(arg, -1);
	}
	ks_write_success(&sink->buffer, got);
	hot_len += got;
	return ParseBuffer(sink);
}
kev_result KDechunkContext::Read(KHttpSink *sink, void *arg, result_callback result, buffer_callback buffer)
{
	this->arg = arg;
	this->result = result;
	this->buffer = buffer;
	if (chunk_left > 0) {
		return ReadChunk(sink);
	}
	if (hot==NULL) {
		hot = sink->buffer.buf;
		hot_len = sink->buffer.used;
	}
	if (hot_len > 0) {
		return ParseBuffer(sink);
	}
	sink->buffer.used = 0;
	hot = sink->buffer.buf;
	return selectable_read(&sink->cn->st, result_dechunk_context, buffer_read_http_sink2, sink);
}
kev_result KDechunkContext::ParseBuffer(KHttpSink *sink)
{
	kassert(chunk_left == 0);
	for (;;) {
		chunk = NULL;
		switch (eng.dechunk(&hot, hot_len, &chunk, chunk_left)) {
		case dechunk_failed:
			return result(arg, -1);
		case dechunk_end:
			if (raw_read) {
				chunk_left = hot - sink->buffer.buf;
				chunk = sink->buffer.buf;
			} else {
				chunk_left = 0;
			}
			return ReadChunk(sink);
		case dechunk_continue:
			if (raw_read) {				
				chunk_left = sink->buffer.used;
				chunk = sink->buffer.buf;
				kassert(chunk_left > 0);
				return ReadChunk(sink);
			} else if (chunk && buffer) {
				return ReadChunk(sink);
			}
			chunk_left = 0;
			sink->buffer.used = 0;
			hot = sink->buffer.buf;
			hot_len = 0;
			return selectable_read(&sink->cn->st, result_dechunk_context, buffer_read_http_sink2, sink);
		case dechunk_success:
			if (raw_read || buffer == NULL) {
				continue;
			}
			return ReadChunk(sink);
		}
	}
	return kev_err;
}
kev_result KDechunkContext::ReadChunk(KHttpSink *sink)
{
	kassert(sink->buffer.used >= chunk_left);
	int len;
	if (chunk_left == 0) {
		ks_save_point(&sink->buffer, hot, hot_len);
		hot_len = 0;
		hot = NULL;
		return result(arg, 0);
	}
	if (buffer) {
		WSABUF buf;
		int bc = buffer(arg, &buf, 1);
		kassert(bc == 1);
		len = MIN((int)buf.iov_len, chunk_left);
		if (len > 0) {
			memcpy(buf.iov_base, chunk, len);
			chunk_left -= len;
			chunk += len;
			if (raw_read) {
				ks_save_point(&sink->buffer, chunk, sink->buffer.used - len);
				hot = NULL;
			}
		}
	} else {
		len = chunk_left;
		chunk_left = 0;
	}
	return result(arg, len);
}
