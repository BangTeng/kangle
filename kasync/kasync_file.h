#ifndef KASYNC_FILE_H_99
#define KASYNC_FILE_H_99
#include "kfeature.h"
#include "kselectable.h"
#ifdef LINUX
#include <linux/aio_abi.h>
#elif BSD_OS
#include <aio.h>
#endif
KBEGIN_DECLS
extern int kgl_aio_align_size;
void init_aio_align_size();
void *aio_alloc_buffer(size_t size);
void aio_free_buffer(void *buf);
kev_result result_async_file_event(void *arg, int got);
struct kasync_file_s {
#ifndef LINUX
	kselectable st;
#endif
#ifdef LINUX
	struct iocb iocb;
	int offset_adjust;
	int length;
	FILE_HANDLE fd;
#elif BSD_OS
	struct aiocb iocb;
#endif
	char *buf;
	void *arg;
	aio_callback cb;
};
void async_file_event(kasync_file *fp,char *buf,int got);
void kasync_file_close(kasync_file *fp);
KEND_DECLS
#endif
