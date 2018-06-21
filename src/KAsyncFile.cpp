#include "KAsyncFile.h"
#include "KMemPool.h"
#include "KHttpRequest.h"
#include "katom.h"
#ifdef LINUX
#include <mntent.h>
#include <sys/ioctl.h>  
#include <linux/fs.h> 
#endif
int kgl_aio_align_size = 512;
void init_aio_align_size()
{
	kgl_aio_align_size = 512;
#ifdef LINUX
	FILE *mntfile = setmntent("/proc/mounts", "r");
	if (mntfile==NULL) {
		kgl_aio_align_size = 4096;
		klog(KLOG_ERR,"open /proc/mounts file failed. now set kgl_aio_align_size=[%d]\n",kgl_aio_align_size);
		return;
	}
	struct mntent *mntent;
	while (NULL!=(mntent = getmntent(mntfile))) {
		int fd = open(mntent->mnt_fsname, O_RDONLY);  
		if (fd<0) {
			continue;
		}
		int lbs = 0; 
		if (0==ioctl(fd, BLKSSZGET, &lbs)) {
			if (lbs>kgl_aio_align_size) {
				kgl_aio_align_size = lbs;
			}
		}
		close(fd);
	}
	endmntent(mntfile);
#endif
	klog(KLOG_ERR,"kgl_aio_align_size=[%d]\n",kgl_aio_align_size);
}
void *aio_alloc_buffer(size_t size)
{	
#ifdef _WIN32
	return malloc(size);
#else
	size = kgl_align(size, kgl_aio_align_size);
	return kgl_memalign(kgl_aio_align_size, size);
#endif
}
void aio_free_buffer(void *buf)
{
#ifdef _WIN32
	return free(buf);
#else
	kgl_align_free(buf);
#endif
}
void resultAsyncFileEvent(void *arg, int got)
{
	KAsyncFile *file = (KAsyncFile *)arg;
#ifdef BSD_OS
	got =  aio_return(&file->iocb);
#endif
	file->event(file->buf,got);
}
void KAsyncFile::event(char *result_buf,int got)
{
	aio_callback cb2 = cb;
	cb = NULL;
	katom_dec((void *)&kgl_aio_count);
	cb2(this,arg, result_buf, got);
}
