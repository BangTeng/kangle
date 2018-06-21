#ifndef KGL_HTTP2_H
#define KGL_HTTP2_H
#include "global.h"
#include "KReadWriteBuffer.h"
#include "KSSLSocket.h"
#include "KBuffer.h"
#include "forwin32.h"
#include "KList.h"
#include "KSendBuffer.h"
#include "KCondWait.h"
#include "KResponseContext.h"
#include "KHttp2WriteBuffer.h"
#include "KHttpHeader.h"
#include "KMutex.h"
#include "KMemPool.h"
#include "md5.h"
#ifdef ENABLE_HTTP2
#define  KGL_SUCCESS     0
#define  KGL_ERROR      -1
#define  KGL_AGAIN      -2
#define  KGL_BUSY       -3
#define  KGL_DONE       -4
#define  KGL_DECLINED   -5
#define  KGL_ABORT      -6
class KConnectionSelectable;
class KHttpRequest;
#ifdef TLSEXT_TYPE_next_proto_neg
#define KGL_HTTP_V2_NPN_NEGOTIATED       "h2"
#define KGL_HTTP_V2_NPN_ADVERTISE        "\x02h2"
#endif
#define KGL_HTTP_V2_STATE_BUFFER_SIZE    16

#define KGL_HTTP_V2_MAX_FRAME_SIZE       ((1 << 24) - 1)

#define KGL_HTTP_V2_INT_OCTETS           4
#define KGL_HTTP_V2_MAX_FIELD                                                 \
    (127 + (1 << (KGL_HTTP_V2_INT_OCTETS - 1) * 7) - 1)

#define KGL_HTTP_V2_DATA_DISCARD         1
#define KGL_HTTP_V2_DATA_ERROR           2
#define KGL_HTTP_V2_DATA_INTERNAL_ERROR  3

#define KGL_HTTP_V2_FRAME_HEADER_SIZE    9

/* frame types */
#define KGL_HTTP_V2_DATA_FRAME           0x0
#define KGL_HTTP_V2_HEADERS_FRAME        0x1
#define KGL_HTTP_V2_PRIORITY_FRAME       0x2
#define KGL_HTTP_V2_RST_STREAM_FRAME     0x3
#define KGL_HTTP_V2_SETTINGS_FRAME       0x4
#define KGL_HTTP_V2_PUSH_PROMISE_FRAME   0x5
#define KGL_HTTP_V2_PING_FRAME           0x6
#define KGL_HTTP_V2_GOAWAY_FRAME         0x7
#define KGL_HTTP_V2_WINDOW_UPDATE_FRAME  0x8
#define KGL_HTTP_V2_CONTINUATION_FRAME   0x9

/* frame flags */
#define KGL_HTTP_V2_NO_FLAG              0x00
#define KGL_HTTP_V2_ACK_FLAG             0x01
#define KGL_HTTP_V2_END_STREAM_FLAG      0x01
#define KGL_HTTP_V2_END_HEADERS_FLAG     0x04
#define KGL_HTTP_V2_PADDED_FLAG          0x08
#define KGL_HTTP_V2_PRIORITY_FLAG        0x20

#define KGL_HTTP_V2_DEFAULT_MAX_STREAM   64
#define kgl_http_v2_parse_uint16(p)  ntohs(*(uint16_t *) (p))
#define kgl_http_v2_parse_uint32(p)  ntohl(*(uint32_t *) (p))
#define kgl_http_v2_prefix(bits)  ((1 << (bits)) - 1)
#define KGL_HTTP_V2_STREAM_INDEX_MASK 0xF
#define kgl_http_v2_index_size() (KGL_HTTP_V2_STREAM_INDEX_MASK+1)
#define kgl_http_v2_index(sid)  ((sid >> 1) & KGL_HTTP_V2_STREAM_INDEX_MASK)

#define kgl_http_v2_parse_length(p)  ((p) >> 8)
#define kgl_http_v2_parse_type(p)    ((p) & 0xff)
#define kgl_http_v2_parse_sid(p)     (kgl_http_v2_parse_uint32(p) & 0x7fffffff)
#define kgl_http_v2_parse_window(p)  (kgl_http_v2_parse_uint32(p) & 0x7fffffff)
bool kgl_http_v2_huff_decode(u_char *state, u_char *src, size_t len, u_char **dst, uintptr_t last);

//about h2 write
#define kgl_http_v2_indexed(i)      (128 + (i))
#define kgl_http_v2_inc_indexed(i)  (64 + (i))
#define KGL_HTTP_V2_ENCODE_RAW            0
#define KGL_HTTP_V2_ENCODE_HUFF           0x80

#define KGL_HTTP_V2_STATUS_INDEX          8
#define KGL_HTTP_V2_STATUS_200_INDEX      8
#define KGL_HTTP_V2_STATUS_204_INDEX      9
#define KGL_HTTP_V2_STATUS_206_INDEX      10
#define KGL_HTTP_V2_STATUS_304_INDEX      11
#define KGL_HTTP_V2_STATUS_400_INDEX      12
#define KGL_HTTP_V2_STATUS_404_INDEX      13
#define KGL_HTTP_V2_STATUS_500_INDEX      14

#define KGL_HTTP_V2_CONTENT_LENGTH_INDEX  28
#define KGL_HTTP_V2_CONTENT_TYPE_INDEX    31
#define KGL_HTTP_V2_DATE_INDEX            33
#define KGL_HTTP_V2_LAST_MODIFIED_INDEX   44
#define KGL_HTTP_V2_LOCATION_INDEX        46
#define KGL_HTTP_V2_SERVER_INDEX          54
#define KGL_HTTP_V2_VARY_INDEX            59


/* errors */
#define KGL_HTTP_V2_NO_ERROR                     0x0
#define KGL_HTTP_V2_PROTOCOL_ERROR               0x1
#define KGL_HTTP_V2_INTERNAL_ERROR               0x2
#define KGL_HTTP_V2_FLOW_CTRL_ERROR              0x3
#define KGL_HTTP_V2_SETTINGS_TIMEOUT             0x4
#define KGL_HTTP_V2_STREAM_CLOSED                0x5
#define KGL_HTTP_V2_SIZE_ERROR                   0x6
#define KGL_HTTP_V2_REFUSED_STREAM               0x7
#define KGL_HTTP_V2_CANCEL                       0x8
#define KGL_HTTP_V2_COMP_ERROR                   0x9
#define KGL_HTTP_V2_CONNECT_ERROR                0xa
#define KGL_HTTP_V2_ENHANCE_YOUR_CALM            0xb
#define KGL_HTTP_V2_INADEQUATE_SECURITY          0xc
#define KGL_HTTP_V2_HTTP_1_1_REQUIRED            0xd

/* frame sizes */
#define KGL_HTTP_V2_RST_STREAM_SIZE              4
#define KGL_HTTP_V2_PRIORITY_SIZE                5
#define KGL_HTTP_V2_PING_SIZE                    8
#define KGL_HTTP_V2_GOAWAY_SIZE                  8
#define KGL_HTTP_V2_WINDOW_UPDATE_SIZE           4

#define KGL_HTTP_V2_STREAM_ID_SIZE               4

#define KGL_HTTP_V2_SETTINGS_PARAM_SIZE          6

/* settings fields */
#define KGL_HTTP_V2_HEADER_TABLE_SIZE_SETTING    0x1
#define KGL_HTTP_V2_MAX_STREAMS_SETTING          0x3
#define KGL_HTTP_V2_INIT_WINDOW_SIZE_SETTING     0x4
#define KGL_HTTP_V2_MAX_FRAME_SIZE_SETTING       0x5

#define KGL_HTTP_V2_FRAME_BUFFER_SIZE            24

#define KGL_HTTP_V2_DEFAULT_FRAME_SIZE           (1 << 14)

#define KGL_HTTP_V2_MAX_WINDOW                   ((1U << 31) - 1)
#define KGL_HTTP_V2_DEFAULT_WINDOW               65535

#define KGL_HTTP_V2_STREAM_RECV_WINDOW          262143
#define KGL_HTTP_V2_CONNECTION_RECV_WINDOW      KGL_HTTP_V2_MAX_WINDOW
#define KGL_HTTP_V2_ROOT                         (KHttp2Node *) -1
#pragma pack(push,1)
struct http2_frame_header
{
	uint32_t length_type;
	uint8_t flags;
	uint32_t stream_id;
	int get_type()
	{
		return (length_type & 0xff);
	}
	int get_length()
	{
		return (length_type >> 8);
	}
	void set_length_type(int length,int type) {
		length_type = (length << 8) | type;
		assert(get_type() == type);
		assert(get_length() == length);
		length_type = htonl(length_type);
	}
};
struct http2_frame_setting {
	uint16_t id;
	uint32_t value;
};
struct http2_frame_window_update {
	uint32_t inc_size;
};
struct http2_frame_rst_stream {
	uint32_t status;
};
struct http2_frame_ping {
	uint64_t opaque;
};
struct http2_frame_goaway {
	uint32_t last_stream_id;
	uint32_t error_code;
};
#pragma pack(pop)
class KHttp2Context;
class KHttp2;
bool test_http2();
typedef int (*http2_header_parser_pt)(KHttp2Context *ctx, kgl_str_t *name, kgl_str_t *value);
typedef int (*http2_accept_handler_pt)(KHttp2Context *ctx);
typedef u_char *(KHttp2::*kgl_http_v2_handler_pt) (u_char *pos, u_char *end);
typedef struct {
	uintptr_t        hash;
	kgl_str_t         key;
	kgl_str_t         value;
	u_char           *lowcase_key;
} kgl_table_elt_t;

class kgl_sync_result
{
public:
	KHttp2 *http2;
	KHttp2Context *ctx;
	KCondWait cond;
	union {
		LPWSABUF buf;
		INT64 send_header_body_len;
	};
	int bufCount;
	int got;
};
typedef struct {
	kgl_str_t                        name;
	kgl_str_t                        value;
} kgl_http_v2_header_t;

struct kgl_http_v2_state_t {
	uint32_t					     sid;
	size_t                           length;
	size_t                           padding;
	unsigned                         flags : 8;
	unsigned                         incomplete : 1;
	/* HPACK */
	unsigned                         parse_name : 1;
	unsigned                         parse_value : 1;
	unsigned                         index : 1;
	unsigned						keep_pool : 1;
	kgl_pool_t						 *pool;
	kgl_http_v2_header_t             header;
	size_t                           header_limit;
	u_char                           field_state;
	u_char                          *field_start;
	u_char                          *field_end;
	size_t                           field_rest;
	KHttp2Context					*stream;
	u_char                           buffer[8192];
	size_t                           buffer_used;
	kgl_http_v2_handler_pt           handler;
};

typedef struct {
	kgl_http_v2_header_t           **entries;
	uintptr_t                       added;
	uintptr_t                       deleted;
	uintptr_t                       reused;
	uintptr_t                       allocated;

	size_t                           size;
	size_t                           free;
	u_char                          *storage;
	u_char                          *pos;
} kgl_http_v2_hpack_t;


class KHttp2Node {
public:
	KHttp2Node()
	{
		memset(this, 0, sizeof(KHttp2Node));
	}
	uint32_t                  id;
	KHttp2Node				 *index;
	uint8_t                   rank;
	uint8_t                   weight;
	double                    rel_weight;
	KHttp2Context            *stream;

};
class KUpstreamSelectable;
class KHttp2Context
{
public:
	KHttp2Node *node;
	union {
		KHttpRequest *request;
		
	};
	kgl_array_t  *cookies;
#ifndef NDEBUG
	INT64 orig_content_length;
	//KMD5_CTX md5;
#endif
	INT64 content_left;
	friend class KHttp2;
	unsigned  in_closed : 1;
	unsigned  out_closed:1;
	unsigned  rst : 1;
	unsigned  parsed_header : 1;
	
	unsigned  destroy_by_http2:1;
	unsigned  skip_data:1;
	unsigned  know_content_length:1;
	int send_window;
	size_t recv_window;
	KHttp2HeaderFrame *send_header;
	kgl_http2_event *write_wait;
	kgl_http2_event *read_wait;
	KSendBuffer *read_buffer;
	http2_buff *build_write_buffer(kgl_http2_event *e, int &len)
	{
		http2_buff *buf_out = NULL;
		http2_buff *last = NULL;
		WSABUF vc[4];
		int bufferCount = 4;
		int total_len = 0;
		e->buffer(e->arg, vc, bufferCount);
		for (int i = 0; i<bufferCount; i++) {
			if (len <= 0) {
				break;
			}
			http2_buff *new_buf = new http2_buff;
			new_buf->skip_data_free = 1;
			new_buf->data = (char *)vc[i].iov_base;
			new_buf->used = (uint16_t)MIN(len, (int)vc[i].iov_len);
#ifndef NDEBUG
		//	KMD5Update(&md5, (unsigned char *)new_buf->data, new_buf->used);
#endif
			//fwrite(new_buf->data, 1, new_buf->used, stdout);
			len -= new_buf->used;
			total_len += new_buf->used;
			if (last) {
				last->next = new_buf;
			} else {
				buf_out = new_buf;
			}
			last = new_buf;
		}
		assert(last);
		assert(total_len>0);
		e->len = total_len;
		http2_buff *new_buf = new http2_buff;
		http2_frame_header *data = (http2_frame_header *)malloc(sizeof(http2_frame_header));
		if (last) {
			//notice to last
			last->ctx = this;
		} else {
			new_buf->ctx = this;
		}
		new_buf->data = (char *)data;
		new_buf->used = sizeof(http2_frame_header);
		memset(data, 0, sizeof(http2_frame_header));
		data->set_length_type(total_len, KGL_HTTP_V2_DATA_FRAME);
		data->stream_id = ntohl(node->id);
		if (know_content_length) {
			content_left -= total_len;
			if (content_left <= 0) {
				data->flags |= KGL_HTTP_V2_END_STREAM_FLAG;
				out_closed = 1;
				assert(content_left == 0);
				last->tcp_nodelay = 1;
			}
		}
		assert(buf_out);
		new_buf->next = buf_out;
		len = total_len;
		return new_buf;
	}
	http2_buff *build_write_buffer(resultEvent result, bufferEvent buffer, void *arg, int &len)
	{
		assert(write_wait == NULL);
		write_wait = new kgl_http2_event;
		write_wait->buffer = buffer;
		write_wait->arg = arg;
		write_wait->result = result;
		write_wait->len = -1;
		return build_write_buffer(write_wait, len);
	}
	void setContentLength (INT64 content_length) {
		if (content_length >= 0) {
			know_content_length = 1;
			this->content_left = content_length;
#ifndef NDEBUG
			this->orig_content_length = content_length;
#endif
		} else {
			know_content_length = 0;
		}
	}
private:
	KHttp2Context(int send_window)
	{
		memset(this,0,sizeof(KHttp2Context));
		recv_window = KGL_HTTP_V2_STREAM_RECV_WINDOW;
		this->send_window = send_window;
#ifndef NDEBUG
		//KMD5Init(&md5);
#endif
	}
	~KHttp2Context()
	{
		assert(read_wait==NULL);
		assert(write_wait == NULL||write_wait->buffer==NULL);
		if (write_wait) {
			delete write_wait;
		}
		if (read_buffer) {
			delete read_buffer;
		}
		if (send_header) {
			delete send_header;
		}
	}

};
class KHttp2
{
public:
	KHttp2();
public:
	void server(KConnectionSelectable *c);
	
public:
	void read_header(KHttp2Context *ctx, resultEvent result, void *arg);
	int read(KHttp2Context *ctx,char *buf,int len);
	int write(KHttp2Context *ctx,LPWSABUF buf,int bufCount);
	bool add_status(KHttp2Context *ctx,uint16_t status_code);
	bool add_method(KHttp2Context *ctx, u_char meth);
	bool add_header(KHttp2Context *ctx, know_http_header name, const char *val, hlen_t val_len);
	bool add_header(KHttp2Context *ctx,const char *name, hlen_t name_len, const char *val, hlen_t val_len);
	void read(KHttp2Context *ctx,resultEvent result,bufferEvent buffer,void *arg);
	void read_hup(KHttp2Context *ctx, resultEvent result, void *arg);
	void remove_read_hup(KHttp2Context *ctx);
	void shutdown(KHttp2Context *ctx);
	void write(KHttp2Context *ctx,resultEvent result,bufferEvent buffer,void *arg);
	void release(KHttp2Context *ctx);
	void release_stream(KHttp2Context *ctx);
	void release_admin(KHttp2Context *ctx);
	int sync_send_header(KHttp2Context *ctx,INT64 body_len);
	int send_header(KHttp2Context *ctx, INT64 body_len);
	void write_end(KHttp2Context *ctx);
public:
	void getReadBuffer(iovec *buf,int &bufCount);
	void getWriteBuffer(iovec *buf,int &bufCount);
	void resultRead(int got);
	void resultWrite(int got);
	friend class KConnectionSelectable;
	friend class KSelector;
	friend class http2_buff;
private:
	static kgl_http_v2_handler_pt kgl_http_v2_frame_states[];
	u_char *state_data(u_char *pos, u_char *end);
	u_char *state_headers(u_char *pos, u_char *end);
	u_char *state_priority(u_char *pos, u_char *end);
	u_char *state_rst_stream(u_char *pos, u_char *end);
	u_char *state_settings(u_char *pos, u_char *end);
	u_char *state_push_promise(u_char *pos, u_char *end);
	u_char *state_ping(u_char *pos, u_char *end);
	u_char *state_goaway(u_char *pos, u_char *end);
	u_char *state_window_update(u_char *pos, u_char *end);
	u_char *state_continuation(u_char *pos, u_char *end);
private:
	bool check_recv_window(KHttp2Context *ctx);
	bool add_header_cookie(KHttp2Context *ctx, const char *val, hlen_t val_len);
	KHttp2Node *get_node(uint32_t id, bool alloc);
	u_char *close(bool read, int status);
	void init(KConnectionSelectable *c);
	~KHttp2();
	bool can_destroy() {
		return processing == 0 && write_processing == 0 && read_processing == 0;
	}
	void destroy();
	void startRead();
	void startWrite();
	void ping();
	void goaway(int error_code);
	KHttp2Context *create_stream();
	int copyReadBuffer(KHttp2Context *ctx,bufferEvent buffer,void *arg);
	void setDependency(KHttp2Node *node, uint32_t depend, bool exclusive);
	intptr_t parse_int(u_char **pos, u_char *end, uintptr_t prefix);
	size_t                           send_window;
	size_t                           recv_window;
	size_t                           init_window;
	size_t                           frame_size;
	size_t							 max_stream;
	KHttp2WriteBuffer write_buffer;
	KHttp2Node **streams_index;
	kgl_http_v2_state_t state;
	kgl_http_v2_hpack_t hpack;
	INT64 last_stream_msec;
	uint32_t last_peer_sid;
	uint32_t last_self_sid;
	unsigned write_processing : 1;
	unsigned read_processing : 1;
	unsigned peer_goaway : 1;
	unsigned self_goaway : 1;
	unsigned pinged : 1;
	
	unsigned processing;
private:
	KConnectionSelectable *c;
	bool send_settings(bool ack);
	bool send_window_update(uint32_t sid, size_t window);
	u_char *state_settings_params(u_char *pos, u_char *end);
	u_char *state_preface(u_char *pos, u_char *end);
	u_char *state_preface_end(u_char *pos, u_char *end);
	u_char *state_head(u_char *pos, u_char *end);
	u_char *state_skip(u_char *pos, u_char *end);
	u_char *state_complete(u_char *pos, u_char *end);
	u_char *skip_padded(u_char *pos, u_char *end);
	u_char *state_skip_headers(u_char *pos, u_char *end);
	u_char *state_header_block(u_char *pos, u_char *end);
	u_char *state_field_len(u_char *pos, u_char *end);
	u_char *state_skip_padded(u_char *pos, u_char *end);
	u_char *state_header_complete(u_char *pos, u_char *end);
	u_char *state_save(u_char *pos, u_char *end, kgl_http_v2_handler_pt handler);
	u_char *state_process_header(u_char *pos, u_char *end);
	u_char *state_field_skip(u_char *pos, u_char *end);
	u_char *state_field_huff(u_char *pos, u_char *end);
	u_char *state_field_raw(u_char *pos, u_char *end);
	u_char *state_read_data(u_char *pos, u_char *end);
	u_char *handle_continuation(u_char *pos, u_char *end, kgl_http_v2_handler_pt handler);
	void destroy_node(KHttp2Node *node);
	void adjust_windows(size_t window);
	void check_write_wait();
	bool send_rst_stream(uint32_t sid, uint32_t status);
	bool terminate_stream(KHttp2Context *stream, uint32_t status);
private:
	bool get_indexed_header(uintptr_t index, bool name_only);
	bool table_size(size_t size);
	bool table_account(size_t size);
	bool add_header(kgl_http_v2_header_t *header);
	bool add_cookie(kgl_http_v2_header_t *header);
};
#endif
#endif

