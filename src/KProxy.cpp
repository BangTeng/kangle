#include "KHttpRequest.h"
#include "KUpstream.h"
#include "kselector_manager.h"
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
typedef kev_result(KRequestProxy::*proxy_state_handler_pt) ();
class KRequestProxy {
public:
	kev_result destroy();
	kev_result state(int got);
	kev_result state_header();
	kev_result state_body();
	proxy_state_handler_pt state_handle;
	kgl_proxy_hdr_v2 hdr;
	union {
		kgl_proxy_ipv4_addr v4_addr;
		kgl_proxy_ipv6_addr v6_addr;
	};
	char *hot;
	int left;
	kconnection *cn;
	result_callback cb;
};
int buffer_proxy_read(void *arg, LPWSABUF buf, int bc)
{
	KRequestProxy *proxy = (KRequestProxy *)arg;
	buf->iov_len = proxy->left;
	buf->iov_base = proxy->hot;
	return 1;
}
kev_result result_proxy_reader(void *arg, int got)
{
	KRequestProxy *proxy = (KRequestProxy *)arg;
	return proxy->state(got);
}
kev_result KRequestProxy::destroy()
{
	kev_result ret = cb(cn, -1);
	delete this;
	return ret;
}
kev_result KRequestProxy::state_body()
{
	cn->proxy_ip = (char *)kgl_pnalloc(cn->pool,MAXIPLEN);
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
		return destroy();
	}
	if (!ksocket_ipaddr_ip(&addr, cn->proxy_ip, MAXIPLEN)) {
		return destroy();
	}
	kev_result ret = cb(cn, 0);
	delete this;
	return ret;
}
kev_result KRequestProxy::state_header()
{
	if (memcmp(hdr.sig, kgl_proxy_v2sig, sizeof(hdr.sig)) != 0) {
		//sign error
		return destroy();
	}
	hdr.len = ntohs(hdr.len);
	left = hdr.len;
	hot = (char *)&v4_addr;
	if (hdr.ver_cmd >> 4 != 2) {
		//version error
		return destroy();
	}
	if ((hdr.ver_cmd & 0xF) != 1) {
		//only support PROXY command
		return destroy();
	}
	state_handle = &KRequestProxy::state_body;
	return selectable_read(&cn->st, result_proxy_reader, buffer_proxy_read, this);
}
kev_result KRequestProxy::state(int got)
{
	if (got <= 0) {
		destroy();
		return kev_destroy;
	}
	hot += got;
	left -= got;
	if (left > 0) {
		return selectable_read(&cn->st, result_proxy_reader, buffer_proxy_read, this);
	}
	return (this->*state_handle)();
}

kev_result handl_proxy_request(kconnection *cn, result_callback cb)
{
	KRequestProxy *proxy = new KRequestProxy;
	memset(proxy, 0, sizeof(KRequestProxy));
	proxy->hot = (char *)&proxy->hdr;
	proxy->left = sizeof(kgl_proxy_hdr_v2);
	proxy->state_handle = &KRequestProxy::state_header;
	proxy->cn = cn;
	proxy->cb = cb;
	return selectable_read(&cn->st,result_proxy_reader, buffer_proxy_read,proxy);
}
bool build_proxy_header(KReadWriteBuffer *buffer, KHttpRequest *rq)
{
	const char *ip = rq->getClientIp();
	ip_addr ia;
	kgl_proxy_hdr_v2 *hdr;
	if (!ksocket_get_ipaddr(ip, &ia)) {
		return false;
	}
	int len;
	kbuf *buf = NULL;
	if (ia.sin_family == PF_INET) {
		len = sizeof(kgl_proxy_hdr_v2) + sizeof(kgl_proxy_ipv4_addr);
		buf = new_kbuf(len);
		hdr = (kgl_proxy_hdr_v2 *)buf->data;
		memset(hdr, 0, len);
		hdr->fam = 1;
		kgl_proxy_ipv4_addr *addr = (kgl_proxy_ipv4_addr *)(hdr + 1);
		addr->src_addr = ia.addr32[0];
	} else if (ia.sin_family == PF_INET6) {
		len = sizeof(kgl_proxy_hdr_v2) + sizeof(kgl_proxy_ipv6_addr);
		buf = new_kbuf(len);
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
	buffer->Append(buf);
	return true;
}
#endif
