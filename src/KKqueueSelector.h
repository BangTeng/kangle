#ifndef KKQUEUESELECTOR_H_
#define KKQUEUESELECTOR_H_
#include "global.h"
#ifdef BSD_OS
#include <sys/event.h>
#include "KSelector.h"
#include "malloc_debug.h"
#include "KEpollNoticeSelectable.h"
class KKqueueSelector : public KSelector
{
public:
	const char *getName()
	{
		return "kqueue";
	}
	KKqueueSelector();
	virtual ~KKqueueSelector();
	friend class KEpollNoticeSelectable;
protected:
	void select();
        bool listen(KServerSelectable *st,resultEvent result);
        bool read(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
        bool read_hup(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
        void remove_read_hup(KSelectable *st);
        bool write(KSelectable *st,resultEvent result,bufferEvent buffer,void *arg);
        bool next(resultEvent result,void *arg,int got);
        bool connect(KSelectable *st,resultEvent result,void *arg);
        bool aio_read(KAsyncFile *file,char *buf,INT64 offset,int length,aio_callback cb,void *arg); 
        bool aio_write(KAsyncFile *file, char *buf, INT64 offset, int length, aio_callback cb, void *arg);
        KAsyncFile *aio_open(KFile *fp); 
	void removeSocket(KSelectable *st);
	int get_kernel_fd()
	{
		return kdpfd;
	}
private:
	bool add_event(KSelectable *st,uint16_t ev);
	void handle_read_event(KSelectable *st);
	void handle_write_event(KSelectable *st);
	int kdpfd;
	KEpollNoticeSelectable *notice_st;
};
#endif 
#endif
