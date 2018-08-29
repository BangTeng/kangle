#include "global.h"
#ifdef LINUX
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <assert.h>
#include <sys/syscall.h>
#include "KAioSelectable.h"
#include "KAsyncFile.h"
#include "KMemPool.h"
#include "KHttpRequest.h"
#include "KEpollSelector.h"
#define NUM_EVENTS 128
int io_setup(u_int nr_reqs, aio_context_t *ctx)
{
    return syscall(SYS_io_setup, nr_reqs, ctx);
}


int io_destroy(aio_context_t ctx)
{
    return syscall(SYS_io_destroy, ctx);
}


int io_getevents(aio_context_t ctx, long min_nr, long nr, struct io_event *events,
    struct timespec *tmo)
{
    return syscall(SYS_io_getevents, ctx, min_nr, nr, events, tmo);
}
int io_submit(aio_context_t ctx, long n, struct iocb **paiocb)
{
    return syscall(SYS_io_submit, ctx, n, paiocb);
}

void resultAioEvent(void *arg,int got)
{
	KAioSelectable *ast = (KAioSelectable *)arg;
	assert(got==0);
	ast->event();
}
void aio_result(KAsyncFile *ctx,struct iocb *iocb, long res, long res2)
{
	if (res<=0) {
		ctx->event(NULL,res);
		return;
	}
	//printf("aio_result res=[%d] iocb=[%p],offset_adjust=[%d]\n",res,iocb,ctx->offset_adjust);
	int length = res - ctx->offset_adjust;
	char *buf = ctx->buf + ctx->offset_adjust;
	ctx->event(buf,MIN(length,ctx->length));
}
KAioSelectable::KAioSelectable(KEpollSelector *selector)
{
	KSelectable *st = static_cast<KSelectable *>(this);
	memset(st,0,sizeof(KSelectable));
	this->selector = selector;
	fd = eventfd(0, EFD_CLOEXEC);
	if (fd == -1) {
		perror("eventfd");
	}
	st->set_flag(STF_READ|STF_REV);
	st->e[OP_READ].arg = this;
	st->e[OP_READ].result = resultAioEvent;
	st->e[OP_READ].buffer = NULL;
	struct epoll_event epevent;
	epevent.events = EPOLLIN;
	epevent.data.ptr = st;
	if (epoll_ctl(selector->get_epoll_fd(), EPOLL_CTL_ADD, fd, &epevent)) {
		perror("epoll_ctl");
		return ;
	}
	memset(&aio_ctx,0,sizeof(aio_ctx));
	if (io_setup(128, &aio_ctx)) {
		perror("io_setup");
	}
}
KAioSelectable::~KAioSelectable()
{
	if (fd!=-1) {
		close(fd);
		fd = -1;
	}
	io_destroy(aio_ctx);
}
void KAioSelectable::event()
{
	uint64_t finished_aio;
	int i,j,r;
	struct timespec tms;
	if (::read(fd, &finished_aio, sizeof(finished_aio)) != sizeof(finished_aio)) {
	    perror("read");
	    return;
	}
	struct io_event events[NUM_EVENTS];
	//printf("finished io number: %d\n", (int)finished_aio);

	while (finished_aio > 0)  {
		tms.tv_sec = 0;
		tms.tv_nsec = 0;
		int get_events = finished_aio;
		if (get_events>NUM_EVENTS) {
			get_events = NUM_EVENTS;
		}
		r = io_getevents(aio_ctx, 1, get_events, events, &tms);
		if (r > 0) {
			for (j = 0; j < r; ++j) {
				KAsyncFile *ctx = (KAsyncFile *)events[j].data;
				aio_result(ctx,(iocb *)events[j].obj, events[j].res, events[j].res2);
				//((io_callback_t)(events[j].data))(aio_ctx, events[j].obj, events[j].res, events[j].res2);
			}
			i += r;
			finished_aio -= r;
		}
	}
}
KAsyncFile *KAioSelectable::open(KFile *fp)
{
	KAsyncFile *aio_file = new KAsyncFile;
	memset(aio_file,0,sizeof(struct KAsyncFile));
	aio_file->fp = fp;
	return aio_file;
}
bool KAioSelectable::read(KAsyncFile *ctx,char *buf,INT64 offset,int length,aio_callback cb,void *arg)
{
	//printf("aio read buf=[%p] offset=[%d],length=[%d]\n",buf,(int)offset,length);
	katom_inc((void *)&kgl_aio_count);
	ctx->buf = buf;
	ctx->arg = arg;
	ctx->cb = cb;
	ctx->length = length;
	 INT64 offset2 = kgl_align_floor(offset,kgl_aio_align_size);
	 int length2 = kgl_align(length,kgl_aio_align_size);
	 if (length2==0) {
		 length2 = kgl_aio_align_size;
	 }
	 ctx->offset_adjust = (int)(offset - offset2);
	 /*
	 if (length2!=length) {
		 printf("aio read length adjust from [%d] to [%d]\n",length,length2);
	 }
	 if (offset2!=offset) {
		 printf("aio read offset adjust from [%d] to [%d]\n",offset ,offset2);
	 }
	 //*/
	 struct iocb *iocb = &ctx->iocb;

	memset(iocb, 0, sizeof(*iocb));
	iocb->aio_fildes = ctx->fp->getHandle();
	iocb->aio_lio_opcode = IOCB_CMD_PREAD;
	iocb->aio_reqprio = 0;
	iocb->aio_buf = (__u64)(uintptr_t)buf;
	iocb->aio_nbytes = length2;
	iocb->aio_offset = offset2;
	iocb->aio_flags = IOCB_FLAG_RESFD;
	iocb->aio_resfd = fd;
	 iocb->aio_data = (__u64)(uintptr_t)ctx;
	 //printf("read set iocb=[%p]\n",iocb);
	 if (io_submit(aio_ctx, 1, &iocb)==1) {
		 return true;
	 }
	 perror("io_submit read");
	 katom_dec((void *)&kgl_aio_count);
	 return false;
}
bool KAioSelectable::write(KAsyncFile *ctx,char *buf,INT64 offset,int length,aio_callback cb,void *arg)
{
	//printf("aio write buf=[%p] offset=[%d],length=[%d]\n",buf,(int)offset,length);
	katom_inc((void *)&kgl_aio_count);
	ctx->buf = buf;
	ctx->arg = arg;
	ctx->cb = cb;
	ctx->length = length;
	ctx->offset_adjust = 0;
	ctx->length = length;
	int length2 = kgl_align(length,kgl_aio_align_size);
	assert(offset == kgl_align(offset,kgl_aio_align_size));
	assert(buf == (char *)kgl_align_ptr(buf,kgl_aio_align_size));
	 struct iocb *iocb = &ctx->iocb;

	 memset(iocb, 0, sizeof(*iocb));
        iocb->aio_fildes = ctx->fp->getHandle();
        iocb->aio_lio_opcode = IOCB_CMD_PWRITE;
        iocb->aio_reqprio = 0;
        iocb->aio_buf = (__u64)(uintptr_t)buf;
        iocb->aio_nbytes = length2;
        iocb->aio_offset = offset;
        iocb->aio_flags = IOCB_FLAG_RESFD;
        iocb->aio_resfd = fd;
         iocb->aio_data = (__u64)(uintptr_t)ctx;

	 //io_prep_pwrite(iocb, ctx->fp->getHandle(), buf, length2,offset);
	// io_set_eventfd(iocb, fd);
	 //iocb->data = ctx;
	 if (io_submit(aio_ctx, 1, &iocb)==1) {
		 return true;
	 }
	 perror("io_submit write");
	 katom_dec((void *)&kgl_aio_count);
	 return false;
}
#endif
