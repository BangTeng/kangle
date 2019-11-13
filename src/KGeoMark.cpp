#include "KGeoMark.h"
#include "kselector_manager.h"
#include "KSimulateRequest.h"

kev_result geo_mark_timer(void *arg,int got)
{
	KGeoMark *gm = (KGeoMark *)arg;
	gm->flush_timer_callback();
	return kev_ok;
}
kev_result geo_mark_download(void *arg, int status)
{
	KGeoMark *gm = (KGeoMark *)arg;
	gm->download_callback(status);
	return kev_ok;
}
void KGeoMark::download_callback(int status)
{
	lock.WLock();
	flush_timer = false;
	std::string file;
	if (isAbsolutePath(this->file.c_str())) {
		file = this->file;
	} else {
		file = conf.path + this->file;
	}
	if (status == 200) {
		load_data(file.c_str());
		lock.WUnlock();
		release();
		return;
	}
	if (status == 404) {
		//remove local
		unlink(file.c_str());
		im.clear();
		clean_env();
	} else {
		add_flush_timer(this->flush_time);
	}
	lock.WUnlock();
	release();
	return;
}
void KGeoMark::load_data(const char *file)
{	
	time_t last_modified = kfile_last_modified(file);
	if (last_modified == 0) {
		add_flush_timer(0);
	} else {
		add_flush_timer(this->flush_time);
	}
	if (last_modified == this->last_modified) {
		return;
	}
	this->last_modified = last_modified;
	im.clear();
	clean_env();
	pool = kgl_create_pool(KGL_REQUEST_POOL_SIZE);
	KStreamFile lf;
	if (!lf.open(file)) {
		return;
	}
	geo_lable *current_lable = NULL;
	for (;;) {
		char *line = lf.read();
		if (line == NULL) {
			break;
		}
		if (*line == '*') {
			current_lable = build_lable(line + 1);
			total_item++;
			continue;
		}
		if (current_lable == NULL) {
			continue;
		}
		im.add_addr(line, current_lable);
	}
}
void KGeoMark::add_flush_timer(int timer)
{
	if (flush_timer) {
		return;
	}
	flush_timer = true;
	this->addRef();
	selector_manager_add_timer(geo_mark_timer, this,timer*1000,NULL);
}
void KGeoMark::flush_timer_callback()
{
	if (getRef() == 1) {
		flush_timer = false;
		this->release();
		return;
	}
	lock.RLock();
	kassert(flush_timer);
	if (this->url == NULL) {
		flush_timer = false;
		lock.RUnlock();
		this->release();
		return;
	}
	std::string file;
	if (isAbsolutePath(this->file.c_str())) {
		file = this->file;
	} else {
		file = conf.path + this->file;
	}
	char *url = strdup(this->url);
	lock.RUnlock();
#ifdef ENABLE_SIMULATE_HTTP
	async_download(this->url, file.c_str(), geo_mark_download, this);
#else
	geo_mark_download(this, 302);
#endif
	xfree(url);
}
