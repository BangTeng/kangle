#include <stdio.h>
#include "KTsUpstream.h"
#include "kselectable.h"
#include "kconnection.h"

static kev_result next_ts_destroy(void *arg, int got)
{
	KUpstream *us = (KUpstream *)arg;
	us->Destroy();
	return kev_ok;
}
static kev_result next_ts_gc(void *arg, int got)
{
	KUpstream *us = (KUpstream *)arg;
	us->Gc(got,us->expire_time);
	return kev_ok;
}
static kev_result next_ts_read_header(void *arg, int got)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	return ts_us->NextReadHeader();
}
static kev_result next_ts_read(void *arg, int got)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	return ts_us->NextRead();
}
static kev_result next_ts_write(void *arg, int got)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	return ts_us->NextWrite();
}
static kev_result result_ts_read2(void *arg, int got)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	kassert(kselector_is_same_thread(ts_us->main_selector));
	kassert(ts_us->read_lock);
	ts_us->read_lock = 0;
	return ts_us->stack[OP_READ].result(ts_us->stack[OP_READ].arg, got);
}
static kev_result result_ts_read(void *arg, int got)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	kgl_selector_module.next(ts_us->main_selector, result_ts_read2, arg, got);
	return kev_ok;
}
static kev_result result_ts_write2(void *arg, int got)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	kassert(kselector_is_same_thread(ts_us->main_selector));
	kassert(ts_us->write_lock);
	ts_us->write_lock = 0;
	return ts_us->stack[OP_WRITE].result(ts_us->stack[OP_WRITE].arg, got);
}
static kev_result result_ts_write(void *arg, int got)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	kgl_selector_module.next(ts_us->main_selector, result_ts_write2, arg, got);
	return kev_ok;
}
static int buffer_ts_read(void *arg, LPWSABUF buf, int bc)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	return ts_us->stack[OP_READ].buffer(buf, bc);
}
static int buffer_ts_write(void *arg, LPWSABUF buf, int bc)
{
	KTsUpstream *ts_us = (KTsUpstream *)arg;
	return ts_us->stack[OP_WRITE].buffer(buf, bc);
}
kev_result KTsUpstream::NextReadHeader()
{
	if (write_end) {
		write_end = 0;
		us->WriteEnd();		
	}
	if (!us->ReadHttpHeader(this,result_ts_read)) {
		result_ts_read(this, -1);
	}
	return kev_ok;
}
bool KTsUpstream::ReadHttpHeader(void *arg,  result_callback result)
{
	if (!us->IsMultiStream()) {
		return false;
	}
	kassert(read_lock == 0);
	read_lock = 1;
	stack[OP_READ].arg = arg;
	stack[OP_READ].result = result;
	selectable_next(&us->GetConnection()->st, next_ts_read_header, this, 0);
	return true;
}
kev_result KTsUpstream::Connect(void *arg, result_callback result)
{
	fprintf(stderr, "never goto here.\n");
	kassert(false);
	return kev_err;
}
kev_result KTsUpstream::NextRead()
{
	if (shutdown) {
		us->Shutdown();
	}
	return us->Read(this, result_ts_read, buffer_ts_read);
}
kev_result KTsUpstream::NextWrite()
{
	if (shutdown) {
		us->Shutdown();
	}
	return us->Write(this, result_ts_write, buffer_ts_write);
}
kev_result KTsUpstream::Read(void *arg, result_callback result, buffer_callback buffer)
{
	//printf("ts_upstream read.\n");
	kassert(kselector_is_same_thread(main_selector));
	kassert(read_lock == 0);
	read_lock = 1;
	stack[OP_READ].arg = arg;
	stack[OP_READ].result = result;
	stack[OP_READ].bc = buffer(arg, stack[OP_READ].buf, TS_UPSTREAM_BUFFER_COUNT);
	selectable_next(&us->GetConnection()->st, next_ts_read, this, 0);
	return kev_ok;
}
kev_result KTsUpstream::Write(void *arg, result_callback result, buffer_callback buffer)
{
	//printf("ts_upstream write.\n");
	kassert(kselector_is_same_thread(main_selector));
	kassert(write_lock == 0);
	write_lock = 1;
	stack[OP_WRITE].arg = arg;
	stack[OP_WRITE].result = result;
	stack[OP_WRITE].bc = buffer(arg, stack[OP_WRITE].buf, TS_UPSTREAM_BUFFER_COUNT);
	selectable_next(&us->GetConnection()->st, next_ts_write, this, 0);
	return kev_ok;
}
void KTsUpstream::WriteEnd()
{
	if (us->IsMultiStream()) {
		write_end = 1;
		return;
	}
	us->WriteEnd();
}
void KTsUpstream::Shutdown()
{
	if (us->IsMultiStream()) {
		//http2²ÉÓÃÈíshutdown
		//printf("*****************use soft shutdown.\n");
		shutdown = 1;
		return;
	}
	us->Shutdown();
}
void KTsUpstream::Destroy()
{
	fprintf(stderr, "never goto here.\n");
	kassert(false);
	if (us) {
		selectable_next(&us->GetConnection()->st, next_ts_destroy, us, 0);
		us = NULL;
	}
	delete this;
}
bool KTsUpstream::IsLocked()
{
	return read_lock || write_lock;
}
void KTsUpstream::Gc(int life_time,time_t base_time)
{
	us->expire_time = base_time;
	selectable_next(&us->GetConnection()->st, next_ts_gc, us, life_time);
	us = NULL;
	delete this;
}
