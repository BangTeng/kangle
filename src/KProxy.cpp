#include "KHttpRequest.h"
#include "KUpstreamSelectable.h"
#include "KSelectorManager.h"
#ifdef ENABLE_PROXY_PROTOCOL
#pragma pack(push,1)
struct kgl_proxy_hdr_v2 {
	uint8_t sig[12];  /* hex 0D 0A 0D 0A 00 0D 0A 51 55 49 54 0A */
	uint8_t ver_cmd;  /* protocol version and command */
	uint8_t fam;      /* protocol family and address */
	uint16_t len;     /* number of following bytes part of the header */
};
struct kgl_proxy_ipv4_addr  {        /* for TCP/UDP over IPv4, len = 12 */
	uint32_t src_addr;
	uint32_t dst_addr;
	uint16_t src_port;
	uint16_t dst_port;
} ;
struct kgl_proxy_ipv6_addr  {        /* for TCP/UDP over IPv6, len = 36 */
	uint8_t  src_addr[16];
	uint8_t  dst_addr[16];
	uint16_t src_port;
	uint16_t dst_port;
} ;
#pragma pack(pop)
const char kgl_proxy_v2sig[] = "\x0D\x0A\x0D\x0A\x00\x0D\x0A\x51\x55\x49\x54\x0A";
class KRequestProxy;
typedef void (KRequestProxy::*proxy_state_handler_pt) ();
class KRequestProxy {
public:
	void destroy();
	void state(int got);
	void state_header();
	void state_body();
	KHttpRequest *rq;	
	proxy_state_handler_pt state_handle;
	kgl_proxy_hdr_v2 hdr;
	union {
		kgl_proxy_ipv4_addr v4_addr;
		kgl_proxy_ipv6_addr v6_addr;
	};
	char *hot;
	int left;
	bool ssl;
};
void buffer_proxy_read(void *arg, LPWSABUF buf, int &bufCount)
{
	KRequestProxy *proxy = (KRequestProxy *)arg;
	buf->iov_len = proxy->left;
	buf->iov_base = proxy->hot;
	bufCount = 1;
}
void result_proxy_reader(void *arg, int got)
{
	KRequestProxy *proxy = (KRequestProxy *)arg;
	proxy->state(got);
}
void KRequestProxy::destroy()
{
	if (ssl) {
		rq->c->set_flag(STF_SSL);
	}
	delete rq;
	delete this;
}
void KRequestProxy::state_body()
{
	rq->c->proxy_protocol_ip = (char *)rq->alloc_connect_memory(MAXIPLEN);
	ip_addr addr;
	memset(&addr, 0, sizeof(addr));
	if (hdr.fam == 1) {
		addr.sin_family = PF_INET;
		addr.addr32[0] = v4_addr.src_addr;
	} else if (hdr.fam == 2) {
		addr.sin_family = PF_INET6;
		memcpy(addr.addr8, v6_addr.src_addr, sizeof(v6_addr.src_addr));		
	} else {
		//not support protocol
		destroy();
		return;
	}
	if (!KSocket::make_ip(&addr, rq->c->proxy_protocol_ip, MAXIPLEN)) {
		destroy();
		return;
	}
	if (ssl) {
		rq->c->set_flag(STF_SSL);
	}
	handle_start_request(rq, ssl);
	delete this;
}
void KRequestProxy::state_header()
{
	if (memcmp(hdr.sig, kgl_proxy_v2sig, sizeof(hdr.sig)) != 0) {
		//sign error
		destroy();
		return;
	}
	hdr.len = ntohs(hdr.len);
	left = hdr.len;
	hot = (char *)&v4_addr;
	if (hdr.ver_cmd >> 4 != 2) {
		//version error
		destroy();
		return;
	}
	if ((hdr.ver_cmd & 0xF) != 1) {
		//only support PROXY command
		destroy();
		return;
	}
	state_handle = &KRequestProxy::state_body;
	rq->c->async_read(this, result_proxy_reader, buffer_proxy_read);
}
void KRequestProxy::state(int got)
{
	if (got <= 0) {
		destroy();
		return;
	}
	hot += got;
	left -= got;
	if (left > 0) {
		rq->c->async_read(this, result_proxy_reader, buffer_proxy_read);
		return;
	}
	(this->*state_handle)();
}

void handl_proxy_request(KHttpRequest *rq)
{
	KRequestProxy *proxy = new KRequestProxy;
	memset(proxy, 0, sizeof(KRequestProxy));
	proxy->hot = (char *)&proxy->hdr;
	proxy->left = sizeof(kgl_proxy_hdr_v2);
	proxy->ssl = rq->c->isSSL();
	rq->c->clear_flag(STF_SSL);
	proxy->state_handle = &KRequestProxy::state_header;
	proxy->rq = rq;
	rq->c->async_read(proxy, result_proxy_reader, buffer_proxy_read);
}
bool build_proxy_header(KSocketBuffer *buffer, KHttpRequest *rq)
{
	const char *ip = rq->getClientIp();
	ip_addr ia;
	kgl_proxy_hdr_v2 *hdr;
	if (!KSocket::getaddr(ip, &ia)) {
		return false;
	}
	int len;
	buff *buf = NULL;
	if (ia.sin_family == PF_INET) {
		len = sizeof(kgl_proxy_hdr_v2) + sizeof(kgl_proxy_ipv4_addr);
		buf = new_buff(len);
		hdr = (kgl_proxy_hdr_v2 *)buf->data;
		memset(hdr, 0, len);
		hdr->fam = 1;
		kgl_proxy_ipv4_addr *addr = (kgl_proxy_ipv4_addr *)(hdr + 1);
		addr->src_addr = ia.addr32[0];
	} else if (ia.sin_family == PF_INET6) {
		len = sizeof(kgl_proxy_hdr_v2) + sizeof(kgl_proxy_ipv6_addr);
		buf = new_buff(len);
		hdr = (kgl_proxy_hdr_v2 *)buf->data;
		memset(hdr, 0, len);
		hdr->fam = 2;
		kgl_proxy_ipv6_addr *addr = (kgl_proxy_ipv6_addr *)(hdr + 1);
		memcpy(addr->src_addr, ia.addr8, sizeof(addr->src_addr));
	} else {
		//not support protocol
		return false;
	}
	memcpy(hdr->sig, kgl_proxy_v2sig, sizeof(hdr->sig));
	hdr->ver_cmd = (2<<4)|1;
	hdr->len = htons((uint16_t)(len-sizeof(kgl_proxy_hdr_v2)));
	buffer->appendBuffer(buf);
	return true;
}
#endif
