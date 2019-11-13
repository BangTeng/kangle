#include "kfile.h"
#include "kasync_file.h"
#include "klog.h"
#include "katom.h"
#include "kmalloc.h"
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
	if (mntfile == NULL) {
		kgl_aio_align_size = 4096;
		klog(KLOG_ERR, "open /proc/mounts file failed. now set kgl_aio_align_size=[%d]\n", kgl_aio_align_size);
		return;
	}
	struct mntent *mntent;
	while (NULL != (mntent = getmntent(mntfile))) {
		int fd = open(mntent->mnt_fsname, O_RDONLY);
		if (fd < 0) {
			continue;
		}
		int lbs = 0;
		if (0 == ioctl(fd, BLKSSZGET, &lbs)) {
			if (lbs > kgl_aio_align_size) {
				kgl_aio_align_size = lbs;
			}
		}
		close(fd);
	}
	endmntent(mntfile);
#endif
	klog(KLOG_ERR, "kgl_aio_align_size=[%d]\n", kgl_aio_align_size);
}
kev_result result_async_file_event(void *arg, int got)
{
	katom_dec((void *)&kgl_aio_count);
	kasync_file *file = (kasync_file *)arg;
#ifdef BSD_OS
	got = aio_return(&file->iocb);
#endif
	return file->cb(file, file->arg, file->buf, got);
}
void *aio_alloc_buffer(size_t size)
{
#ifdef _WIN32
	return xmalloc(size);
#else
	size = kgl_align(size, kgl_aio_align_size);
	return kgl_memalign(kgl_aio_align_size, size);
#endif
}
void aio_free_buffer(void *buf)
{
#ifdef _WIN32
	xfree(buf);
#else
	kgl_align_free(buf);
#endif
}
void kasync_file_close(kasync_file *fp)
{
#ifndef LINUX
	kfclose((FILE_HANDLE)fp->st.fd);
#else
	kfclose(fp->fd);
#endif
	xfree(fp);
}
void async_file_event(kasync_file *fp,char *buf,int got)
{
	katom_dec((void *)&kgl_aio_count);
	aio_callback cb2 = fp->cb;
	fp->cb = NULL;
	cb2(fp,fp->arg, buf, got);
}
