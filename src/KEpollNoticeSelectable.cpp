/*
 * KEpollNoticeSelectable.cpp
 *
 *      Author: keengo
 */
#include "global.h"
#include "KEpollNoticeSelectable.h"
#ifdef LINUX
#include <sys/eventfd.h>
#include "KEpollSelector.h"
#elif BSD_OS
#include "KKqueueSelector.h"
#endif
void result_notice_event(void *arg,int got)
{
	KEpollNoticeSelectable *ast = (KEpollNoticeSelectable *)arg;
	assert(got==0);
	ast->event();
}
#ifdef LINUX
KEpollNoticeSelectable::KEpollNoticeSelectable(KEpollSelector *selector)
#else
KEpollNoticeSelectable::KEpollNoticeSelectable(KKqueueSelector *selector)
#endif
{
	KSelectable *st = static_cast<KSelectable *>(this);
	memset(st,0,sizeof(KSelectable));
	this->selector = selector;
	notice_list = NULL;
	st->set_flag(STF_READ|STF_REV);
	st->e[OP_READ].arg = this;
	st->e[OP_READ].result = result_notice_event;
	st->e[OP_READ].buffer = NULL;
#ifdef LINUX
	fd = eventfd(0, EFD_CLOEXEC);
	if (fd == -1) {
		perror("eventfd");
	}
	struct epoll_event epevent;
	epevent.events = EPOLLIN;
	epevent.data.ptr = st;
	if (epoll_ctl(selector->get_epoll_fd(), EPOLL_CTL_ADD, fd, &epevent)) {
		perror("epoll_ctl");
		return ;
	}
#else
	EV_SET(&notify,0,EVFILT_USER,EV_ADD|EV_CLEAR,0,0,0);
        if(kevent(selector->get_kernel_fd(), &notify, 1, NULL, 0, NULL)==-1){
                perror("kevent");
        }
        EV_SET(&notify,0,EVFILT_USER,0,NOTE_TRIGGER,0,(void *)static_cast<KSelectable *>(this));
#endif
}
KEpollNoticeSelectable::~KEpollNoticeSelectable()
{
#ifdef LINUX
	close(fd);
#endif
}
bool KEpollNoticeSelectable::notice(KSelectable *st,int got)
{
	st->next_got = got;
	lock.Lock();
	st->queue.next = notice_list;
	notice_list = &st->queue;
	lock.Unlock();
#ifdef LINUX
	uint64_t value = 1;
	return write(fd,&value,sizeof(value))==sizeof(value);
#else
	return true;
#endif
}

void KEpollNoticeSelectable::event()
{
#ifdef LINUX
	uint64_t value;
	if (::read(fd, &value, sizeof(value)) != sizeof(value)) {
		perror("read");
		return;
	}
	while (value>0) {
		lock.Lock();
		kgl_list *l = notice_list;
		assert(l!=NULL);
		KSelectable *st = kgl_list_data(l,KSelectable,queue);
		notice_list = notice_list->next;
		lock.Unlock();
		value --;
		st->e[OP_READ].result(st->e[OP_READ].arg,st->next_got);
		delete st;
	}
#else
	kgl_list *l;
	lock.Lock();
	l = notice_list;
	notice_list = NULL;
	lock.Unlock();
	while (l) {
		kgl_list *next = l->next;
		KSelectable *st = kgl_list_data(l,KSelectable,queue);
		st->e[OP_READ].result(st->e[OP_READ].arg,st->next_got);
                delete st;
		l = next;
	}
#endif
}
