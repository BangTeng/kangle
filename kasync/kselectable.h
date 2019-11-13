#ifndef MSOCKET_SELECTABLE_H
#define MSOCKET_SELECTABLE_H
#include "kfeature.h"
#include "kforwin32.h"
#include "kselector.h"
#include "klist.h"
#include "ksocket.h"
#include "kgl_ssl.h"
#define STF_READ        1
#define STF_WRITE       (1<<1)
#define STF_RDHUP       (1<<2)


#define STF_REV         (1<<3)
#define STF_WEV         (1<<4)
#ifndef _WIN32
#define STF_ET          (1<<5)
#endif
#define STF_ERR         (1<<6)

#define STF_RREADY      (1<<7)
#define STF_WREADY      (1<<8)

#define STF_RREADY2     (1<<9)
#define STF_WREADY2     (1<<10)

#define STF_RTIME_OUT   (1<<11)
#define STF_RECVFROM    (1<<12)

#define STF_REVENT      (STF_READ|STF_RECVFROM)
#define STF_WEVENT      (STF_WRITE|STF_RDHUP)
#define STF_EVENT       (STF_REVENT|STF_WEVENT)

#define STF_RLOCK       (STF_READ|STF_RECVFROM)
#define STF_WLOCK       STF_WRITE
#define STF_LOCK        (STF_RLOCK|STF_WLOCK)
#define STF_APP_HTTP2   (1<<15)
#define ST_ERR_TIME_OUT    -2

KBEGIN_DECLS
typedef struct {
	void *arg;
	result_callback result;
	buffer_callback buffer;
} kgl_app_event;

typedef struct
{
	void *arg;
	result_callback result;
	buffer_callback buffer;
#ifdef _WIN32
	WSAOVERLAPPED lp;
#endif
} kgl_event;

struct kselectable_s {
	SOCKET fd;
	uint16_t st_flags;
	uint8_t tmo_left;
	uint8_t tmo;
	kgl_list queue;
#ifdef RQ_LEAK_DEBUG
	kgl_list queue_edge;
#endif
#ifdef KSOCKET_SSL
	kssl_session *ssl;
#endif
	int64_t active_msec;
	void *app_data;
	kselector *selector;
	kgl_event e[2];
};
void selectable_clean(kselectable *st);
bool selectable_remove(kselectable *st);
INLINE void selectable_next(kselectable *st, result_callback result, void *arg,int got)
{
	kgl_selector_module.next(st->selector, result, arg, got);
}
void selectable_next_read(kselectable *st, result_callback result, void *arg);
void selectable_next_write(kselectable *st, result_callback result, void *arg);

kev_result selectable_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg);
kev_result selectable_write(kselectable *st, result_callback result, buffer_callback buffer,void *arg);
bool selectable_try_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg);
bool selectable_try_write(kselectable *st, result_callback result, buffer_callback buffer, void *arg);
bool selectable_readhup(kselectable *st, result_callback result, void *arg);
void selectable_remove_readhup(kselectable *st);

void selectable_add_sync(kselectable *st);
void selectable_remove_sync(kselectable *st);
int selectable_sync_read(kselectable *st, LPWSABUF buf, int bc);
int selectable_sync_write(kselectable *st, LPWSABUF buf, int bc);

void selectable_shutdown(kselectable *st);
INLINE void selectable_clear_flags(kselectable *st, uint16_t flags)
{
	CLR(st->st_flags, flags);
}
INLINE void selectable_bind(kselectable *st, kselector *selector)
{
	kgl_selector_module.bind(selector, st);
}
INLINE bool is_selectable(kselectable *st, uint16_t flags)
{
	return TEST(st->st_flags, flags) > 0;
}
INLINE bool selectable_is_locked(kselectable *st)
{
	return is_selectable(st, STF_LOCK);
}
void selectable_recvfrom_event(kselectable *st);
void selectable_read_event(kselectable *st);
void selectable_write_event(kselectable *st);
kev_result selectable_event_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg);
kev_result selectable_event_write(kselectable *st, result_callback result, buffer_callback buffer, void *arg);

#ifdef ENABLE_KSSL_BIO
void selectable_low_event_read(kselectable *st, result_callback result, buffer_callback buffer, void *arg);
void selectable_low_event_write(kselectable *st, result_callback result, buffer_callback buffer, void *arg);
#endif
KEND_DECLS
#endif
