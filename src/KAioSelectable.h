#ifndef KAIO_SELECTABLE_H
#define KAIO_SELECTABLE_H 1
#include <linux/aio_abi.h>
#include "KSelectable.h"
class KEpollSelector;
class KAioSelectable : public KSelectable
{
public:
	KAioSelectable(KEpollSelector *selector);
	~KAioSelectable();
	void event();
	KAsyncFile *open(KFile *fp);
	bool read(KAsyncFile *file,char *buf,INT64 offset,int length,aio_callback cb,void *arg);
	bool write(KAsyncFile *file,char *buf,INT64 offset,int length,aio_callback cb,void *arg);
private:
	aio_context_t aio_ctx;
};
#endif
