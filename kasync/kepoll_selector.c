#include "kfeature.h"
#ifdef LINUX
#include <sys/epoll.h>
#include <errno.h>
#include <stdint.h>
#include <sys/eventfd.h>
#include <stdio.h>
#include <sys/syscall.h>
#include "kselectable.h"
#include "kserver.h"
#include "ksocket.h"
#include "klist.h"
#include "ksync.h"
#include "klog.h"
#include "klist.h"
#include "kasync_file.h"
#include "kmalloc.h"
#include "kepoll_selector.h"

#define MAXEVENT	512


typedef struct {
	kselectable st;
	aio_context_t aio_ctx;
} kepoll_aio_selectable;

typedef struct {
	int kdpfd;
	kepoll_notice_selectable notice_st;
	kepoll_aio_selectable aio_st;
} kepoll_selector;

int io_setup(u_int nr_reqs, aio_context_t *ctx)
{
    return syscall(SYS_io_setup, nr_reqs, ctx);
}
int io_destroy(aio_context_t ctx)
{
    return syscall(SYS_io_destroy, ctx);
}
int io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events, struct timespec *tmo)
{
    return syscall(SYS_io_getevents, ctx, min_nr, nr, events, tmo);
}
int io_submit(aio_context_t ctx, long n, struct iocb **paiocb)
{
    return syscall(SYS_io_submit, ctx, n, paiocb);
}
static kev_result result_notice_event(void *arg,int got)
{
	kepoll_notice_selectable *ast = (kepoll_notice_selectable *)arg;
	assert(got==0);
	uint64_t value;
	if (read(ast->st.fd, &value, sizeof(value)) != sizeof(value)) {
		perror("read");
		return kev_ok;
	}
	while (value>0) {
		kmutex_lock(&ast->lock);
		kselector_notice *notice = ast->head;
		assert(notice!=NULL);
		ast->head = notice->next;
		kmutex_unlock(&ast->lock);
		value --;
		notice->result(notice->arg,notice->got);
		xfree(notice);
	}
	return kev_ok;
}
void aio_result(kasync_file *file,struct iocb *iocb, long res, long res2)
{
        if (res<=0) {
        	async_file_event(file,NULL,res);
            return;
        }
        //printf("aio_result res=[%d] iocb=[%p],offset_adjust=[%d]\n",res,iocb,ctx->offset_adjust);
        int length = res - file->offset_adjust;
        char *buf = file->buf + file->offset_adjust;
        async_file_event(file,buf,MIN(length,file->length));
}

static kev_result result_aio_event(void *arg,int got)
{
	kepoll_aio_selectable *aio_st = (kepoll_aio_selectable *)arg;
	kassert(got==0);
	uint64_t finished_aio;
	int i,j,r;
	struct timespec tms;
	if (read(aio_st->st.fd, &finished_aio, sizeof(finished_aio)) != sizeof(finished_aio)) {
	   perror("read");
	   return kev_err;
	}
	struct io_event events[MAXEVENT];

	while (finished_aio > 0)  {
		   tms.tv_sec = 0;
		   tms.tv_nsec = 0;
		   int get_events = finished_aio;
		   if (get_events>MAXEVENT) {
				   get_events = MAXEVENT;
		   }
		   r = io_getevents(aio_st->aio_ctx, 1, get_events, events, &tms);
		   if (r > 0) {
				   for (j = 0; j < r; ++j) {
						   kasync_file *ctx = (kasync_file *)events[j].data;
						   aio_result(ctx,(struct iocb *)events[j].obj, events[j].res, events[j].res2);
				   }
				   i += r;
				   finished_aio -= r;
		   }
	}
	return kev_ok;

}
static bool epoll_add_event(int kdpfd,kselectable *st,uint16_t ev)
{
	struct epoll_event event;
	int op = EPOLL_CTL_ADD;
	uint32_t events = 0;
	uint16_t prev_ev = st->st_flags;
	if (TEST(ev,STF_REV)) {
		events |= EPOLLIN|EPOLLRDHUP|EPOLLET;
		if (TEST(prev_ev,STF_WEV)) {
			op = EPOLL_CTL_MOD;
			events|=EPOLLOUT;
		}
		SET(st->st_flags,STF_REV|STF_ET|STF_WREADY);
	}
	if (TEST(ev,STF_WEV)) {
		events |= EPOLLOUT|EPOLLRDHUP|EPOLLET;
		if (TEST(prev_ev,STF_REV)) {
			op = EPOLL_CTL_MOD;
			events|=EPOLLIN;
		}
		SET(st->st_flags,STF_WEV|STF_ET);
	}
	SOCKET sockfd = st->fd;
	event.events = events;
#ifndef NDEBUG
//	klog(KLOG_DEBUG,"%s event [%d] epoll event=[%lld] sockfd=[%d],st=[%p]\n",op==EPOLL_CTL_ADD?"add":"modify",ev,int64_t(events),sockfd,st);
#endif
	event.data.ptr = st;
	int ret = epoll_ctl(kdpfd, op, sockfd, &event);
	if (ret !=0) {
		klog(KLOG_ERR, "epoll ctl error fd=%d,errno=%d %s\n", sockfd,errno,strerror(errno));
		return false;
	}
	return true;
}
static void epoll_selector_init(kselector *selector)
{
	struct epoll_event epevent;
	epevent.events = EPOLLIN;

	kepoll_selector *ctx = (kepoll_selector *)xmalloc(sizeof(kepoll_selector));
	memset(ctx,0,sizeof(kepoll_selector));
#ifdef EPOLL_CLOEXEC
	ctx->kdpfd = epoll_create1(EPOLL_CLOEXEC);
#else
	ctx->kdpfd = epoll_create(MAXEVENT);
#endif
	//init notice_st
	kmutex_init(&ctx->notice_st.lock,NULL);
	ctx->notice_st.st.fd = eventfd(0, EFD_CLOEXEC);
	if (ctx->notice_st.st.fd == -1) {
		perror("eventfd");
	}
	ctx->notice_st.st.selector = selector;
	SET(ctx->notice_st.st.st_flags,STF_READ|STF_REV);
	ctx->notice_st.st.e[OP_READ].arg = &ctx->notice_st;
	ctx->notice_st.st.e[OP_READ].result = result_notice_event;
	ctx->notice_st.st.e[OP_READ].buffer = NULL;
	epevent.data.ptr = &ctx->notice_st.st;
	if (epoll_ctl(ctx->kdpfd, EPOLL_CTL_ADD, ctx->notice_st.st.fd, &epevent)) {
		perror("epoll_ctl");
	}

	//init aio_st
	ctx->aio_st.st.selector = selector;
	SET(ctx->aio_st.st.st_flags,STF_READ|STF_REV);
	ctx->aio_st.st.e[OP_READ].arg = &ctx->aio_st;
	ctx->aio_st.st.e[OP_READ].result = result_aio_event;
	ctx->aio_st.st.fd = eventfd(0, EFD_CLOEXEC);

	epevent.data.ptr = &ctx->aio_st.st;
	if (epoll_ctl(ctx->kdpfd, EPOLL_CTL_ADD, ctx->aio_st.st.fd, &epevent)) {
		perror("epoll_ctl");
	}
	memset(&ctx->aio_st.aio_ctx,0,sizeof(ctx->aio_st.aio_ctx));
	if (io_setup(128, &ctx->aio_st.aio_ctx)) {
			perror("io_setup");
	}

	selector->ctx = (void *)ctx;
}
static void epoll_selector_destroy(kselector *selector)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	close(es->kdpfd);
	close(es->notice_st.st.fd);
	xfree(es);
}
static void epoll_selector_next(kselector *selector, result_callback result, void *arg, int got)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	kselector_notice *notice = (kselector_notice *)xmalloc(sizeof(kselector_notice));
	memset(notice,0,sizeof(kselector_notice));
	notice->arg = arg;
	notice->result = result;
	notice->got = got;
	kmutex_lock(&es->notice_st.lock);
	notice->next = es->notice_st.head;
	es->notice_st.head = notice;
	kmutex_unlock(&es->notice_st.lock);
	uint64_t value = 1;
	write(es->notice_st.st.fd,&value,sizeof(value));
	return;
}
static bool epoll_selector_readhup(kselector *selector, kselectable *st, result_callback result, void *arg)
{
	return false;
}
static bool epoll_selector_remove_readhup(kselector *selector, kselectable *st)
{
	return false;
}
static bool epoll_selector_listen(kselector *selector, kserver_selectable *ss, result_callback result)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	struct epoll_event ev;
	kselectable *st = &ss->st;
	st->e[OP_READ].arg = ss;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = NULL;
	st->e[OP_WRITE].result = NULL;
	SOCKET sockfd = st->fd;
	int poll_op;
	if (TEST(st->st_flags,STF_REV)) {
		poll_op = EPOLL_CTL_MOD;
	} else {
		poll_op = EPOLL_CTL_ADD;
	}
	CLR(st->st_flags,STF_WRITE|STF_RDHUP);
	SET(st->st_flags,STF_READ|STF_REV);
	ev.events = EPOLLIN;
	ev.data.ptr = st;
	int ret = epoll_ctl(es->kdpfd, poll_op, sockfd, &ev);
	if (ret !=0) {
		klog(KLOG_ERR, "epoll add listen event error: fd=%d,errno=%d %s\n", sockfd,errno,strerror(errno));
		return false;
	}
	return true;
}
static bool epoll_selector_connect(kselector *selector, kselectable *st, result_callback result, void *arg)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	assert(TEST(st->st_flags,STF_READ|STF_WRITE|STF_RDHUP|STF_REV|STF_WEV)==0);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = NULL;
	SET(st->st_flags,STF_WRITE);
	if (!TEST(st->st_flags,STF_WEV|STF_REV)) {
		if (!epoll_add_event(es->kdpfd,st,STF_WEV|STF_REV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	kselector_add_list(selector,st,KGL_LIST_CONNECT);
	return true;
}
static void epoll_selector_remove(kselector *selector, kselectable *st)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	if (!TEST(st->st_flags,STF_REV|STF_WEV)) {
		//socket not set event
		return;
	}
	kselector_remove_list(selector,st);
	SOCKET sockfd = st->fd;
#ifndef NDEBUG
	if (TEST(st->st_flags,STF_ET)) {
		assert(TEST(st->st_flags,STF_READ|STF_WRITE|STF_RDHUP)==0);
	}
	klog(KLOG_DEBUG,"removeSocket st=%p,sockfd=%d\n",st,sockfd);
#endif
	struct epoll_event ev;
	CLR(st->st_flags,STF_REV|STF_WEV|STF_ET|STF_RREADY|STF_WREADY);
	if (epoll_ctl(es->kdpfd, EPOLL_CTL_DEL,sockfd, &ev) != 0) {
		klog(KLOG_ERR, "epoll del sockfd error: fd=%d,errno=%d\n", sockfd,errno);
		return;
	}	
}
#if 0
static bool epoll_read_hup(kselector *selector,kselectable *st,result_callback result,buffer_callback buffer,void *arg)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	//printf("st=[%p] read_hup\n",st);
#ifdef EPOLLRDHUP
	if (TEST(st->st_flags,STF_READ|STF_WRITE)) {
		return false;
	}
	SET(st->st_flags,STF_RDHUP);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	if (!TEST(st->st_flags,STF_WEV|STF_REV)) {
		if (!epoll_add_event(es->kdpfd,st,STF_WEV|STF_REV)) {
			CLR(st->st_flags,STF_RDHUP);
			return false;
		}
	}
	return true;
#else
	return false;
#endif
}
#endif
static bool epoll_selector_recvfrom(kselector *selector, kselectable *st, result_callback result, buffer_callback buffer, buffer_callback addr_buffer, void *arg)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	assert(TEST(st->st_flags,STF_READ|STF_WRITE|STF_RECVFROM)==0);
	st->e[OP_READ].arg = arg;
        st->e[OP_READ].result = result;
        st->e[OP_READ].buffer = buffer;
	st->e[OP_WRITE].buffer = addr_buffer;
        SET(st->st_flags,STF_RECVFROM);
	if (TEST(st->st_flags,STF_RREADY)) {
                kselector_add_list(selector,st,KGL_LIST_READY);
                return true;
        }
        if (!TEST(st->st_flags,STF_REV)) {
                if (!epoll_add_event(es->kdpfd,st,STF_REV)) {
                        CLR(st->st_flags,STF_RECVFROM);
                        return false;
                }
        }
        if (st->queue.next==NULL) {
                kselector_add_list(selector,st,KGL_LIST_RW);
        }
        return true;
}
static bool epoll_selector_read(kselector *selector, kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
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
		if (!epoll_add_event(es->kdpfd,st,STF_REV)) {
			CLR(st->st_flags,STF_READ);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		kselector_add_list(selector,st,KGL_LIST_RW);
	}
	return true;
}
static bool epoll_selector_write(kselector *selector, kselectable *st, result_callback result, buffer_callback buffer, void *arg)
{
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
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
		if (!epoll_add_event(es->kdpfd,st,STF_REV|STF_WEV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		kselector_add_list(selector,st,KGL_LIST_RW);
	}
	return true;
}
static void epoll_selector_select(kselector *selector) {
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	struct epoll_event events[MAXEVENT];
	uint32_t ev;
	int ret = 0;
	for (;;) {
#ifdef MALLOCDEBUG
		if (kselector_can_close(selector)) {
			return;
		}
#endif
		kselector_check_timeout(selector,ret);
		ret = epoll_wait(es->kdpfd, events, MAXEVENT,SELECTOR_TMO_MSEC);
		if (selector->utm) {
			kselector_update_time();	
		}
		//if (ret>0) {
		//	printf("epoll_wait ret=[%d]\n",ret);
		//}
		for (int n = 0; n < ret; ++n) {
			kselectable *st = ((kselectable *) events[n].data.ptr);
			ev = events[n].events;
			bool in_ready_list = false;
#ifndef NDEBUG
			//klog(KLOG_DEBUG,"event happened st=[%p] ev=[%d]\n",st,ev);
#endif
			//if (TEST(ev, EPOLLHUP | EPOLLERR)) {
			//	SET(st->st_flags, STF_ERR);
			//}
#ifdef EPOLLRDHUP
			if (TEST(ev,EPOLLRDHUP|EPOLLIN)==(EPOLLRDHUP|EPOLLIN)) {
				SET(st->st_flags, STF_ERR);
			}
#endif
			//write ready
			if (TEST(ev,EPOLLRDHUP)) {
				SET(st->st_flags,STF_WREADY);
				if (TEST(st->st_flags,STF_WRITE|STF_RDHUP)) {
					kselector_add_list(selector,st,KGL_LIST_READY);
					in_ready_list = true;
				}
			} else if (TEST(ev,EPOLLOUT)) {
				SET(st->st_flags,STF_WREADY);
				if (TEST(st->st_flags,STF_WRITE)) {
					kselector_add_list(selector,st,KGL_LIST_READY);
					in_ready_list = true;
				}
			}
			//read ready
			if (TEST(ev,EPOLLIN|EPOLLPRI)) {
				SET(st->st_flags,STF_RREADY);
				if (TEST(st->st_flags,STF_READ|STF_RECVFROM) && !in_ready_list) {
					kselector_add_list(selector,st,KGL_LIST_READY);
				}
			}
		}
	}
}
kasync_file *epoll_selector_aio_open(kselector *selector, FILE_HANDLE fd)
{
	kasync_file *aio_file = xmemory_new(kasync_file);
	memset(aio_file,0,sizeof(kasync_file));
	aio_file->fd = fd;
	return aio_file;
}
bool epoll_selector_aio_write(kselector *selector, kasync_file *file, char *buf, int64_t offset, int length, aio_callback cb, void *arg)
{
	katom_inc((void *)&kgl_aio_count);
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	// katom_inc((void *)&kgl_aio_count);
	file->buf = buf;
	file->arg = arg;
	file->cb = cb;
	file->length = length;
	file->offset_adjust = 0;
	file->length = length;
	int length2 = kgl_align(length,kgl_aio_align_size);
	assert(offset == kgl_align(offset,kgl_aio_align_size));
	assert(buf == (char *)kgl_align_ptr(buf,kgl_aio_align_size));
	struct iocb *iocb = &file->iocb;

	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = file->fd;
	iocb->aio_lio_opcode = IOCB_CMD_PWRITE;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (__u64)(uintptr_t)buf;
	iocb->aio_nbytes = length2;
	iocb->aio_offset = offset;
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = es->aio_st.st.fd;
	iocb->aio_data = (__u64)(uintptr_t)file;
	if (io_submit(es->aio_st.aio_ctx, 1, &iocb)==1) {
		return true;
	}
	perror("io_submit write");
	katom_dec((void *)&kgl_aio_count);
	return false;

}
bool epoll_selector_aio_read(kselector *selector, kasync_file *file, char *buf, int64_t offset, int length, aio_callback cb, void *arg)
{
	katom_inc((void *)&kgl_aio_count);
	kepoll_selector *es = (kepoll_selector *)selector->ctx;
	//katom_inc((void *)&kgl_aio_count);
	file->buf = buf;
	file->arg = arg;
	file->cb = cb;
	file->length = length;
	INT64 offset2 = kgl_align_floor(offset,kgl_aio_align_size);
	int length2 = kgl_align(length,kgl_aio_align_size);
	if (length2==0) {
		length2 = kgl_aio_align_size;
	}
	file->offset_adjust = (int)(offset - offset2);
	struct iocb *iocb = &file->iocb;

	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = file->fd;
	iocb->aio_lio_opcode = IOCB_CMD_PREAD;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (__u64)(uintptr_t)buf;
	iocb->aio_nbytes = length2;
	iocb->aio_offset = offset2;
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = es->aio_st.st.fd;
	iocb->aio_data = (__u64)(uintptr_t)file;
	//printf("read set iocb=[%p]\n",iocb);
	if (io_submit(es->aio_st.aio_ctx, 1, &iocb)==1) {
		return true;
	}
	perror("io_submit read");
	katom_dec((void *)&kgl_aio_count);
	return false;
}
static kselector_module epoll_selector_module = {
	"epoll",
	epoll_selector_init,
	epoll_selector_destroy,
	kselector_default_bind,
	epoll_selector_listen,
	epoll_selector_connect,
	epoll_selector_remove,
	epoll_selector_read,
	epoll_selector_write,
	epoll_selector_readhup,
	epoll_selector_remove_readhup,
	epoll_selector_recvfrom,
	epoll_selector_select,
	epoll_selector_next,
	epoll_selector_aio_open,
	epoll_selector_aio_write,
	epoll_selector_aio_read
};
void kepoll_module_init() {
	kgl_selector_module = epoll_selector_module;
}
#endif
