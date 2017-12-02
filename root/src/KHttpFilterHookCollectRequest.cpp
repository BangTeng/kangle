#include "KHttpFilterHookCollectRequest.h"
#include "http.h"
#ifdef ENABLE_KSAPI_FILTER
static KGL_RESULT WINAPI  GetHeader (
    kgl_filter_context * pfc,
    LPSTR                         lpszName,
    LPVOID                        lpvBuffer,
    LPDWORD                       lpdwSize)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	KHttpHeader *header = rq->parser.getHeaders();
	while (header) {
		if (is_attr(header,lpszName)) {
			DWORD len = strlen(header->val) + 1;
			*lpdwSize = MIN(*lpdwSize,len);
			memcpy(lpvBuffer,header->val,*lpdwSize);
			return KGL_OK;
		}
		header = header->next;
	}
	return KGL_EUNKNOW;
}
static KGL_RESULT WINAPI  SetHeader (
    kgl_filter_context * pfc,
    LPSTR                         lpszName,
    LPSTR                         lpszValue)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	KHttpHeader *header = rq->parser.getHeaders();
	while (header) {
		if (is_attr(header,lpszName)) {
			xfree(header->val);
			header->val = xstrdup(lpszValue);
			return KGL_OK;
		}
		header = header->next;
	}
	return KGL_EUNKNOW;
}
static KGL_RESULT WINAPI  AddHeader (
    kgl_filter_context * pfc,
    LPSTR                         lpszName,
    LPSTR                         lpszValue)
{
	KHttpRequest *rq = (KHttpRequest *)pfc->ServerContext;
	rq->parser.insertHeader(lpszName,strlen(lpszName),lpszValue,strlen(lpszValue));
	return KGL_OK;
}
int KHttpFilterHookCollectRequest::check_request(KHttpRequest *rq)
{
	kgl_filter_request notification;
	init_kgl_filter_request(notification);
	switch (process(rq,KF_NOTIFY_REQUEST,&notification)) {
	case KF_STATUS_REQ_HANDLED_NOTIFICATION:
	case KF_STATUS_REQ_NEXT_NOTIFICATION:
	case KF_STATUS_REQ_READ_NEXT:
		return JUMP_ALLOW;
	default:
		return JUMP_DENY;
	}
}
void init_kgl_filter_request(kgl_filter_request &notification)
{
	memset(&notification,0,sizeof(notification));
	notification.add_header = AddHeader;
	notification.get_header = GetHeader;
	notification.set_header = SetHeader;
}
#endif
