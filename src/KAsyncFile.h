#ifndef KASYNCSELECTABLE_H
#define KASYNCSELECTABLE_H
#include "KSelectable.h"
#include "KFile.h"
#ifndef _WIN32
#include <libaio.h>
#endif
extern int kgl_aio_align_size;
void init_aio_align_size();
void *aio_alloc_buffer(size_t size);
void aio_free_buffer(void *buf);
void resultAsyncFileEvent(void *arg, int got);
class KAsyncFile
#ifdef _WIN32
	: public KSelectable
#endif
{
public:
	void event(char *buf,int got);
#ifndef _WIN32
	struct iocb iocb;
	int offset_adjust;
	int length;
#endif
	char *buf;
	void *arg;
	aio_callback cb;
	KFile *fp;
};
#endif
