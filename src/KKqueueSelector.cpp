#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "global.h"
#ifdef BSD_OS
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include "log.h"
#include "KSelectorManager.h"
#include "do_config.h"
#include "time_utils.h"
#include "KKqueueSelector.h"
#include "malloc_debug.h"
#define MAXEVENT	256
#ifndef NETBSD
typedef void * kqueue_udata_t;
#else
typedef intptr_t kqueue_udata_t;
#endif
KKqueueSelector::KKqueueSelector() {
	kdpfd = kqueue() ;
	notice_st = new KEpollNoticeSelectable(this);
}

KKqueueSelector::~KKqueueSelector() {
	close(kdpfd);
	delete notice_st;
}
void KKqueueSelector::handle_read_event(KSelectable *st)
{
	//printf("handle_read_event st=[%p]\n",st);
	if (TEST(st->st_flags,STF_ET)) {
		CLR(st->st_flags,STF_READ);
	}
#ifdef ENABLE_KSSL_BIO
	st->lowEventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#else
	st->eventRead(st->e[OP_READ].arg,st->e[OP_READ].result,st->e[OP_READ].buffer);
#endif
}
void KKqueueSelector::handle_write_event(KSelectable *st)
{
	CLR(st->st_flags,STF_WRITE|STF_RDHUP);
#ifdef ENABLE_KSSL_BIO
	st->lowEventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#else
	st->eventWrite(st->e[OP_WRITE].arg,st->e[OP_WRITE].result,st->e[OP_WRITE].buffer);
#endif
}
void KKqueueSelector::select() {
	struct kevent events[MAXEVENT]; 
	struct timespec tm;
	tm.tv_sec = tmo_msec/1000;
	tm.tv_nsec = tmo_msec * 1000 - tm.tv_sec * 1000000;
	for (;;) {
		for (;;) {
			kgl_list *l = klist_head(&list[KGL_LIST_READY]);
			if (l == &list[KGL_LIST_READY]) {
				break;
			}
			KSelectable *st = kgl_list_data(l,KSelectable,queue);
			klist_remove(l);
			memset(l, 0, sizeof(kgl_list));
			count--;
			uint16_t st_flags = st->st_flags;
			if (TEST(st_flags,STF_WREADY) && TEST(st_flags,STF_WRITE|STF_RDHUP)) {
				handle_write_event(st);
				CLR(st_flags,STF_WRITE|STF_RDHUP);
			}
			if (TEST(st_flags,STF_RREADY) && TEST(st_flags,STF_READ)) {
				handle_read_event(st);
				CLR(st_flags,STF_READ);
			}
			if (TEST(st_flags,STF_READ|STF_WRITE) &&
				TEST(st_flags,STF_ET) &&
				st->queue.next==NULL) {
				add_list(st,KGL_LIST_RW);
			}
		}
		checkTimeOut();
		int ret = kevent(kdpfd, NULL, 0, events, MAXEVENT, &tm);
		if (utm) {
			updateTime();
		}
		for (int n = 0; n < ret; ++n) {
			KSelectable *st = (KSelectable *) events[n].udata;
			remove_list(st);
#ifndef NDEBUG
			klog(KLOG_DEBUG,"select st=%p,st_flags=%d,events=%d at %p\n",st,st->st_flags,events[n].filter,pthread_self());
#endif
			switch (events[n].filter) {
			case EVFILT_WRITE:
				SET(st->st_flags,STF_WREADY);
				if (TEST(st->st_flags,STF_WRITE)) {
					add_list(st,KGL_LIST_READY);
				}
				break;
			
			case EVFILT_READ:
			case EVFILT_USER:
				SET(st->st_flags,STF_RREADY);
				if (TEST(st->st_flags,STF_READ) && st->queue.next==NULL) {
					add_list(st,KGL_LIST_READY);
				}
				break;
			case EVFILT_AIO:
				if (TEST(st->st_flags,STF_READ)) {
					SET(st->st_flags,STF_RREADY);
				} else if (TEST(st->st_flags,STF_WRITE)){
					SET(st->st_flags,STF_WREADY);
				}
				add_list(st,KGL_LIST_READY);	
				break;
			default:
				assert(false);
			}
		}
	}

}
void KKqueueSelector::removeSocket(KSelectable *st) {
	if (!TEST(st->st_flags,STF_REV|STF_WEV)) {
		//socket not set event
		return;
	}
#ifndef NDEBUG
	klog(KLOG_DEBUG,"remove socket st=%p\n",st);
#endif
	struct kevent changes[2];
	int ev_count = 0;
	SOCKET sockfd = st->getSocket()->get_socket();
	if (TEST(st->st_flags,STF_REV)) {
		EV_SET(&changes[ev_count++], sockfd, EVFILT_READ, EV_DELETE, 0, 0, NULL); 
	}
	if (TEST(st->st_flags,STF_WEV)) {
		EV_SET(&changes[ev_count++], sockfd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL); 
	}
	kevent(kdpfd, changes, ev_count, NULL, 0, NULL);
	CLR(st->st_flags,STF_REV|STF_WEV|STF_ET|STF_RREADY|STF_WREADY);
}
bool KKqueueSelector::add_event(KSelectable *st,uint16_t ev)
{
	struct kevent changes[2];
	int ev_count = 0;
	SOCKET sockfd = st->getSocket()->get_socket();
	if (TEST(ev,STF_REV)) {
		if (!TEST(st->st_flags,STF_REV)) {
			EV_SET(&changes[ev_count++], sockfd, EVFILT_READ, EV_ADD|EV_CLEAR, 0, 0, (kqueue_udata_t)st);	
			SET(st->st_flags,STF_REV|STF_ET|STF_WREADY);
		}
	}
	if (TEST(ev,STF_WEV)) {
		if (!TEST(st->st_flags,STF_WEV)) {
			EV_SET(&changes[ev_count++], sockfd, EVFILT_WRITE, EV_ADD|EV_CLEAR, 0, 0, (kqueue_udata_t)st);
			SET(st->st_flags,STF_WEV|STF_ET);
		}
	}
	assert(ev_count>0);
	if(kevent(kdpfd, changes, ev_count, NULL, 0, NULL)==-1){
		return false;
	}
	return true;
}
bool KKqueueSelector::read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
	assert(TEST(st->st_flags,STF_READ)==0);
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = buffer;
	SET(st->st_flags,STF_READ);
	CLR(st->st_flags,STF_RDHUP);
	if (TEST(st->st_flags,STF_RREADY)) {
		add_list(st,KGL_LIST_READY);
		return true;
	}
	if (!TEST(st->st_flags,STF_REV)) {
		if (!add_event(st,STF_REV)) {
			CLR(st->st_flags,STF_READ);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		add_list(st,KGL_LIST_RW);
	}
	return true;
}
bool KKqueueSelector::write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
	assert(TEST(st->st_flags,STF_WRITE)==0);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = buffer;
	SET(st->st_flags,STF_WRITE);
	CLR(st->st_flags,STF_RDHUP);
	if (TEST(st->st_flags,STF_WREADY)) {
		add_list(st,KGL_LIST_READY);
		return true;
	}
	if (!TEST(st->st_flags,STF_WEV)) {
		if (!add_event(st,STF_REV|STF_WEV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	if (st->queue.next==NULL) {
		add_list(st,KGL_LIST_RW);
	}
	return true;
}
bool KKqueueSelector::connect(KSelectable *st,resultEvent result,void *arg)
{
	//printf("st=[%p] connect\n",st);
	assert(TEST(st->st_flags,STF_READ|STF_WRITE|STF_RDHUP|STF_REV|STF_WEV)==0);
	st->e[OP_WRITE].arg = arg;
	st->e[OP_WRITE].result = result;
	st->e[OP_WRITE].buffer = NULL;
	SET(st->st_flags,STF_WRITE);
	if (!TEST(st->st_flags,STF_WEV|STF_REV)) {
		if (!add_event(st,STF_WEV|STF_REV)) {
			CLR(st->st_flags,STF_WRITE);
			return false;
		}
	}
	add_list(st,KGL_LIST_CONNECT);
	return true;
}
bool KKqueueSelector::listen(KServerSelectable *st,resultEvent result)
{
 	struct kevent changes[2];
        int ev_count = 0;
        st->e[OP_READ].arg = st;
        st->e[OP_READ].result = result;
        st->e[OP_READ].buffer = NULL;
        SOCKET sockfd = st->getSocket()->get_socket();
        assert(TEST(st->st_flags,STF_READ)==0);
        EV_SET(&changes[ev_count++], sockfd, EVFILT_READ, EV_ADD, 0, 0, (kqueue_udata_t)static_cast<KSelectable *>(st));
        SET(st->st_flags,STF_READ|STF_REV);
        if(kevent(kdpfd, changes, ev_count, NULL, 0, NULL)==-1){
                klog(KLOG_ERR,"cann't addSocket sockfd=%d for read\n",sockfd);
                return false;
        }
        return true;
}
bool KKqueueSelector::next(resultEvent result,void *arg,int got)
{
	KSelectable *st = new KSelectable;
	memset(st,0,sizeof(KSelectable));
	st->selector = this;
	st->e[OP_READ].arg = arg;
	st->e[OP_READ].result = result;
	st->e[OP_READ].buffer = NULL;
	notice_st->notice(st,got);
        if(kevent(kdpfd, &notice_st->notify, 1, NULL, 0, NULL)==-1){
		delete st;
		return false;
	}
	return true;
}
bool KKqueueSelector::read_hup(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg)
{
	return false;
}
void KKqueueSelector::remove_read_hup(KSelectable *st)
{
}
bool KKqueueSelector::aio_read(KAsyncFile *file,char *buf,INT64 offset,int length,aio_callback cb,void *arg)
{
	//printf("try aio_read now st=[%p]\n",static_cast<KSelectable *>(file));
	katom_inc((void *)&kgl_aio_count);
	file->e[OP_READ].result = resultAsyncFileEvent;
	file->e[OP_READ].arg = file;
	file->e[OP_READ].buffer = NULL;

	file->buf = buf;
	file->arg = arg;
	file->cb = cb;

	SET(file->st_flags, STF_READ|STF_ET);
	memset(&file->iocb, 0,sizeof(struct aiocb));
	file->iocb.aio_fildes = file->fd;
	file->iocb.aio_offset = offset;
	file->iocb.aio_buf = buf;
	file->iocb.aio_nbytes = length;
//*
	file->iocb.aio_sigevent.sigev_notify_kqueue = kdpfd;
	file->iocb.aio_sigevent.sigev_notify = SIGEV_KEVENT;
	file->iocb.aio_sigevent.sigev_value.sigval_ptr = file;
//*/
	int n = ::aio_read(&file->iocb);
	if (n==-1) {
		katom_dec((void *)&kgl_aio_count);
		return false;
	}
	return true;
}
bool KKqueueSelector::aio_write(KAsyncFile *file, char *buf, INT64 offset, int length, aio_callback cb, void *arg)
{
	katom_inc((void *)&kgl_aio_count);
        file->e[OP_WRITE].result = resultAsyncFileEvent;
        file->e[OP_WRITE].arg = file;
        file->e[OP_WRITE].buffer = NULL;

        file->buf = buf;
        file->arg = arg;
        file->cb = cb;

        SET(file->st_flags, STF_WRITE|STF_ET);
        memset(&file->iocb, 0,sizeof(struct aiocb));
        file->iocb.aio_fildes = file->fd;
        file->iocb.aio_offset = offset;
        file->iocb.aio_buf = buf;
        file->iocb.aio_nbytes = length;
//*
        file->iocb.aio_sigevent.sigev_notify_kqueue = kdpfd;
        file->iocb.aio_sigevent.sigev_notify = SIGEV_KEVENT;
        file->iocb.aio_sigevent.sigev_value.sigval_ptr = file;
//*/
        int n = ::aio_write(&file->iocb);
        if (n==-1) {
                katom_dec((void *)&kgl_aio_count);
                return false;
        }
        return true;

}
KAsyncFile *KKqueueSelector::aio_open(KFile *fp)
{
	KAsyncFile *aio_file = new KAsyncFile;
	memset(static_cast<KSelectable *>(aio_file), 0, sizeof(KSelectable));
	aio_file->fp = fp;
	aio_file->bind_file_fd(fp->getHandle());
	bindSelectable(aio_file);
	return aio_file;
}
#endif
