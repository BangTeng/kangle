#include "kforwin32.h"
#include "KHttpFilterDso.h"
#include "KDynamicString.h"
#include "log.h"
#include "KAccess.h"
#include "KAccessDso.h"
#include "kselector_manager.h"
#include "KTimer.h"
#include "kmalloc.h"
#include "KHttpFilterContext.h"
#include "KAccessDsoSupport.h"

#ifdef ENABLE_KSAPI_FILTER
KGL_RESULT global_get_variable(
	PVOID                        ctx,
	LPSTR                        lpszVariableName,
	LPVOID                       lpvBuffer,
	LPDWORD                      lpdwSize
	)
{
	//KDsoExtend *dso = (KDsoExtend *)ctx;
	const char *val = getSystemEnv(lpszVariableName);
	if (val == NULL || *val == '\0') {
		return KGL_EINVALID_PARAMETER;
	}
	return add_api_var(lpvBuffer, lpdwSize, val);
}

KGL_RESULT global_support_function(
	PVOID                        ctx,
	DWORD                        req,
	PVOID                        data,
	PVOID                        *ret
	)
{
	switch (req) {
	case KGL_REQ_REGISTER_ACCESS: {
			kgl_access *access = (kgl_access *)data;
			if (TEST(access->flags, KF_NOTIFY_REQUEST_ACL)) {
				KAccessDso *model = new KAccessDso(access,
					(KDsoExtend *)ctx,
					KF_NOTIFY_REQUEST_ACL);
				KAccess::addAclModel(REQUEST, new KAccessDsoAcl(model));
			}
			if (TEST(access->flags, KF_NOTIFY_RESPONSE_ACL)) {
				KAccessDso *model = new KAccessDso(access,
					(KDsoExtend *)ctx,
					KF_NOTIFY_RESPONSE_ACL);
				KAccess::addAclModel(RESPONSE, new KAccessDsoAcl(model));
			}
			if (TEST(access->flags, KF_NOTIFY_REQUEST_MARK)) {
				KAccessDso *model = new KAccessDso(access,
					(KDsoExtend *)ctx,
					KF_NOTIFY_REQUEST_MARK);
				KAccess::addMarkModel(REQUEST, new KAccessDsoMark(model));
			}
			if (TEST(access->flags, KF_NOTIFY_RESPONSE_MARK)) {
				KAccessDso *model = new KAccessDso(access,
					(KDsoExtend *)ctx,
					KF_NOTIFY_RESPONSE_MARK);
				KAccess::addMarkModel(RESPONSE, new KAccessDsoMark(model));
			}
			return KGL_OK;
	}
	case KGL_REQ_REGISTER_ASYNC_UPSTREAM:
	{
		kgl_upstream *us = (kgl_upstream *)data;
		CLR(us->flags, KF_UPSTREAM_SYNC);
		KDsoExtend *de = (KDsoExtend *)ctx;
		if (de->RegisterUpstream(us)) {
			return KGL_OK;
		}
		return KGL_EUNKNOW;
	}
	case KGL_REQ_REGISTER_SYNC_UPSTREAM:
	{
		return KGL_ENOT_SUPPORT;
	}
	case KGL_REQ_SERVER_VAR: {
			const char *name = (const char *)data;
			if (name == NULL) {
				return KGL_EINVALID_PARAMETER;
			}
			const char *val = getSystemEnv(name);
			if (val == NULL || *val == '\0') {
				return KGL_EINVALID_PARAMETER;
			}
			//(*ret) = (void *)strdup(val);
			return KGL_OK;
	}
	case KGL_REQ_ONREADY:{
			kgl_call_back *c = (kgl_call_back *)data;
			selector_manager_on_ready(c->call_back, c->arg);
			return KGL_OK;
	}
	case KGL_REQ_TIMER:	{
			kgl_timer *t = (kgl_timer *)data;
			if (!is_selector_manager_init()) {
				return KGL_ENOT_READY;
			}
			timer_run(t->timer_run, t->arg, t->msec, t->selector);
			return KGL_OK;
	}
	case KGL_REQ_CREATE_WORKER:	{
			int *max_worker = (int *)data;
			kasync_worker *worker = kasync_worker_init(*max_worker,0);
			*ret = (void *)worker;
			return KGL_OK;
	}
	case KGL_REQ_RELEASE_WORKER: {
			kasync_worker *worker = (kasync_worker *)data;
			kasync_worker_release(worker);
			return KGL_OK;
	}
	case KGL_REQ_THREAD: {
			kgl_thread *thread = (kgl_thread *)data;
			if (thread->worker) {
				kasync_worker *worker = (kasync_worker *)thread->worker;
				if (!kasync_worker_try_start(worker, thread->param, thread->thread_function,false)) {
					return KGL_EUNKNOW;
				}
				return KGL_OK;
			}
			if (kasync_worker_thread_start(thread->param, thread->thread_function)) {
				return KGL_OK;
			}
			return KGL_EUNKNOW;
	}
	}
	return KGL_EINVALID_PARAMETER;
}
#if 0
KHttpFilterDso::KHttpFilterDso(const char *name)
{
	this->name = xstrdup(name);
	filename = NULL;
	kgl_filter_init = NULL;
	kgl_filter_process = NULL;
	kgl_filter_finit = NULL;
	memset(&version, 0, sizeof(version));
}
KHttpFilterDso::~KHttpFilterDso()
{
	xfree(name);
	if (filename) {
		xfree(filename);
	}
	if (orign_filename) {
		xfree(orign_filename);
	}
}
bool KHttpFilterDso::load(const char *filename)
{
	orign_filename = strdup(filename);
	KDynamicString ds;
	this->filename = ds.parseString(filename);
	if (!dso.load(this->filename)) {
		klog(KLOG_ERR, "cann't load filter [%s]\n", name);
		return false;
	}
	kgl_filter_init = (kgl_filter_init_f)dso.findFunction("kgl_filter_init");
	if (kgl_filter_init == NULL) {
		kgl_filter_init = (kgl_filter_init_f)dso.findFunction("GetFilterVersion");
	}
	if (kgl_filter_init == NULL) {
		klog(KLOG_ERR, "cann't load filter [%s] kgl_filter_init function cann't find\n", name);
		return false;
	}
	kgl_filter_process = (kgl_filter_process_f)dso.findFunction("kgl_filter_process");
	if (kgl_filter_process == NULL) {
		kgl_filter_process = (kgl_filter_process_f)dso.findFunction("HttpFilterProc");
	}
	if (kgl_filter_process == NULL) {
		klog(KLOG_ERR, "cann't load filter [%s] kgl_filter_process function cann't find\n", name);
		return false;
	}
	kgl_filter_finit = (kgl_filter_finit_f)dso.findFunction("kgl_filter_finit");
	if (kgl_filter_finit == NULL) {
		kgl_filter_finit = (kgl_filter_finit_f)dso.findFunction("TerminateFilter");
	}
	version.server_filter_version = 0;
	version.global_support_function = global_support_function;
	version.get_variable = global_get_variable;
	version.ctx = this;
	if (!kgl_filter_init(&version)) {
		klog(KLOG_ERR, "cann't load filter [%s] kgl_filter_init return false\n", name);
		return false;
	}
	return true;
}
kgl_filter_version *KHttpFilterDso::get_version()
{
	return &version;
}
#endif
#endif
