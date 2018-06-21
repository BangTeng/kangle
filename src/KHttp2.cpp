#include "KHttp2.h"
#include "KSSLSocket.h"
#include "KSelector.h"
#include "KConnectionSelectable.h"
#include "KHttpRequest.h"
#include "KHttpObjectParserHook.h"
#ifdef ENABLE_HTTP2
http2_buff *get_frame(uint32_t sid, size_t length, uint8_t type, u_char flags)
{
	http2_buff *buf = new http2_buff;
	http2_frame_header *h = (http2_frame_header *)malloc(length + sizeof(http2_frame_header));
	buf->used = length + sizeof(http2_frame_header);
	memset(h, 0, length + sizeof(http2_frame_header));
	buf->data = (char *)h;
	h->set_length_type(length, type);
	h->flags = flags;
	h->stream_id = htonl(sid);
	return buf;
}
kgl_http_v2_handler_pt KHttp2::kgl_http_v2_frame_states[] = {
	&KHttp2::state_data,
	&KHttp2::state_headers,
	&KHttp2::state_priority,
	&KHttp2::state_rst_stream,
	&KHttp2::state_settings,
	&KHttp2::state_push_promise,
	&KHttp2::state_ping,
	&KHttp2::state_goaway,
	&KHttp2::state_window_update,
	&KHttp2::state_continuation
};
#define KGL_HTTP_V2_FRAME_STATES                                              \
    (sizeof(kgl_http_v2_frame_states) / sizeof(kgl_http_v2_handler_pt))
#define MAX_FRAME_BODY  65536
void dump_hex(void *d,int len)
{
	unsigned char *s = (unsigned char *)d;
	for(int i=0;i<len;i++){
		printf("%02x ",s[i]);
	}
	printf("\n");
}
void resultHttp2Read(void *arg,int got)
{
	KConnectionSelectable *c = (KConnectionSelectable *)arg;
	assert(c->http2);
	c->http2->resultRead(got);
}
void resultHttp2Write(void *arg,int got)
{
	KConnectionSelectable *c = (KConnectionSelectable *)arg;
	assert(c->http2);
	c->http2->resultWrite(got);
}

static void syncResultHttp2(void *arg, int got)
{
	kgl_sync_result *sr = (kgl_sync_result *)arg;
	sr->got = got;
	sr->cond.notice();
}
static void syncBufferHttp2(void *arg, iovec *buf, int &bufCount)
{
	kgl_sync_result *sr = (kgl_sync_result *)arg;
	int n = MIN(bufCount, sr->bufCount);
	for (int i = 0; i < n; i++) {
		buf[i].iov_base = sr->buf[i].iov_base;
		buf[i].iov_len = sr->buf[i].iov_len;
	}
	bufCount = n;

}
void bufferHttp2Read(void *arg,iovec *buf,int &bufCount)
{
	KConnectionSelectable *c = (KConnectionSelectable *)arg;
	assert(c->http2);
	c->http2->getReadBuffer(buf,bufCount);
}
void bufferHttp2Write(void *arg,iovec *buf,int &bufCount)
{
	KConnectionSelectable *c = (KConnectionSelectable *)arg;
	assert(c->http2);
	c->http2->getWriteBuffer(buf,bufCount);
}
void next_sync_send_header(void *arg, int got)
{
	kgl_sync_result *sr = (kgl_sync_result *)arg;
	sr->got = sr->http2->send_header(sr->ctx, sr->send_header_body_len);
	sr->cond.notice();
}
void next_sync_write(void *arg, int got)
{
	kgl_sync_result *sr = (kgl_sync_result *)arg;
	sr->http2->write(sr->ctx, syncResultHttp2, syncBufferHttp2, sr);
}
void next_sync_read(void *arg, int got)
{
	kgl_sync_result *sr = (kgl_sync_result *)arg;
	sr->http2->read(sr->ctx, syncResultHttp2, syncBufferHttp2, sr);
}
static bool construct_cookie_header(KHttpRequest *r)
{
	char                     *buf, *p, *end;
	size_t                      len;
	kgl_str_t                  *vals;
	size_t                  i;
	kgl_array_t                *cookies;

	kgl_http_v2_header_t h;
	kgl_str_set(&h.name, "cookie");	

	cookies = r->http2_ctx->cookies;

	if (cookies == NULL) {
		return true;
	}

	vals = (kgl_str_t *)cookies->elts;

	i = 0;
	len = 0;

	do {
		len += vals[i].len + 2;
	} while (++i != cookies->nelts);

	len -= 2;

	buf = (char *)kgl_pnalloc(r->pool, len + 1);
	if (buf == NULL) {
		return false;
	}
	p = buf;
	end = buf + len;

	for (i = 0; /* void */; i++) {

		p = (char *)kgl_cpymem(p, vals[i].data, vals[i].len);

		if (p == end) {
			*p = '\0';
			break;
		}

		*p++ = ';'; *p++ = ' ';
	}

	h.value.len = len;
	h.value.data = buf;
	r->parser.parseHeader(&h.name, &h.value, r);
	return true;
}
KHttp2::KHttp2()
{
	//printf("new http2=[%p]\n", this);
	memset(this, 0, sizeof(*this));
	streams_index = (KHttp2Node **)malloc(sizeof(KHttp2Node *)*kgl_http_v2_index_size());
	memset(streams_index, 0, sizeof(KHttp2Node *)*kgl_http_v2_index_size());
}
KHttp2::~KHttp2()
{
	//printf("~KHttp2 called [%p]\n",this);
	assert(write_processing == 0 && read_processing == 0 && processing == 0);
	for (int i = 0; i < kgl_http_v2_index_size(); i++) {
		KHttp2Node *node = streams_index[i];
		assert(node == NULL);
		while (node) {
			streams_index[i] = node->index;
			delete node;
			node = streams_index[i];
		}
	}
	if (state.stream) {
		if(!state.stream->parsed_header
			
		){
			//incomplete stream
			assert(state.stream->request);
			if (state.stream->request) {
				delete state.stream->request;
			}
			delete state.stream;
		} else if (state.stream->destroy_by_http2) {
			//must destroy by http2
			delete state.stream;
		}
		state.stream = NULL;
	} 
	if (!state.keep_pool && state.pool) {
		kgl_destroy_pool(state.pool);
		state.pool = NULL;
	}
	free(streams_index);
}
bool KHttp2::send_settings(bool ack)
{
	int setting_frame_count = 2;
	if (KGL_HTTP_V2_STREAM_RECV_WINDOW != KGL_HTTP_V2_DEFAULT_WINDOW) {
		setting_frame_count++;
	}
	int len = ack ? 0 : (sizeof(http2_frame_setting)) * setting_frame_count;
	http2_buff *buf = new http2_buff;
	buf->data = (char *)malloc(len + sizeof(http2_frame_header));
	buf->used = len + sizeof(http2_frame_header);
	memset(buf->data, 0, len + sizeof(http2_frame_header));
	http2_frame_header *h = (http2_frame_header *)buf->data;
	h->set_length_type(len, KGL_HTTP_V2_SETTINGS_FRAME);
	if (ack) {
		h->flags = KGL_HTTP_V2_ACK_FLAG;
	}
	if (!ack) {
		http2_frame_setting *setting = (http2_frame_setting *)(h + 1);
		setting->id = htons(KGL_HTTP_V2_MAX_STREAMS_SETTING);
		setting->value = htonl(max_stream);		
		if (KGL_HTTP_V2_STREAM_RECV_WINDOW != KGL_HTTP_V2_DEFAULT_WINDOW) {
			setting += 1;
			setting->id = htons(KGL_HTTP_V2_INIT_WINDOW_SIZE_SETTING);
			setting->value = htonl(KGL_HTTP_V2_STREAM_RECV_WINDOW);
		}
		setting += 1;
		setting->id = htons(KGL_HTTP_V2_MAX_FRAME_SIZE_SETTING);
		setting->value = htonl(KGL_HTTP_V2_MAX_FRAME_SIZE);
	}
	//buf->tcp_nodelay = 1;
	write_buffer.push(buf);
	startWrite();
	return true;
}
bool KHttp2::send_window_update(uint32_t sid, size_t window)
{
	if (window == 0) {
		return false;
	}
	//printf("stream id=[%d] send windows update [%d]\n", sid, window);
	http2_buff *buf = get_frame(sid, sizeof(http2_frame_window_update), KGL_HTTP_V2_WINDOW_UPDATE_FRAME, 0);
	http2_frame_window_update *b = (http2_frame_window_update *)(buf->data + sizeof(http2_frame_header));	
	b->inc_size = htonl(window);
	buf->tcp_nodelay = 1;
	write_buffer.push(buf);
	return true;
}
void KHttp2::getReadBuffer(iovec *buf,int &bufCount)
{
	bufCount = 1;
	buf[0].iov_base = (char *)(state.buffer + state.buffer_used);
	buf[0].iov_len = sizeof(state.buffer) - state.buffer_used;
#ifndef NDEBUG
	if (buf[0].iov_len > 1) {
		//buf[0].iov_len = 1;
	}
#endif
}
void KHttp2::getWriteBuffer(iovec *buf,int &bufCount)
{
	write_buffer.getReadBuffer(c->socket,buf,bufCount);
}
u_char *KHttp2::close(bool read,int status)
{
	//printf("%lld http2 [%p] close read=[%d] status=[%d]\n", kgl_current_sec,this,read,status);
	http2_buff *buf = NULL;
#ifdef MALLOCDEBUG
	//c->socket->shutdown(SHUT_WR);
	//c->selector->removeList(c);
	//c->selector = NULL;
#endif
	//close
	c->set_flag(STF_ERR);
	bool clean_write_buffer = false;
	if (read) {
		assert(read_processing == 1);
		read_processing = 0;
		self_goaway = 1;
	} else {
		clean_write_buffer = true;
		assert(write_processing == 1);
		write_processing = 0;
	}
	//clean write_buffer
	if (clean_write_buffer && write_buffer.getBufferSize()>0) {
		buf = write_buffer.clean();
	}
	KHttp2Node *node;
	KHttp2Context *stream;
	int size = kgl_http_v2_index_size();
	kgl_http2_event *wait_event = NULL;
	//clean wait event
	for (int i = 0; i < size; i++) {
		for (node = streams_index[i]; node; node = node->index) {
			stream = node->stream;
			if (stream == NULL) {
				continue;
			}
			stream->in_closed = 1;
			stream->out_closed = 1;
			stream->rst = 1;
			if (stream->read_wait) {
				stream->read_wait->next = wait_event;
				wait_event = stream->read_wait;
				stream->read_wait = NULL;
			}
			if (stream->write_wait && stream->write_wait->buffer==NULL) {
				stream->write_wait->next = wait_event;
				wait_event = stream->write_wait;
				stream->write_wait = NULL;
			}			
		}
	}
	bool destroy_flag = can_destroy();
	kgl_http2_event *next_event;
	while (wait_event) {
		assert(!destroy_flag);
		wait_event->result(wait_event->arg, -1);
		next_event = wait_event->next;
		delete wait_event;
		wait_event = next_event;
	}
	if (buf) {
		KHttp2WriteBuffer::remove_buff(buf);
	}
	if (destroy_flag) {
		//http2
		destroy();
	}
	return NULL;
}
void KHttp2::resultRead(int got)
{
	//printf("%lld http2=[%p] st=[%p] got=[%d]\n", kgl_current_sec,this,static_cast<KSelectable *>(c), got);
	if (got == ST_ERR_TIME_OUT) {
		if (pinged) {
			c->shutdown_socket();
			return;
		}
		ping();
		return;
	}
	if (got<=0) {
		close(true,KGL_HTTP_V2_CONNECT_ERROR);
		return;
	}
	pinged = 0;
	u_char *p = state.buffer;
	u_char *end = p + state.buffer_used + got;
	state.buffer_used = 0;
	state.incomplete = 0;
	do {
		p = (this->*state.handler)(p, end);
		if (p == NULL) {			
			return;
		}
	} while (p != end);

	startRead();
}
u_char *KHttp2::handle_continuation( u_char *pos,u_char *end, kgl_http_v2_handler_pt handler)
{
	u_char    *p;
	size_t     len, skip;
	uint32_t   head;

	len = state.length;
	if (state.padding && (size_t)(end - pos) > len) {
		skip = MIN(state.padding, (end - pos) - len);

		state.padding -= skip;

		p = pos;
		pos += skip;
		memmove(pos, p, len);
	}

	if ((size_t)(end - pos) < len + KGL_HTTP_V2_FRAME_HEADER_SIZE) {
		return state_save(pos, end, handler);
	}

	p = pos + len;

	head = kgl_http_v2_parse_uint32(p);

	if (kgl_http_v2_parse_type(head) != KGL_HTTP_V2_CONTINUATION_FRAME) {
		klog(KLOG_WARNING,"client sent inappropriate frame while CONTINUATION was expected\n");
		return this->close(true,KGL_HTTP_V2_PROTOCOL_ERROR);
	}

	state.length += kgl_http_v2_parse_length(head);
	state.flags |= p[4];

	if (state.sid != kgl_http_v2_parse_sid(&p[5])) {
		klog(KLOG_WARNING, 
			"client sent CONTINUATION frame with incorrect identifier\n");

		return this->close(true,KGL_HTTP_V2_PROTOCOL_ERROR);
	}

	p = pos;
	pos += KGL_HTTP_V2_FRAME_HEADER_SIZE;

	memcpy(pos, p, len);
	state.handler = handler;
	return pos;
}
bool KHttp2::add_cookie(kgl_http_v2_header_t *header)
{
	kgl_str_t    *val;
	kgl_array_t  *cookies;
	KHttpRequest *r = state.stream->request;
	cookies = state.stream->cookies;

	if (cookies == NULL) {
		cookies = kgl_array_create(r->pool, 2, sizeof(kgl_str_t));
		if (cookies == NULL) {
			return false;
		}

		state.stream->cookies = cookies;
	}

	val = (kgl_str_t *)kgl_array_push(cookies);
	if (val == NULL) {
		return false;
	}

	val->len = header->value.len;
	val->data = header->value.data;

	return true;
}


u_char * KHttp2::state_process_header( u_char *pos,	u_char *end)
{
	size_t                      len;
	//intptr_t                   rc;
	//kgl_table_elt_t            *h;
	KHttpRequest	         *r;
	kgl_http_v2_header_t       *header;

	static kgl_str_t cookie = kgl_string("cookie");

	header = &state.header;

	if (state.parse_name) {
		state.parse_name = 0;

		header->name.len = state.field_end - state.field_start;
		header->name.data = (char *)state.field_start;

		return state_field_len( pos, end);
	}

	if (state.parse_value) {
		state.parse_value = 0;
		header->value.len = state.field_end - state.field_start;
		header->value.data = (char *)state.field_start;
	}

	len = header->name.len + header->value.len;

	if (len > state.header_limit) {
		klog(KLOG_ERR, "client exceeded http2_max_header_size limit\n");
		return this->close(true,KGL_HTTP_V2_ENHANCE_YOUR_CALM);
	}

	state.header_limit -= len;

	if (state.index) {
		if (!add_header(header)) {
			klog(KLOG_ERR, "http2 cann't add_header\n");
			return this->close(true,KGL_HTTP_V2_INTERNAL_ERROR);
		}
		state.index = 0;
	}

	if (state.stream == NULL) {
		return state_header_complete(pos, end);
	}
	if (header->name.data == NULL || header->value.data == NULL) {
		return state_header_complete(pos, end);
	}
	//printf("request name=[%s][%d %d] val=[%s][%d]\n", header->name.data, header->name.len, strlen(header->name.data), header->value.data, header->value.len);
	
	if (header->name.len == cookie.len	
		&& memcmp(header->name.data, cookie.data, cookie.len) == 0) {
		if (!add_cookie(header)) {
			return this->close(true,KGL_HTTP_V2_INTERNAL_ERROR);
		}
		return state_header_complete(pos, end);
	}
	r = state.stream->request;
	r->parser.parseHeader(&header->name, &header->value, r);

	/* TODO Optimization: validate headers while parsing. */
	return state_header_complete(pos, end);
/*
error:

	state.stream = NULL;
	state.pool = NULL;

	return state_header_complete(pos, end);
*/
}


u_char *KHttp2::state_header_block(u_char *pos, u_char *end)
{
	u_char      ch;
	intptr_t   value;
	uintptr_t  indexed, size_update, prefix;

	if (end - pos < 1) {
		return state_save(pos, end,&KHttp2::state_header_block);
	}

	if (!(state.flags & KGL_HTTP_V2_END_HEADERS_FLAG)
		&& state.length < KGL_HTTP_V2_INT_OCTETS)
	{
		return handle_continuation(pos, end,&KHttp2::state_header_block);
	}

	size_update = 0;
	indexed = 0;

	ch = *pos;

	if (ch >= (1 << 7)) {
		/* indexed header field */
		indexed = 1;
		prefix = kgl_http_v2_prefix(7);

	}
	else if (ch >= (1 << 6)) {
		/* literal header field with incremental indexing */
		state.index = 1;
		prefix = kgl_http_v2_prefix(6);

	} else if (ch >= (1 << 5)) {
		/* dynamic table size update */
		size_update = 1;
		prefix = kgl_http_v2_prefix(5);

	} else if (ch >= (1 << 4)) {
		/* literal header field never indexed */
		prefix = kgl_http_v2_prefix(4);

	} else {
		/* literal header field without indexing */
		prefix = kgl_http_v2_prefix(4);
	}

	value = parse_int(&pos, end, prefix);
	if (value < 0) {
		if (value == KGL_AGAIN) {
			return state_save( pos, end, &KHttp2::state_header_block);
		}
		if (value == KGL_DECLINED) {
			klog(KLOG_WARNING,"client sent header block with too long %s value\n",
				size_update ? "size update" : "header index");

			return this->close(true,KGL_HTTP_V2_COMP_ERROR);
		}
		klog(KLOG_WARNING,	"client sent header block with incorrect length\n");
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	if (indexed) {
		if (!get_indexed_header(value, false)) {
			klog(KLOG_ERR, "http2 cann't get indexed header [%d] name_only=false\n", value);
			return this->close(true, KGL_HTTP_V2_COMP_ERROR);
		}
		return state_process_header(pos, end);
	}
	if (size_update) {
		if (!table_size(value)) {
			return this->close(true, KGL_HTTP_V2_COMP_ERROR);
		}
		return state_header_complete(pos, end);
	}

	if (value == 0) {
		state.parse_name = 1;
	} else if (!get_indexed_header(value, true)) {
		klog(KLOG_ERR, "http2 cann't get indexed header [%d] name_only=true\n", value);
		return this->close(true, KGL_HTTP_V2_COMP_ERROR);
	}
	state.parse_value = 1;
	return state_field_len(pos, end);
}

u_char * KHttp2::state_skip_padded(u_char *pos,u_char *end)
{
	state.length += state.padding;
	state.padding = 0;
	return state_skip(pos, end);
}

u_char *KHttp2::state_header_complete(u_char *pos,u_char *end)
{
	KHttp2Context  *stream;

	if (state.length) {
		state.handler = &KHttp2::state_header_block;
		return pos;
	}

	if (!(state.flags & KGL_HTTP_V2_END_HEADERS_FLAG)) {
		return handle_continuation(pos, end,&KHttp2::state_header_complete);
	}
	stream = state.stream;
	if (stream) {
		//printf("%lld http2=[%p] stream=[%d] header complete\n", kgl_current_sec,this, stream->node->id);
		
			if (!construct_cookie_header(stream->request)) {
				return this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
			}
			KHttpRequest *rq = stream->request;
			stream->parsed_header = 1;
			assert(processing >= 0);
			processing++;
			state.stream = NULL;
			handleStartRequest(rq, 0);
		
	} 
	if (!state.keep_pool && state.pool) {
		kgl_destroy_pool(state.pool);
	}
	state.pool = NULL;
	state.keep_pool = 0;
	if (state.padding) {
		return state_skip_padded(pos, end);
	}
	return state_complete( pos, end);
}

intptr_t KHttp2::parse_int(u_char **pos, u_char *end, uintptr_t prefix)
{
	u_char      *start, *p;
	uintptr_t   value, octet, shift;

	start = *pos;
	p = start;

	value = *p++ & prefix;

	if (value != prefix) {
		if (state.length == 0) {
			return KGL_ERROR;
		}

		state.length--;

		*pos = p;
		return value;
	}

	if (end - start > KGL_HTTP_V2_INT_OCTETS) {
		end = start + KGL_HTTP_V2_INT_OCTETS;
	}

	for (shift = 0; p != end; shift += 7) {
		octet = *p++;

		value += (octet & 0x7f) << shift;

		if (octet < 128) {
			if ((size_t)(p - start) > state.length) {
				return KGL_ERROR;
			}

			state.length -= p - start;

			*pos = p;
			return value;
		}
	}

	if ((size_t)(end - start) >= state.length) {
		return KGL_ERROR;
	}

	if (end == start + KGL_HTTP_V2_INT_OCTETS) {
		return KGL_DECLINED;
	}

	return KGL_AGAIN;
	
}
KHttp2Context *KHttp2::create_stream()
{
	KHttp2Context *stream = new KHttp2Context(init_window);
	
	state.keep_pool = 1;
	stream->request = new KHttpRequest(c);
	stream->request->init(state.pool);	
	stream->request->workModel = c->ls->model;
	stream->request->http2_ctx = stream;
	SET(stream->request->raw_url.flags, KGL_URL_SSL);	
	return stream;
}
void KHttp2::setDependency(KHttp2Node *node, uint32_t depend, bool exclusive)
{

}
void KHttp2::resultWrite(int got)
{	
	if (got<=0) {
		close(false,KGL_HTTP_V2_CONNECT_ERROR);
		return;
	}
	http2_buff *remove_list = write_buffer.readSuccess(c->socket,got);
	KHttp2WriteBuffer::remove_buff(remove_list);
	assert(write_processing == 1);
	if (write_buffer.getBufferSize() > 0) {
		c->async_write(c, resultHttp2Write, bufferHttp2Write);
		return;
	}
	write_processing = 0;
	return;
}
void KHttp2::startRead()
{
	if (processing == 0 &&
		self_goaway ==0 &&
		kgl_current_msec - last_stream_msec > (conf.time_out + 30)*1000) {
		goaway(0);
	}
	if (processing == 0 && write_processing == 0 && peer_goaway) {
		close(true, KGL_HTTP_V2_NO_ERROR);
		return;
	}
	c->async_read(c, resultHttp2Read, bufferHttp2Read);
}
void KHttp2::destroy()
{
	if (state.stream) {
		state.stream->out_closed = 1;
		if (state.stream->node) {
			destroy_node(state.stream->node);
		}
		delete state.stream;
		state.stream = NULL;
		state.pool = NULL;
	}
	assert(state.pool == NULL);
	if (state.pool) {
		kgl_destroy_pool(state.pool);		
	}
	c->real_destroy();
}
void KHttp2::goaway(int error_code)
{
	//printf("self_goaway\n");
	self_goaway = 1;
	http2_buff *buf = get_frame(0, sizeof(http2_frame_goaway), KGL_HTTP_V2_GOAWAY_FRAME, 0);
	http2_frame_goaway *b = (http2_frame_goaway *)(buf->data + sizeof(http2_frame_header));
	b->last_stream_id = htonl(last_peer_sid);
	b->error_code = htonl(error_code);
	buf->tcp_nodelay = 1;
	write_buffer.push(buf);
	startWrite();
}
void KHttp2::ping()
{
	if (self_goaway && (processing == 0 && write_processing == 0)) {
		//printf("shutdown socket\n");
		c->shutdown_socket();
		return;
	}
	//printf("ping\n");
	pinged = 1;
	http2_buff *buf = get_frame(0, sizeof(http2_frame_ping), KGL_HTTP_V2_PING_FRAME, 0);
	http2_frame_ping *b = (http2_frame_ping *)(buf->data + sizeof(http2_frame_header));
	b->opaque = time(NULL);
	buf->tcp_nodelay = 1;
	write_buffer.push(buf);
	startWrite();
}
void KHttp2::startWrite()
{
	assert(c->selector->is_same_thread());
	if (write_processing == 1 || write_buffer.getBufferSize() == 0) {
		return;
	}
	write_processing = 1;
	c->async_write(c, resultHttp2Write, bufferHttp2Write);
	return;

}
void KHttp2::write_end(KHttp2Context *ctx)
{
	assert(c->selector->is_same_thread());
	if (ctx->out_closed) {
		return;
	}
	//printf("ctx id=[%d] out_closed\n", ctx->node->id);
	ctx->out_closed = 1;
	http2_buff *new_buf = get_frame(ctx->node->id,0, KGL_HTTP_V2_DATA_FRAME, KGL_HTTP_V2_END_STREAM_FLAG);
	new_buf->tcp_nodelay = 1;
	write_buffer.push(new_buf);
	startWrite();
}
void KHttp2::destroy_node(KHttp2Node *node)
{
	uint8_t index = kgl_http_v2_index(node->id);
	KHttp2Node *last = NULL;
	KHttp2Node *n = streams_index[index];
	while (n) {
		if (n->id == node->id) {
			if (last == NULL) {
				streams_index[index] = n->index;
			} else {
				last->index = n->index;
			}
			break;
		}
		last = n;
		n = n->index;
		assert(n);
	}
	delete node;
}
void KHttp2::release_stream(KHttp2Context *ctx)
{
	assert(c->selector->is_same_thread());
	assert(ctx->read_wait == NULL);
	assert(ctx->write_wait == NULL);
	if (ctx->node) {
		last_stream_msec = kgl_current_msec;
		if (ctx->read_wait) {
			delete ctx->read_wait;
			ctx->read_wait = NULL;
		}
		if (ctx->write_wait) {
			delete ctx->write_wait;
			ctx->write_wait = NULL;
		}
		if (!ctx->out_closed || !ctx->in_closed) {			
			ctx->out_closed = 1;
			ctx->in_closed = 1;
			ctx->rst = 1;
			send_rst_stream(ctx->node->id, KGL_HTTP_V2_INTERNAL_ERROR);
		}
		destroy_node(ctx->node);
		ctx->node = NULL;
	}
}
void KHttp2::release_admin(KHttp2Context *ctx)
{	
	assert(c->selector->is_same_thread());
	
	if (state.stream == ctx) {
		//the ctx is current using by state
		ctx->destroy_by_http2 = 1;
	} else {
		delete ctx;
	}
	processing--;
	assert(processing >= 0);
	if (can_destroy()) {
		destroy();
	}
}
void KHttp2::release(KHttp2Context *ctx)
{
	release_stream(ctx);
	release_admin(ctx);
}
int KHttp2::copyReadBuffer(KHttp2Context *ctx,bufferEvent buffer,void *arg)
{
	WSABUF vc[16];
	int bufferCount = 16;
	int total_send = 0;
	if (ctx->read_buffer->getHeader() == NULL) {
		return 0;
	}
	buffer(arg,vc,bufferCount);
	for (int i=0;i<bufferCount;i++) {
		while (vc[i].iov_len>0) {
			int this_len;
			char *data = ctx->read_buffer->getReadBuffer(this_len);
			this_len = MIN(this_len,(int)vc[i].iov_len);
			memcpy(vc[i].iov_base,data,this_len);
			total_send += this_len;
			vc[i].iov_len -= this_len;
			vc[i].iov_base = (char *)vc[i].iov_base + this_len;
			if (!ctx->read_buffer->readSuccess(this_len)) {
				goto done;
			}
		}
	}
done:
	return total_send;
}
void KHttp2::read_header(KHttp2Context *http2_ctx, resultEvent result, void *arg)
{
	assert(c->selector->is_same_thread());
	if (http2_ctx->rst) {
		result(arg, -1);
		return;
	}
	if (http2_ctx->parsed_header) {
		result(arg, 0);
		return;
	}
	assert(http2_ctx);
	assert(http2_ctx->read_wait == NULL);
	http2_ctx->read_wait = new kgl_http2_event;
	http2_ctx->read_wait->buffer = NULL;
	http2_ctx->read_wait->result = result;
	http2_ctx->read_wait->arg = arg;
}
bool KHttp2::check_recv_window(KHttp2Context *http2_ctx)
{
	if (http2_ctx->in_closed) {
		return false;
	}
	int recv_window = http2_ctx->recv_window;
	if (http2_ctx->read_buffer) {
		recv_window += http2_ctx->read_buffer->getLength();
	}
	if (recv_window < KGL_HTTP_V2_STREAM_RECV_WINDOW / 4) {
		//printf("stream [%d] recv_window is too small stream_recv_window=[%d],add_read_buffer size=[%d]\n", http2_ctx->node->id, http2_ctx->recv_window, recv_window);
		bool send_window_flag = send_window_update(http2_ctx->node->id, KGL_HTTP_V2_STREAM_RECV_WINDOW - http2_ctx->recv_window);
		http2_ctx->recv_window = KGL_HTTP_V2_STREAM_RECV_WINDOW;
		return send_window_flag;
	}
	return false;
}
bool KHttp2::terminate_stream(KHttp2Context *ctx, uint32_t status)
{
	kgl_http2_event *e = NULL;
	kgl_http2_event *next;
	bool send_flag;
	if (ctx->read_wait) {
		ctx->read_wait->next = e;
		e = ctx->read_wait;
		ctx->read_wait = NULL;
	}
	if (ctx->write_wait) {
		ctx->write_wait->next = e;
		e = ctx->write_wait;
		ctx->write_wait = NULL;
	}
	if (ctx->in_closed && ctx->out_closed) {
		ctx->rst = 1;
		send_flag = false;
	} else {
		ctx->in_closed = 1;
		ctx->out_closed = 1;
		ctx->rst = 1;
		send_flag = send_rst_stream(ctx->node->id, status);
	}
	while (e) {
		next = e->next;
		e->result(e->arg, -1);
		delete e;
		e = next;
	}
	return send_flag;
}
void KHttp2::shutdown(KHttp2Context *ctx)
{
	terminate_stream(ctx, KGL_HTTP_V2_CANCEL);
}
void KHttp2::remove_read_hup(KHttp2Context *http2_ctx)
{
	if (http2_ctx->write_wait && http2_ctx->write_wait->buffer==NULL) {
		delete http2_ctx->write_wait;
		http2_ctx->write_wait = NULL;
	}
}
void KHttp2::read_hup(KHttp2Context *http2_ctx, resultEvent result, void *arg)
{
	assert(c->selector->is_same_thread());
	if (http2_ctx->write_wait) {
		if (http2_ctx->write_wait->buffer != NULL) {
			return;
		}
	}
	if (http2_ctx->read_wait) {
		return;
	}
	if (http2_ctx->write_wait == NULL) {
		http2_ctx->write_wait = new kgl_http2_event;
		memset(http2_ctx->write_wait, 0, sizeof(kgl_http2_event));
	}
	http2_ctx->write_wait->buffer = NULL;
	http2_ctx->write_wait->result = result;
	http2_ctx->write_wait->arg = arg;
	http2_ctx->write_wait->len = -1;
}
void KHttp2::read(KHttp2Context *http2_ctx,resultEvent result,bufferEvent buffer,void *arg)
{
	assert(c->selector->is_same_thread());
	assert(http2_ctx);
	if (http2_ctx->write_wait && http2_ctx->write_wait->buffer==NULL) {
		//remove read_hup
		delete http2_ctx->write_wait;
		http2_ctx->write_wait = NULL;
	}
	if (http2_ctx->read_wait) {
		delete http2_ctx->read_wait;
		http2_ctx->read_wait = NULL;
	}
	
	//printf("stream_recv_window=[%d],recv_window=[%d]\n", http2_ctx->recv_window, recv_window);
	bool recv_window_result = check_recv_window(http2_ctx);
	if (recv_window_result) {
		startWrite();
	}
	if (http2_ctx->read_buffer && http2_ctx->read_buffer->getHeader()!=NULL) {
		int got = copyReadBuffer(http2_ctx,buffer,arg);
		result(arg, got);
		return;
	}
	if (http2_ctx->rst) {
		result(arg, -1);
		return;
	}
	if (http2_ctx->in_closed) {
		result(arg,0);
		return;
	}
	http2_ctx->read_wait = new kgl_http2_event;
	http2_ctx->read_wait->buffer = buffer;
	http2_ctx->read_wait->result = result;
	http2_ctx->read_wait->arg = arg;
}
void KHttp2::write(KHttp2Context *http2_ctx,resultEvent result,bufferEvent buffer,void *arg)
{
	if (http2_ctx->write_wait) {
		assert(http2_ctx->write_wait->buffer == NULL);
		delete http2_ctx->write_wait;
		http2_ctx->write_wait = NULL;
	}
	assert(c->selector->is_same_thread());
	if (http2_ctx->out_closed) {
		result(arg, -1);
		return;
	}
	//printf("stream_send_window=[%d] send_window=[%d]\n",http2_ctx->send_window,send_window);
	if (http2_ctx->send_window <= 0 || send_window<=0) {
		http2_ctx->write_wait =  new kgl_http2_event;
		http2_ctx->write_wait->arg = arg;
		http2_ctx->write_wait->result = result;
		http2_ctx->write_wait->buffer = buffer;
		http2_ctx->write_wait->len = -1;
		return;
	}
	int len = MIN((int)frame_size, http2_ctx->send_window);
	len = MIN(len, (int)send_window);
	http2_buff *new_buf = http2_ctx->build_write_buffer(result, buffer, arg, len);
	http2_ctx->send_window -= len;
	send_window -= len;
	write_buffer.push(new_buf);
	startWrite();
}
int KHttp2::send_header(KHttp2Context *http2_ctx, INT64 body_len)
{
	assert(c->selector->is_same_thread());
	
	http2_buff *buf = http2_ctx->send_header->create(http2_ctx->node->id, body_len == 0, frame_size);
	if (body_len == 0) {
		//printf("ctx id=[%d] no_body out_closed\n", http2_ctx->node->id);
		http2_ctx->out_closed = 1;
		buf->tcp_nodelay = 1;
	}
	delete http2_ctx->send_header;
	http2_ctx->send_header = NULL;
	http2_ctx->setContentLength(body_len);
	write_buffer.push(buf);
	int header_len = 0;
	while (buf) {
		header_len += buf->used;
		buf = buf->next;
	}
	startWrite();
	return header_len;
}
int KHttp2::sync_send_header(KHttp2Context *http2_ctx, INT64 body_len)
{
	assert(http2_ctx->send_header);
	assert(!c->selector->is_same_thread());
	kgl_sync_result sr;
	sr.http2 = this;
	sr.ctx = http2_ctx;
	sr.send_header_body_len = body_len;
	sr.got = -1;
	if (!c->selector->next(next_sync_send_header, &sr)) {
		return -1;
	}
	sr.cond.wait();
	return sr.got;
}
bool KHttp2::add_method(KHttp2Context *ctx, u_char meth)
{
	assert(ctx->send_header == NULL);
	if (ctx->send_header == NULL) {
		ctx->send_header = new KHttp2HeaderFrame;
	}
	const char *method = KHttpKeyValue::getMethod(meth);
	add_header(ctx, ":method", (hlen_t)sizeof(":method") - 1, method, (hlen_t)strlen(method));
	return true;
}
bool KHttp2::add_status(KHttp2Context *ctx, uint16_t status_code)
{
	u_char  status;
	switch (status_code) {
	case STATUS_OK:
		status = kgl_http_v2_indexed(KGL_HTTP_V2_STATUS_200_INDEX);
		break;
	case STATUS_NO_CONTENT:
		status = kgl_http_v2_indexed(KGL_HTTP_V2_STATUS_204_INDEX);
		break;
	case STATUS_CONTENT_PARTIAL:
		status = kgl_http_v2_indexed(KGL_HTTP_V2_STATUS_206_INDEX);
		break;
	case STATUS_NOT_MODIFIED:
		status = kgl_http_v2_indexed(KGL_HTTP_V2_STATUS_304_INDEX);
		break;
	case STATUS_BAD_REQUEST:
		status = kgl_http_v2_indexed(KGL_HTTP_V2_STATUS_400_INDEX);
		break;
	case STATUS_NOT_FOUND:
		status = kgl_http_v2_indexed(KGL_HTTP_V2_STATUS_404_INDEX);
		break;
	case STATUS_SERVER_ERROR:
		status = kgl_http_v2_indexed(KGL_HTTP_V2_STATUS_500_INDEX);
		break;
	default:
		status = 0;
	}
	if (ctx->send_header == NULL) {
		ctx->send_header = new KHttp2HeaderFrame;
	}
	if (status) {
		ctx->send_header->write((char *)&status, 1);
	} else {
		ctx->send_header->write(kgl_http_v2_inc_indexed(KGL_HTTP_V2_STATUS_INDEX));
		ctx->send_header->write(KGL_HTTP_V2_ENCODE_RAW | 3);
		char buf[4];
		sprintf(buf, "%03u", status_code);
		ctx->send_header->write(buf, 3);
	}
	return true;
}
bool KHttp2::add_header(KHttp2Context *ctx, know_http_header name, const char *val, hlen_t val_len)
{
	return add_header(ctx, know_http_headers[name].data, (hlen_t)know_http_headers[name].len, val, val_len);
}
bool KHttp2::add_header_cookie(KHttp2Context *ctx, const char *val, hlen_t val_len)
{
	ctx->send_header->write(0);
	ctx->send_header->write_int(KGL_HTTP_V2_ENCODE_RAW, kgl_http_v2_prefix(7), sizeof("cookie")-1);
	ctx->send_header->write_lower_string("cookie", sizeof("cookie")-1);
	ctx->send_header->write_int(KGL_HTTP_V2_ENCODE_RAW, kgl_http_v2_prefix(7), val_len);
	ctx->send_header->write(val, val_len);
	return true;
}
bool KHttp2::add_header(KHttp2Context *ctx, const char *name, hlen_t name_len, const char *val, hlen_t val_len)
{
	if (ctx->send_header == NULL) {
		ctx->send_header = new KHttp2HeaderFrame;
	}
	switch (name_len) {
	case 6:
		if (strcasecmp(name, "cookie") == 0) {
			return add_header_cookie(ctx,val, val_len);
		}
	default:
		break;
	}
	ctx->send_header->write(0);
	ctx->send_header->write_int(KGL_HTTP_V2_ENCODE_RAW, kgl_http_v2_prefix(7), name_len);
	ctx->send_header->write_lower_string(name, name_len);
	ctx->send_header->write_int(KGL_HTTP_V2_ENCODE_RAW, kgl_http_v2_prefix(7), val_len);
	ctx->send_header->write(val, val_len);
	//printf("http2 add header name=[%s: %s]\n", name, val);
	return true;
}
void KHttp2::init(KConnectionSelectable *c)
{
	//printf("Http2 init [%p]\n", this);
	this->c = c;
	c->set_flag(STF_RTIME_OUT);
	read_processing = 1;
	send_window = KGL_HTTP_V2_DEFAULT_WINDOW;
	recv_window = KGL_HTTP_V2_CONNECTION_RECV_WINDOW;
	init_window = KGL_HTTP_V2_DEFAULT_WINDOW;
	frame_size = KGL_HTTP_V2_DEFAULT_FRAME_SIZE;
	max_stream = KGL_HTTP_V2_DEFAULT_MAX_STREAM;
	last_stream_msec = kgl_current_msec;
}

void KHttp2::server(KConnectionSelectable *c)
{
	init(c);
	state.handler = &KHttp2::state_preface;
	send_settings(false);
	if (send_window_update(0, KGL_HTTP_V2_CONNECTION_RECV_WINDOW - KGL_HTTP_V2_DEFAULT_WINDOW)) {
		startWrite();
	}
	startRead();
}
int KHttp2::read(KHttp2Context *ctx,char *buf,int len)
{
	assert(!c->selector->is_same_thread());
	kgl_sync_result sr;
	WSABUF b;
	b.iov_base = buf;
	b.iov_len = len;
	sr.http2 = this;
	sr.ctx = ctx;
	sr.buf = &b;
	sr.bufCount = 1;
	sr.got = -1;
	if (!c->selector->next(next_sync_read, &sr)) {
		return -1;
	}
	sr.cond.wait();
	return sr.got;
}
int KHttp2::write(KHttp2Context *ctx,LPWSABUF buf,int bufCount)
{
	assert(!c->selector->is_same_thread());
	kgl_sync_result sr;
	sr.http2 = this;
	sr.ctx = ctx;
	sr.buf = buf;
	sr.bufCount = bufCount;
	sr.got = -1;
	if (!c->selector->next(next_sync_write, &sr)) {
		return -1;
	}
	sr.cond.wait();
	return sr.got;
}

KHttp2Node *KHttp2::get_node(uint32_t sid, bool alloc)
{
	uint8_t               index;
	KHttp2Node      *node;	
	index = kgl_http_v2_index(sid);

	for (node = this->streams_index[index]; node; node = node->index) {
		if (node->id == sid) {
			return node;
		}
	}

	if (!alloc) {
		return NULL;
	}
	node = new KHttp2Node();
	if (node == NULL) {
		return NULL;
	}
	node->id = sid;
	node->index = this->streams_index[index];
	this->streams_index[index] = node;
	return node;
}
u_char *KHttp2::state_preface(u_char *pos, u_char *end)
{
	static const u_char preface[] = "PRI * HTTP/2.0\r\n";
	if (end - pos < (int)sizeof(preface)) {
		return state_save(pos, end, &KHttp2::state_preface);
	}
	if (memcmp(pos, preface, sizeof(preface) - 1) != 0) {
		this->close(true, KGL_HTTP_V2_PROTOCOL_ERROR);
		return NULL;
	}
	return state_preface_end(pos + sizeof(preface) - 1, end);
}
u_char *KHttp2::state_preface_end(u_char *pos, u_char *end)
{
	static const u_char preface[] = "\r\nSM\r\n\r\n";
	if (end - pos < (int)sizeof(preface)) {
		return state_save(pos, end, &KHttp2::state_preface_end);
	}
	if (memcmp(pos, preface, sizeof(preface) - 1) != 0) {
		this->close(true, KGL_HTTP_V2_PROTOCOL_ERROR);
		return NULL;
	}
	return state_head(pos + sizeof(preface) - 1, end);
}
u_char *KHttp2::state_save(u_char *pos, u_char *end, kgl_http_v2_handler_pt handler)
{
	size_t size = end - pos;
	if (size > KGL_HTTP_V2_STATE_BUFFER_SIZE) {
		klog(KLOG_WARNING, 	"state buffer overflow: %d bytes required\n", size);
		this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
		return NULL;
	}
	memcpy(this->state.buffer, pos, size);
	state.buffer_used = size;
	state.handler = handler;
	state.incomplete = 1;
	return end;
}
u_char *KHttp2::state_head(u_char *pos, u_char *end)
{
	uint32_t    head;
	int  type;
	if (end - pos < (int)sizeof(http2_frame_header)) {
		return state_save(pos, end, &KHttp2::state_head);
	}
	head = kgl_http_v2_parse_uint32(pos);
	state.length = kgl_http_v2_parse_length(head);
	state.flags = pos[4];
	state.sid = kgl_http_v2_parse_sid(&pos[5]);
	pos += sizeof(http2_frame_header);
	type = kgl_http_v2_parse_type(head);
	//printf("%lld http2=[%p] recv frame sid=[%d] type=[%d] flag=[%d] length=[%d]\n",kgl_current_sec, this,state.sid,type,state.flags,state.length);
	if (type >= (int)KGL_HTTP_V2_FRAME_STATES) {
		return state_skip(pos, end);
	}

	return (this->*kgl_http_v2_frame_states[type])(pos, end);
}
u_char *KHttp2::state_data(u_char *pos, u_char *end)
{
	KHttp2Node    *node;
	KHttp2Context  *stream;

	if (state.flags & KGL_HTTP_V2_PADDED_FLAG) {
		if (state.length == 0) {
			klog(KLOG_WARNING, "client sent padded DATA frame "
				"with incorrect length: %u\n",state.length);
			return this->close(true,KGL_HTTP_V2_SIZE_ERROR);
		}

		if (end - pos == 0) {
			return state_save(pos, end,&KHttp2::state_data);
		}

		state.padding = *pos++;
		state.length--;

		if (state.padding > state.length) {
			klog(KLOG_WARNING,
				"client sent padded DATA frame "
				"with incorrect length: %u, padding: %u\n",
				state.length, state.padding);

			return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
		}

		state.length -= state.padding;
	}


	if (state.length > recv_window) {
		klog(KLOG_WARNING,
			"client violated connection flow control: "
			"received DATA frame length %u, available window %u\n",
			state.length, recv_window);
		return this->close(true, KGL_HTTP_V2_FLOW_CTRL_ERROR);
	}
	bool send_window_flag = false;
	recv_window -= state.length;
	if (recv_window < KGL_HTTP_V2_CONNECTION_RECV_WINDOW / 4) {
		send_window_flag = send_window_update(0, KGL_HTTP_V2_CONNECTION_RECV_WINDOW - recv_window);
		recv_window = KGL_HTTP_V2_CONNECTION_RECV_WINDOW;
	}

	node = get_node(state.sid, false);
	if (node == NULL || node->stream == NULL) {
		klog(KLOG_DEBUG,"unknown http2 stream");
		if (send_window_flag) {
			startWrite();
		}
		return state_skip_padded(pos, end);
	}

	stream = node->stream;

	if (state.length > stream->recv_window) {
		klog(KLOG_INFO, "client violated flow control for stream %u: "
			"received DATA frame length %u, available window %u",
			node->id, state.length, stream->recv_window);
		terminate_stream(stream, KGL_HTTP_V2_FLOW_CTRL_ERROR);		
		return state_skip_padded(pos, end);
	}

	stream->recv_window -= state.length;
	if (stream->in_closed) {
		klog(KLOG_INFO, "client sent DATA frame for half-closed stream %u\n",	node->id);
		if (!terminate_stream(stream,KGL_HTTP_V2_STREAM_CLOSED) && send_window_flag) {
			startWrite();
		}
		return state_skip_padded(pos, end);
	}
	assert(state.stream == NULL);
	state.stream = stream;
	if (send_window_flag) {
		startWrite();
	}
	return state_read_data(pos, end);

}
u_char *KHttp2::state_read_data(u_char *pos, u_char *end)
{
	size_t                     size;
	//int                    n;
	KHttp2Context *stream;


	stream = state.stream;

	if (stream == NULL) {
		return state_skip_padded(pos, end);
	}

	if (stream->skip_data) {
		klog(KLOG_DEBUG, "skipping http2 DATA frame, reason: %d", stream->skip_data);
		if (state.flags & KGL_HTTP_V2_END_STREAM_FLAG) {
			stream->in_closed = 1;
			kgl_http2_event *wait = stream->read_wait;
			stream->read_wait = NULL;			
			if (wait) {
				wait->result(wait->arg, 0);
				delete wait;
			}
		}		
		return state_skip_padded(pos, end);
	}

	size = end - pos;

	if (size > state.length) {
		size = state.length;
	}

	if (size) {
		//TODO: read size data
		//printf("read data length=[%d]\n", size);
		if (stream->read_buffer == NULL) {
			stream->read_buffer = new KSendBuffer();
		}
		stream->read_buffer->append((char *)pos, (uint16_t)size);
		kgl_http2_event *wait = stream->read_wait;		
		if (wait) {
			stream->read_wait = NULL;
			int got = copyReadBuffer(stream, wait->buffer, wait->arg);
			wait->result(wait->arg, got);
			delete wait;
		}
		state.length -= size;
		pos += size;
	}

	if (state.length) {
		return state_save(pos, end,	&KHttp2::state_read_data);
	}

	if (state.flags & KGL_HTTP_V2_END_STREAM_FLAG) {
		stream->in_closed = 1;
		kgl_http2_event *wait = stream->read_wait;
		stream->read_wait = NULL;		
		if (wait) {
			wait->result(wait->arg, 0);
			delete wait;
		}
	}

	if (state.padding) {
		return state_skip_padded(pos, end);
	}

	return state_complete(pos, end);

}
u_char *KHttp2::state_headers(u_char *pos, u_char *end)
{
	size_t                   size;
	uintptr_t               padded, priority, depend, dependency, excl, weight;
	KHttp2Node      *node;
	KHttp2Context    *stream;

	padded = state.flags & KGL_HTTP_V2_PADDED_FLAG;
	priority = state.flags & KGL_HTTP_V2_PRIORITY_FLAG;

	size = 0;

	if (padded) {
		size++;
	}

	if (priority) {
		size += sizeof(uint32_t) + 1;
	}

	if (state.length < size) {
		klog(KLOG_WARNING, "client sent HEADERS frame with incorrect length %u\n",state.length);
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}
	if (state.length == size) {
		klog(KLOG_WARNING,"client sent HEADERS frame with empty header block\n");
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	if ((size_t)(end - pos) < size) {
		return state_save(pos, end,	&KHttp2::state_headers);
	}

	state.length -= size;

	if (padded) {
		state.padding = *pos++;
		if (state.padding > state.length) {
			klog(KLOG_WARNING, 
				"client sent padded HEADERS frame "
				"with incorrect length: %uz, padding: %u\n",
				state.length, state.padding);

			return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
		}
		state.length -= state.padding;
	}

	depend = 0;
	excl = 0;
	weight = 16;

	if (priority) {
		dependency = kgl_http_v2_parse_uint32(pos);

		depend = dependency & 0x7fffffff;
		excl = dependency >> 31;
		weight = pos[4] + 1;

		pos += sizeof(uint32_t) + 1;
	}
	//limit to 1M
	state.header_limit = 1048576;

	klog(KLOG_DEBUG,"http2 HEADERS frame sid:%ui on %ui excl:%ui weight:%ui",state.sid, depend, excl, weight);

	
	assert(state.pool == NULL);
	state.pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
	if (state.sid % 2 == 0 || state.sid <= last_peer_sid) {
		klog(KLOG_WARNING,
			"client sent HEADERS frame with incorrect identifier "
			"%ui, the last was %u\n", state.sid, last_peer_sid);
		return this->close(true, KGL_HTTP_V2_PROTOCOL_ERROR);
	}
	if (self_goaway) {
		if (!send_rst_stream(state.sid, KGL_HTTP_V2_NO_ERROR)) {
			return this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
		}
		return state_skip_headers(pos, end);
	}
	last_peer_sid = state.sid;
	if (depend == state.sid) {
		klog(KLOG_WARNING,
			"client sent HEADERS frame for stream %u "
			"with incorrect dependency\n", state.sid);

		if (!send_rst_stream(state.sid, KGL_HTTP_V2_PROTOCOL_ERROR)) {
			return this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
		}
		return state_skip_headers(pos, end);
	}
	
	node = get_node(state.sid, true);
	if (node == NULL) {
		return this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
	}
	assert(node->stream == NULL);
	stream = create_stream();
	if (stream == NULL) {
		return this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
	}
	stream->in_closed = state.flags & KGL_HTTP_V2_END_STREAM_FLAG;
	stream->node = node;
	node->stream = stream;
	assert(state.stream == NULL);
	state.stream = stream;
	assert(state.pool == stream->request->pool);
	/*
	if (priority || node->parent == NULL) {
		node->weight = weight;
		lock.Lock();
		this->setDependency(node, depend, excl>0);
		lock.Unlock();
	}
	*/
	return state_header_block(pos, end);
}
u_char *KHttp2::state_priority(u_char *pos, u_char *end)
{
	uintptr_t           depend, dependency, excl, weight;
	//KHttp2Node  *node;
	if (state.length != KGL_HTTP_V2_PRIORITY_SIZE) {
		klog(KLOG_WARNING, "client sent PRIORITY frame with incorrect length %d\n",state.length);
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	if (end - pos < KGL_HTTP_V2_PRIORITY_SIZE) {
		return state_save(pos, end,	&KHttp2::state_priority);
	}

	dependency = kgl_http_v2_parse_uint32(pos);
	depend = dependency & 0x7fffffff;
	excl = dependency >> 31;
	weight = pos[4] + 1;
	pos += KGL_HTTP_V2_PRIORITY_SIZE;
	if (state.sid == 0) {
		klog(KLOG_WARNING,"client sent PRIORITY frame with incorrect identifier\n");
		return this->close(true, KGL_HTTP_V2_PROTOCOL_ERROR);
	}

	if (depend == state.sid) {
		klog(KLOG_WARNING, "client sent PRIORITY frame for stream %u "
			"with incorrect dependency\n", state.sid);
		
		return state_complete(pos, end);
	}
	return state_complete(pos, end);
}
u_char *KHttp2::state_rst_stream(u_char *pos, u_char *end)
{
	uint32_t             status;
	KHttp2Node		*node;
	if (state.length != KGL_HTTP_V2_RST_STREAM_SIZE) {
		klog(KLOG_WARNING, "client sent RST_STREAM frame with incorrect length %u\n",state.length);
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	if (end - pos < KGL_HTTP_V2_RST_STREAM_SIZE) {
		return state_save(pos, end, &KHttp2::state_rst_stream);
	}

	status = kgl_http_v2_parse_uint32(pos);

	pos += KGL_HTTP_V2_RST_STREAM_SIZE;

	klog(KLOG_DEBUG,"http2 RST_STREAM frame, sid:%ui status:%ui",state.sid, status);

	if (state.sid == 0) {
		klog(KLOG_WARNING, "client sent RST_STREAM frame with incorrect identifier\n");
		return this->close(true, KGL_HTTP_V2_PROTOCOL_ERROR);
	}
	node = this->get_node(state.sid, false);

	if (node == NULL || node->stream == NULL) {
		//klog(KLOG_WARNING,"unknown http2 stream [%d]\n",state.sid);
		return state_complete(pos, end);
	}

	switch (status) {

	case KGL_HTTP_V2_CANCEL:
		klog(KLOG_INFO, "client canceled stream %ui", state.sid);
		break;

	case KGL_HTTP_V2_INTERNAL_ERROR:
		klog(KLOG_INFO, "client terminated stream %ui due to internal error",state.sid);
		break;

	default:
		klog(KLOG_INFO,	"client terminated stream %ui with status %ui",state.sid, status);
		break;
	}
	kgl_http2_event *re = node->stream->read_wait;
	node->stream->read_wait = NULL;

	kgl_http2_event *we = NULL;
	if (node->stream->write_wait && node->stream->write_wait->buffer == NULL) {
		we = node->stream->write_wait;
		node->stream->write_wait = NULL;
	}	
	node->stream->in_closed = 1;
	node->stream->out_closed = 1;
	node->stream->rst = 1;
	if (node->stream->read_buffer) {
		node->stream->read_buffer->clean();
	}
	if (re) {
		re->result(re->arg, -1);
		delete re;
	}
	if (we) {
		we->result(we->arg, -1);
		delete we;
	}
	return state_complete(pos, end);
}
u_char *KHttp2::state_settings(u_char *pos, u_char *end)
{
	if (state.flags == KGL_HTTP_V2_ACK_FLAG) {
		if (state.length != 0) {
			klog(KLOG_WARNING,"client sent SETTINGS frame with the ACK flag and nonzero length");
			return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
		}

		/* TODO settings acknowledged */

		return state_complete(pos, end);
	}

	if (state.length % KGL_HTTP_V2_SETTINGS_PARAM_SIZE) {
		klog(KLOG_WARNING,"client sent SETTINGS frame with incorrect length %u\n",	state.length);
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	send_settings(true);

	return state_settings_params(pos, end);
}
void KHttp2::check_write_wait()
{
	KHttp2Node *node;
	KHttp2Context *stream;
	int size = kgl_http_v2_index_size();
	bool buffer_writed = false;
	assert(c->selector->is_same_thread());
	for (int i = 0; i < size; i++) {
		for (node = streams_index[i]; node; node = node->index) {
			if (send_window <= 0) {
				goto done;
			}
			stream = node->stream;
			if (stream == NULL
				|| stream->write_wait==NULL
				|| stream->write_wait->buffer==NULL
				|| stream->send_window <= 0
				|| stream->write_wait->len>=0) {
				continue;
			}
			buffer_writed = true;
			int len = MIN(frame_size, send_window);
			len = MIN(len, stream->send_window);
			http2_buff *buf = stream->build_write_buffer(stream->write_wait, len);
			send_window -= len;
			stream->send_window -= len;
			write_buffer.push(buf);
		}
	}
done:
	if (buffer_writed) {
		startWrite();
	}
}
void KHttp2::adjust_windows(size_t window)
{
	KHttp2Node *node;
	KHttp2Context *stream;
	int size = kgl_http_v2_index_size();

	int delta = window - init_window;
	init_window = window;
	for (int i = 0; i < size; i++) {
		for (node = streams_index[i]; node; node = node->index) {
			stream = node->stream;
			if (stream == NULL) {
				continue;
			}
			if (delta > 0 && stream->send_window	> (int)(KGL_HTTP_V2_MAX_WINDOW - delta)) {
				klog(KLOG_WARNING, "adjust_windows failed window [%d] delta [%d] stream send_window=[%d]\n", window, delta, stream->send_window);
				//terminate_stream(stream, KGL_HTTP_V2_FLOW_CTRL_ERROR);
				continue;
			}
			stream->send_window += delta;
		}
	}
	check_write_wait();
}
u_char * KHttp2::state_settings_params( u_char *pos,u_char *end)
{
	uintptr_t  id, value;

	while (state.length) {
		if (end - pos < KGL_HTTP_V2_SETTINGS_PARAM_SIZE) {
			return state_save(pos, end, &KHttp2::state_settings_params);
		}

		state.length -= KGL_HTTP_V2_SETTINGS_PARAM_SIZE;

		id = kgl_http_v2_parse_uint16(pos);
		value = kgl_http_v2_parse_uint32(&pos[2]);
		//printf("setting params id=[%d] value=[%d]\n", id, value);
		switch (id) {

		case KGL_HTTP_V2_INIT_WINDOW_SIZE_SETTING:

			if (value > KGL_HTTP_V2_MAX_WINDOW) {
				klog(KLOG_WARNING,"client sent SETTINGS frame with incorrect "
					"INITIAL_WINDOW_SIZE value %u\n", value);

				return this->close(true, KGL_HTTP_V2_FLOW_CTRL_ERROR);
			}
			adjust_windows(value);		
			break;

		case KGL_HTTP_V2_MAX_FRAME_SIZE_SETTING:
			if (value > KGL_HTTP_V2_MAX_FRAME_SIZE
				|| value < KGL_HTTP_V2_DEFAULT_FRAME_SIZE)
			{
				klog(KLOG_WARNING, "client sent SETTINGS frame with incorrect "
					"MAX_FRAME_SIZE value %u\n", value);

				return this->close(true, KGL_HTTP_V2_PROTOCOL_ERROR);
			}
			frame_size = value;
			break;

		default:
			break;
		}

		pos += KGL_HTTP_V2_SETTINGS_PARAM_SIZE;
	}

	return state_complete( pos, end);
}

u_char *KHttp2::state_push_promise(u_char *pos, u_char *end)
{
	klog(KLOG_WARNING, "client sent unsupport push_promise frame\n");
	return state_skip(pos, end);
}
u_char *KHttp2::state_ping(u_char *pos, u_char *end)
{

	if (state.length != KGL_HTTP_V2_PING_SIZE) {
		klog(KLOG_WARNING, "client sent PING frame with incorrect length %u\n", state.length);
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	if (end - pos < KGL_HTTP_V2_PING_SIZE) {
		return state_save(pos, end, &KHttp2::state_ping);
	}
	if (state.flags & KGL_HTTP_V2_ACK_FLAG) {
		return state_skip(pos, end);
	}
	http2_buff *frame = get_frame(0, KGL_HTTP_V2_PING_SIZE,KGL_HTTP_V2_PING_FRAME,KGL_HTTP_V2_ACK_FLAG);
	if (frame == NULL) {
		klog(KLOG_ERR, "http2 get_frame is NULL\n");
		return this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
	}
	write_buffer.push(frame);
	startWrite();
	return state_complete(pos + KGL_HTTP_V2_PING_SIZE, end);
}
u_char *KHttp2::state_goaway(u_char *pos, u_char *end)
{	
	if (state.length < KGL_HTTP_V2_GOAWAY_SIZE) {
		klog(KLOG_WARNING,"client sent GOAWAY frame with incorrect length %u\n", state.length);
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}
	if (end - pos < KGL_HTTP_V2_GOAWAY_SIZE) {
		return state_save(pos, end, &KHttp2::state_goaway);
	}
	peer_goaway = 1;
	return state_skip(pos, end);
}
u_char *KHttp2::state_window_update(u_char *pos, u_char *end)
{
	size_t                 window;
	KHttp2Node    *node;
	KHttp2Context  *stream;

	if (state.length != KGL_HTTP_V2_WINDOW_UPDATE_SIZE) {
		klog(KLOG_WARNING, 
			"client sent WINDOW_UPDATE frame "
			"with incorrect length %u\n", state.length);
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	if (end - pos < KGL_HTTP_V2_WINDOW_UPDATE_SIZE) {
		return state_save(pos, end,
			&KHttp2::state_window_update);
	}

	window = kgl_http_v2_parse_window(pos);

	pos += KGL_HTTP_V2_WINDOW_UPDATE_SIZE;

	//printf("http2 WINDOW_UPDATE frame sid:%u window:%u\n",state.sid, window);

	if (state.sid) {
		node = this->get_node(state.sid, false);
		if (node == NULL || node->stream == NULL) {
			//klog(KLOG_WARNING,"unknown http2 stream id=[%d]\n",state.sid);
			return state_complete(pos, end);
		}
		stream = node->stream;

		if (window > (size_t)(KGL_HTTP_V2_MAX_WINDOW - stream->send_window)) {
			klog(KLOG_WARNING,
				"client violated flow control for stream %ui: "
				"received WINDOW_UPDATE frame "
				"with window increment %u "
				"not allowed for window %u\n",
				state.sid, window, stream->send_window);
			terminate_stream(stream, KGL_HTTP_V2_FLOW_CTRL_ERROR);			
			return state_complete(pos, end);
		}
		stream->send_window += window;
		kgl_http2_event *e = NULL;
		if (stream->write_wait && stream->write_wait->len<0 && stream->write_wait->buffer) {
			e = stream->write_wait;
			stream->write_wait = NULL;
		}
		if (e) {
			this->write(stream, e->result, e->buffer, e->arg);
			delete e;
		}
		//printf("stream [%d] window=[%d] send_window=[%d]\n", node->id, window, stream->send_window);
		return state_complete(pos, end);
	}

	if (window > KGL_HTTP_V2_MAX_WINDOW - send_window) {
		klog(KLOG_WARNING, 
			"client violated connection flow control: "
			"received WINDOW_UPDATE frame "
			"with window increment %u "
			"not allowed for window %u\n",
			window, send_window);

		return this->close(true, KGL_HTTP_V2_FLOW_CTRL_ERROR);
	}
	if (send_window <= 0) {
		send_window += window;
		check_write_wait();
	} else {
		send_window += window;
	}
	return state_complete(pos, end);
}
u_char *KHttp2::state_continuation(u_char *pos, u_char *end)
{
	klog(KLOG_ERR,"client sent unexpected CONTINUATION frame");
	return this->close(true, KGL_HTTP_V2_PROTOCOL_ERROR);
}
bool KHttp2::send_rst_stream(uint32_t sid, uint32_t status)
{
	http2_buff *buf = get_frame(sid, sizeof(http2_frame_rst_stream),KGL_HTTP_V2_RST_STREAM_FRAME,KGL_HTTP_V2_NO_FLAG);
	buf->tcp_nodelay = 1;
	http2_frame_rst_stream *b = (http2_frame_rst_stream *)(buf->data + sizeof(http2_frame_header));
	b->status = htonl(status);
	write_buffer.push(buf);
	startWrite();
	return true;
}
u_char *KHttp2::state_skip(u_char *pos, u_char *end)
{
	size_t  size;
	size = end - pos;
	if (size < state.length) {
		state.length -= size;
		return state_save(end, end, &KHttp2::state_skip);
	}
	return state_complete(pos + state.length, end);
}
u_char *KHttp2::state_complete(u_char *pos,u_char *end)
{
	if (pos > end) {
		klog(KLOG_WARNING, "receive buffer overrun\n");
		return this->close(true, KGL_HTTP_V2_INTERNAL_ERROR);
	}
	if (state.stream && state.stream->destroy_by_http2) {
#ifndef NDEBUG
		if (!client_model) {
			assert(state.stream->request == NULL);
		}
#endif
		delete state.stream;
	}
	state.stream = NULL;
	state.handler = &KHttp2::state_head;
	return pos;
}
u_char *KHttp2::skip_padded(u_char *pos,u_char *end)
{
	state.length += state.padding;
	state.padding = 0;
	return state_skip(pos, end);
}
u_char *KHttp2::state_skip_headers(u_char *pos, u_char *end)
{
	return state_header_block(pos, end);
}
u_char *KHttp2::state_field_len(u_char *pos,u_char *end)
{
	size_t                   alloc;
	intptr_t                len;
	uintptr_t               huff;

	if (!(state.flags & KGL_HTTP_V2_END_HEADERS_FLAG)
		&& state.length < KGL_HTTP_V2_INT_OCTETS)
	{
		return handle_continuation(pos, end, &KHttp2::state_field_len);
	}

	if (state.length < 1) {
		klog(KLOG_WARNING, "client sent header block with incorrect length\n");
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	if (end - pos < 1) {
		return state_save( pos, end,&KHttp2::state_field_len);
	}

	huff = *pos >> 7;
	len = parse_int(&pos, end, kgl_http_v2_prefix(7));

	if (len < 0) {
		if (len == KGL_AGAIN) {
			return state_save(pos, end,&KHttp2::state_field_len);
		}

		if (len == KGL_DECLINED) {
			klog(KLOG_WARNING, "client sent header field with too long length value\n");
			return this->close(true, KGL_HTTP_V2_COMP_ERROR);
		}

		klog(KLOG_WARNING, "client sent header block with incorrect length\n");
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}

	klog(KLOG_DEBUG, "http2 hpack %s string length: %i\n",	huff ? "encoded" : "raw", len);
	state.field_rest = len;
	if ((size_t)len > 4096) {
		klog(KLOG_WARNING,"client exceeded http2_max_field_size limit len=[%d]\n",len);
		return state_field_skip(pos, end);
	}	
	if (state.stream == NULL && !state.index) {
		return state_field_skip(pos, end);
	}

	alloc = (huff ? len * 8 / 5 : len) + 1;

	state.field_start = (u_char *)kgl_pnalloc(state.pool, alloc);
	if (state.field_start == NULL) {
		return this->close(true,KGL_HTTP_V2_INTERNAL_ERROR);
	}

	state.field_end = state.field_start;

	if (huff) {
		return state_field_huff(pos, end);
	}

	return state_field_raw(pos, end);
}
u_char *KHttp2::state_field_huff(u_char *pos, u_char *end)
{
	size_t  size;
	size = end - pos;

	if (size > state.field_rest) {
		size = state.field_rest;
	}
	if (size > state.length) {
		size = state.length;
	}
	state.length -= size;
	state.field_rest -= size;
	if (!kgl_http_v2_huff_decode(&state.field_state, pos, size,
		&state.field_end,
		state.field_rest == 0))
	{
		klog(KLOG_WARNING, "client sent invalid encoded header field\n");
		return this->close(true, KGL_HTTP_V2_COMP_ERROR);
	}
	pos += size;
	if (state.field_rest == 0) {
		*state.field_end = '\0';
		return state_process_header(pos, end);
	}

	if (state.length) {
		return state_save(pos, end,&KHttp2::state_field_huff);
	}
	if (state.flags & KGL_HTTP_V2_END_HEADERS_FLAG) {
		klog(KLOG_WARNING, "client sent header field with incorrect length\n");
		return this->close(true,KGL_HTTP_V2_SIZE_ERROR);
	}
	return handle_continuation(pos, end,&KHttp2::state_field_huff);
}
u_char *KHttp2::state_field_raw(u_char *pos, u_char *end)
{
	size_t  size;

	size = end - pos;

	if (size > state.field_rest) {
		size = state.field_rest;
	}

	if (size > state.length) {
		size = state.length;
	}

	state.length -= size;
	state.field_rest -= size;

	state.field_end = (u_char *)kgl_cpymem(state.field_end, pos, size);

	pos += size;

	if (state.field_rest == 0) {
		*state.field_end = '\0';
		return state_process_header(pos, end);
	}

	if (state.length) {
		return state_save(pos, end,&KHttp2::state_field_raw);
	}

	if (state.flags & KGL_HTTP_V2_END_HEADERS_FLAG) {
		klog(KLOG_WARNING, "client sent header field with incorrect length\n");
		return this->close(true, KGL_HTTP_V2_SIZE_ERROR);
	}
	return handle_continuation(pos, end,&KHttp2::state_field_raw);
}
u_char *KHttp2::state_field_skip(u_char *pos,u_char *end)
{
	size_t  size;

	size = end - pos;

	if (size > state.field_rest) {
		size = state.field_rest;
	}

	if (size > state.length) {
		size = state.length;
	}

	state.length -= size;
	state.field_rest -= size;

	pos += size;

	if (state.field_rest == 0) {
		return state_process_header(pos, end);
	}

	if (state.length) {
		return state_save(pos, end,&KHttp2::state_field_skip);
	}

	if (state.flags & KGL_HTTP_V2_END_HEADERS_FLAG) {
		klog(KLOG_WARNING, "client sent header field with incorrect length\n");
		return this->close(true,KGL_HTTP_V2_SIZE_ERROR);
	}

	return handle_continuation(pos, end,&KHttp2::state_field_skip);
}
bool test_http2()
{
	KHttp2WriteBuffer wb;
	http2_buff *buf = get_frame(0, 8, 1, 0);
	buf->tcp_nodelay = 1;
	wb.push(buf);
	buf = get_frame(0, 8, 1, 0);
	buf->tcp_nodelay = 1;
	wb.push(buf);
	KClientSocket socket;
	WSABUF buffer[8];
	int buffer_count;
	wb.getReadBuffer(&socket, buffer, buffer_count);
	http2_buff *buf2 = wb.readSuccess(&socket,buf->used-1);
#ifdef ENABLE_HTTP2_TCP_CORK
	assert(socket.delay);
#endif
	assert(buf2 ==NULL);
	wb.getReadBuffer(&socket, buffer, buffer_count);
	buf2 = wb.readSuccess(&socket, buf->used+1);
#ifdef ENABLE_HTTP2_TCP_CORK
	assert(!socket.delay);
#endif
	assert(buf2 && buf2->next);
	KHttp2WriteBuffer::remove_buff(buf2);
	return true;
}
#endif
