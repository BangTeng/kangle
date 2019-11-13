#include "kfeature.h"
#include "kkqueue_selector.h"
#include "kepoll_selector.h"
#include "kselector.h"
#include "kserver.h"
#include "klog.h"
#include "kasync_file.h"

#ifdef BSD_OS
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#include <stdio.h>

#define MAXEVENT	256

#ifndef NETBSD
typedef void * kqueue_udata_t;
#else
typedef uintptr_t kqueue_udata_t;
#endif

typedef struct {
        int kdpfd;
        kepoll_notice_selectable notice_st;
#ifdef BSD_OS
	struct kevent notify;
#endif
} kqueue_selector;

static kev_result result_notice_event(void *arg,int got)
{
	kepoll_notice_selectable *ast = (kepoll_notice_selectable *)arg;
	assert(got==0);
	kmutex_lock(&ast->lock);
	kselector_notice *notice = ast->head;
	ast->head = NULL;
	kmutex_unlock(&ast->lock);
	while (notice) {
		kselector_notice *next = notice->next;
		notice->result(notice->arg,notice->got);
		xfree(notice);
		notice = next;
	}
	return kev_ok;
}
static bool kqueue_add_event(int kdpfd,kselectable *st,uint16_t ev)
{
	struct kevent changes[2];
	int ev_count = 0;
	if (TEST(ev,STF_REV)) {
		if (!TEST(st->st_flags,STF_REV)) {
			EV_SET(&changes[ev_count++], st->fd, EVFILT_READ, EV_ADD|EV_CLEAR, 0, 0, (kqueue_udata_t)st);	
			SET(st->st_flags,STF_REV|STF_ET|STF_WREADY);
		}
	}
	if (TEST(ev,STF_WEV)) {
		if (!TEST(st->st_flags,STF_WEV)) {
			EV_SET(&changes[ev_count++], st->fd, EVFILT_WRITE, EV_ADD|EV_CLEAR, 0, 0, (kqueue_udata_t)st);
			SET(st->st_flags,STF_WEV|STF_ET);
		}
	}
	assert(ev_count>0);
	if(kevent(kdpfd, changes, ev_count, NULL, 0, NULL)==-1){
		return false;
	}
	return true;
}
static void kqueue_selector_init(kselector *selector)
{
	kqueue_selector *ctx = (kqueue_selector *)xmalloc(sizeof(kqueue_selector));
	memset(ctx,0,sizeof(kqueue_selector));
	ctx->kdpfd = kqueue();
	kmutex_init(&ctx->notice_st.lock,NULL);
	EV_SET(&ctx->notify,0,EVFILT_USER,EV_ADD|EV_CLEAR,0,0,0);
        if(kevent(ctx->kdpfd, &ctx->notify, 1, NULL, 0, NULL)==-1){
                perror("kevent");
        }
        EV_SET(&ctx->notify,0,EVFILT_USER,0,NOTE_TRIGGER,0,&ctx->notice_st.st);
	ctx->notice_st.st.selector = selector;
	SET(ctx->notice_st.st.st_flags,STF_READ|STF_REV);
	ctx->notice_st.st.e[OP_READ].arg = &ctx->notice_st;
	ctx->notice_st.st.e[OP_READ].result = result_notice_event;
	ctx->notice_st.st.e[OP_READ].buffer = NULL;
	selector->ctx = ctx;
}
static void kqueue_selector_destroy(kselector *selector)
{
	kqueue_selector *ctx = (kqueue_selector *)selector->ctx;
	close(ctx->kdpfd);
	xfree(ctx);
}
static void kqueue_selector_next(kselector *selector, result_callback result, void *arg, int got)
{
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	kselector_notice *notice = (kselector_notice *)xmalloc(sizeof(kselector_notice));
	memset(notice,0,sizeof(kselector_notice));
	notice->arg = arg;
	notice->result = result;
	notice->got = got;
	kmutex_lock(&es->notice_st.lock);
	notice->next = es->notice_st.head;
	es->notice_st.head = notice;
	kmutex_unlock(&es->notice_st.lock);
	if(kevent(es->kdpfd, &es->notify, 1, NULL, 0, NULL)==-1){
		perror("notice error");
	}
}
static bool kqueue_selector_listen(kselector *selector, kserver_selectable *ss, result_callback result)
{
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	kselectable *st = &ss->st;
	struct kevent changes[2];
    int ev_count = 0;
    st->e[OP_READ].arg = ss;
    st->e[OP_READ].result = result;
    st->e[OP_READ].buffer = NULL;
	st->e[OP_WRITE].result = NULL;
    assert(TEST(st->st_flags,STF_READ)==0);
    EV_SET(&changes[ev_count++], st->fd, EVFILT_READ, EV_ADD, 0, 0, (kqueue_udata_t)st);
    SET(st->st_flags,STF_READ|STF_REV);
    if(kevent(es->kdpfd, changes, ev_count, NULL, 0, NULL)==-1){
            klog(KLOG_ERR,"cann't addSocket sockfd=%d for read\n",st->fd);
            return false;
    }
    return true;
}
static bool kqueue_selector_connect(kselector *selector, kselectable *st, result_callback result, void *arg)
{
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	assert(TEST(st->st_flags,STF_READ|STF_WRITE|STF_RDHUP|STF_REV|STF_WEV)==0);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = NULL;
	SET(st->st_flags,STF_WRITE);
	if (!TEST(st->st_flags,STF_WEV|STF_REV)) {
		if (!kqueue_add_event(es->kdpfd,st,STF_WEV|STF_REV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	kselector_add_list(selector,st,KGL_LIST_CONNECT);
	return true;
}
static bool kqueue_selector_read(kselector *selector, kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	//printf("st=[%p] read\n",st);
	assert(TEST(st->st_flags,STF_READ)==0);
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = buffer;
	SET(st->st_flags,STF_READ);
	CLR(st->st_flags,STF_RDHUP);
	if (TEST(st->st_flags,STF_RREADY)) {
		kselector_add_list(selector,st,KGL_LIST_READY);
		return true;
	}
	if (!TEST(st->st_flags,STF_REV)) {
		if (!kqueue_add_event(es->kdpfd,st,STF_REV)) {
			CLR(st->st_flags,STF_READ);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		kselector_add_list(selector,st,KGL_LIST_RW);
	}
	return true;
}
static bool kqueue_selector_write(kselector *selector, kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	assert(TEST(st->st_flags,STF_WRITE)==0);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	SET(st->st_flags,STF_WRITE);
	CLR(st->st_flags,STF_RDHUP);
	if (TEST(st->st_flags,STF_WREADY)) {
		kselector_add_list(selector,st,KGL_LIST_READY);
		return true;
	}
	if (!TEST(st->st_flags,STF_WEV)) {
		if (!kqueue_add_event(es->kdpfd,st,STF_REV|STF_WEV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		kselector_add_list(selector,st,KGL_LIST_RW);
	}
	return true;
}
static bool kqueue_selector_recvfrom(kselector *selector, kselectable *st, result_callback result, buffer_callback buffer, buffer_callback addr_buffer, void *arg)
{
	return false;
}
static void kqueue_selector_select(kselector *selector) 
{
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	struct kevent events[MAXEVENT]; 
	struct timespec tm;
	tm.tv_sec = SELECTOR_TMO_MSEC/1000;
	tm.tv_nsec = SELECTOR_TMO_MSEC * 1000 - tm.tv_sec * 1000000;
	int ret = 0;
	for (;;) {
#ifdef MALLOCDEBUG
		if (kselector_can_close(selector)) {
			return;
		}
#endif
		kselector_check_timeout(selector,ret);
		ret = kevent(es->kdpfd, NULL, 0, events, MAXEVENT, &tm);
		if (selector->utm) {
			kselector_update_time();	
		}
		for (int n = 0; n < ret; ++n) {
			kselectable *st = (kselectable *) events[n].udata;
#ifndef NDEBUG
			klog(KLOG_DEBUG,"select st=%p,st_flags=%d,events=%d at %p\n",st,st->st_flags,events[n].filter,pthread_self());
#endif
			switch (events[n].filter) {
			case EVFILT_WRITE:
				SET(st->st_flags,STF_WREADY);
				if (TEST(st->st_flags,STF_WRITE)) {
					kselector_add_list(selector,st,KGL_LIST_READY);
				}
				break;
			case EVFILT_READ:
			case EVFILT_USER:
				SET(st->st_flags,STF_RREADY);
				if (TEST(st->st_flags,STF_READ)) {
					kselector_add_list(selector,st,KGL_LIST_READY);
				}
				break;
			case EVFILT_AIO:
				if (TEST(st->st_flags,STF_READ)) {
					SET(st->st_flags,STF_RREADY);
				} else if (TEST(st->st_flags,STF_WRITE)){
					SET(st->st_flags,STF_WREADY);
				}
				kselector_add_list(selector,st,KGL_LIST_READY);
				break;
			default:
				kassert(false);
			}
		}
	}
}
kasync_file *kqueue_selector_aio_open(kselector *selector, FILE_HANDLE fd)
{
#ifdef DARWIN
	//darwin os not support
	return NULL;
#else
	kasync_file *aio_file = xmemory_new(kasync_file);
	memset(aio_file,0,sizeof(kasync_file));
	aio_file->st.fd = fd;
	aio_file->st.selector = selector;
	return aio_file;
#endif
}
bool kqueue_selector_aio_write(kselector *selector, kasync_file *file, char *buf, int64_t offset, int length, aio_callback cb, void *arg)
{
#ifndef DARWIN
	katom_inc((void *)&kgl_aio_count);
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	file->st.e[OP_WRITE].result = result_async_file_event;
	file->st.e[OP_WRITE].arg = file;
	file->st.e[OP_WRITE].buffer = NULL;

	file->buf = buf;
	file->arg = arg;
	file->cb = cb;

 	SET(file->st.st_flags, STF_WRITE|STF_ET);
	memset(&file->iocb, 0,sizeof(struct aiocb));
	file->iocb.aio_fildes = file->st.fd;
	file->iocb.aio_offset = offset;
	file->iocb.aio_buf = buf;
	file->iocb.aio_nbytes = length;

	file->iocb.aio_sigevent.sigev_notify_kqueue = es->kdpfd;
	file->iocb.aio_sigevent.sigev_notify = SIGEV_KEVENT;
	file->iocb.aio_sigevent.sigev_value.sigval_ptr = file;

	int n = aio_write(&file->iocb);
	if (n!=-1) {
			return true;
	}
	katom_dec((void *)&kgl_aio_count);
#endif
	return false;
}
bool kqueue_selector_aio_read(kselector *selector, kasync_file *file, char *buf, int64_t offset, int length, aio_callback cb, void *arg)
{
#ifndef DARWIN
	katom_inc((void *)&kgl_aio_count);
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
	file->st.e[OP_READ].result = result_async_file_event;
	file->st.e[OP_READ].arg = file;
	file->st.e[OP_READ].buffer = NULL;

	file->buf = buf;
	file->arg = arg;
	file->cb = cb;

	SET(file->st.st_flags, STF_READ|STF_ET);
	memset(&file->iocb, 0,sizeof(struct aiocb));
	file->iocb.aio_fildes = file->st.fd;
	file->iocb.aio_offset = offset;
	file->iocb.aio_buf = buf;
	file->iocb.aio_nbytes = length;

	file->iocb.aio_sigevent.sigev_notify_kqueue = es->kdpfd;
	file->iocb.aio_sigevent.sigev_notify = SIGEV_KEVENT;
	file->iocb.aio_sigevent.sigev_value.sigval_ptr = file;

	int n = aio_read(&file->iocb);
	if (n!=-1) {
		return true;
	}
	katom_dec((void *)&kgl_aio_count);
#endif
	return false;
}
static void kqueue_selector_remove(kselector *selector, kselectable *st)
{
	if (!TEST(st->st_flags,STF_REV|STF_WEV)) {
		//socket not set event
		return;
	}
	kqueue_selector *es = (kqueue_selector *)selector->ctx;
#ifndef NDEBUG
	klog(KLOG_DEBUG,"remove socket st=%p\n",st);
#endif
	struct kevent changes[2];
	int ev_count = 0;
	if (TEST(st->st_flags,STF_REV)) {
		EV_SET(&changes[ev_count++], st->fd, EVFILT_READ, EV_DELETE, 0, 0, NULL); 
	}
	if (TEST(st->st_flags,STF_WEV)) {
		EV_SET(&changes[ev_count++], st->fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL); 
	}
	kevent(es->kdpfd, changes, ev_count, NULL, 0, NULL);
	CLR(st->st_flags,STF_REV|STF_WEV|STF_ET|STF_RREADY|STF_WREADY);
}
static kselector_module kqueue_selector_module = {
	"kqueue",
	kqueue_selector_init,
	kqueue_selector_destroy,
	kselector_default_bind,
	kqueue_selector_listen,
	kqueue_selector_connect,
	kqueue_selector_remove,
	kqueue_selector_read,
	kqueue_selector_write,
	kselector_default_readhup,
	kselector_default_remove_readhup,
	kqueue_selector_recvfrom,
	kqueue_selector_select,
	kqueue_selector_next,
	kqueue_selector_aio_open,
	kqueue_selector_aio_write,
	kqueue_selector_aio_read
};
void kkqueue_module_init()
{
	kgl_selector_module = kqueue_selector_module;
}
#endif
