/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#ifndef KEPOLLSELECTOR_H_
#define KEPOLLSELECTOR_H_
#include "global.h"
#ifdef HAVE_SYS_EPOLL_H
#include <sys/epoll.h>
#define ENABLE_ONESHOT_MODEL   1
#include "KSelector.h"
#include "KAioSelectable.h"
#include "KEpollNoticeSelectable.h"
#include "malloc_debug.h"

class KEpollSelector : public KSelector
{
public:
	const char *getName()
	{
		return "epoll";
	}
	KEpollSelector();
	virtual ~KEpollSelector();
	void select();
	bool listen(KServerSelectable *st,resultEvent result);
	bool read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
	bool read_hup(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
	void remove_read_hup(KSelectable *st);
	bool write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
	bool next(resultEvent result,void *arg,int got);
	bool connect(KSelectable *st,resultEvent result,void *arg);
	bool aio_read(KAsyncFile *file,char *buf,INT64 offset,int length,aio_callback cb,void *arg)
	{
		return aio_st->read(file,buf,offset,length,cb,arg);
	}
	bool aio_write(KAsyncFile *file, char *buf, INT64 offset, int length, aio_callback cb, void *arg)
	{
		return aio_st->write(file,buf,offset,length,cb,arg);
	}
	KAsyncFile *aio_open(KFile *fp)
	{
			return aio_st->open(fp);
	}
	int get_epoll_fd()
	{
		return kdpfd;
	}
protected:
	bool addSocket(KSelectable *rq,int op);
	void removeSocket(KSelectable *rq);
private:
	bool add_event(KSelectable *st,uint16_t ev);
	void handle_read_event(KSelectable *st);
	void handle_write_event(KSelectable *st);
	int kdpfd;
	KAioSelectable *aio_st;
	KEpollNoticeSelectable *notice_st;
};
#endif
#endif /*KEPOLLSELECTOR_H_*/
