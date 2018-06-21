#ifndef KASYNCSELECTABLE_H
#define KASYNCSELECTABLE_H
#include "KSelectable.h"
#include "KFile.h"
#ifdef LINUX
#include <libaio.h>
#elif BSD_OS
#include <aio.h>
#endif
extern int kgl_aio_align_size;
void init_aio_align_size();
void *aio_alloc_buffer(size_t size);
void aio_free_buffer(void *buf);
void resultAsyncFileEvent(void *arg, int got);
class KAsyncFile
#ifndef LINUX
	: public KSelectable
#endif
{
public:
	void event(char *buf,int got);
#ifdef LINUX
	struct iocb iocb;
	int offset_adjust;
	int length;
#elif BSD_OS
	struct aiocb iocb;
#endif
	char *buf;
	void *arg;
	aio_callback cb;
	KFile *fp;
};
#endif
