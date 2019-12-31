/*
 * ksapi.h
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef KSAPI_H_
#define KSAPI_H_
#include "kfeature.h"
#include "kforwin32.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef unsigned short hlen_t;
struct	KHttpHeader {
	char *attr;
	char *val;
	hlen_t attr_len;
	hlen_t val_len;
	struct	KHttpHeader	*next;
};
#if defined _WIN32 || defined __CYGWIN__
      #define DLL_PUBLIC __declspec(dllexport)
#else
  #if __GNUC__ >= 4
    #define DLL_PUBLIC __attribute__ ((visibility("default")))
  #else
    #define DLL_PUBLIC
  #endif
#endif
#ifdef _WIN32
#include <windows.h>
#else
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_NO_DATA             232 
#define ERROR_INVALID_INDEX       1413
#define WINAPI  
#define APIENTRY
#define MAX_PATH	260
#define IN
#define MAKELONG(x,y) ((x)|y<<16)
#ifndef FALSE
#define FALSE	0
#endif
#ifndef TRUE
#define TRUE	1
#endif
#define DLL_PROCESS_ATTACH	1
#define DLL_THREAD_ATTACH	2
#define DLL_THREAD_DETACH	3
#define DLL_PROCESS_DETACH	0
typedef unsigned int        DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef void                VOID;
typedef VOID *              LPVOID;
typedef VOID *              PVOID;
typedef char                CHAR;
typedef CHAR *              LPSTR;
typedef BYTE *              LPBYTE;
typedef DWORD *             LPDWORD;

typedef void *              HANDLE;
typedef const char *        LPCSTR;
typedef unsigned short      USHORT;
typedef unsigned long long  ULONGLONG;
typedef int                 HRESULT;
typedef void *              HMODULE;
typedef void *              HINSTANCE;
void SetLastError(DWORD errorCode);
#endif
#ifdef _WIN32
#define PIPE_T	HANDLE
#define ClosePipe	CloseHandle
#define INVALIDE_PIPE	INVALID_HANDLE_VALUE
#define KTHREAD_FUNCTION  void
#define KTHREAD_RETURN    return
#else
#define PIPE_T	int
#define ClosePipe	::close
#define INVALIDE_PIPE	-1
typedef void * KTHREAD_FUNCTION;
#define KTHREAD_RETURN do { return NULL; }while(0)
#endif
#define   KGL_REQ_RESERV_COMMAND                    100000
#define   KGL_REQ_COMMAND                          (KGL_REQ_RESERV_COMMAND+1)
#define   KGL_REQ_THREAD                           (KGL_REQ_RESERV_COMMAND+2)
#define   KGL_REQ_CREATE_WORKER                    (KGL_REQ_RESERV_COMMAND+3)
#define   KGL_REQ_RELEASE_WORKER                   (KGL_REQ_RESERV_COMMAND+4)
#define   KGL_REQ_SERVER_VAR                       (KGL_REQ_RESERV_COMMAND+5)
#define   KGL_REQ_TIMER                            (KGL_REQ_RESERV_COMMAND+6)
#define   KGL_REQ_ASYNC_HTTP                       (KGL_REQ_RESERV_COMMAND+7)
#define   KGL_REQ_ONREADY                          (KGL_REQ_RESERV_COMMAND+8)
#define   KGL_REQ_REGISTER_ACCESS                  (KGL_REQ_RESERV_COMMAND+9)
#define   KGL_REQ_REGISTER_SYNC_UPSTREAM           (KGL_REQ_RESERV_COMMAND+10)
#define   KGL_REQ_REGISTER_ASYNC_UPSTREAM          (KGL_REQ_RESERV_COMMAND+11)

typedef struct _kgl_command_env
{
	char *name;
	char *val;
	struct _kgl_command_env *next;
} kgl_command_env;
typedef struct _kgl_process_std
{	
	PIPE_T hstdin;
	PIPE_T hstdout;
	PIPE_T hstderr;
	const char *stdin_file;
	const char *stdout_file;
	const char *stderr_file;
} kgl_process_std;
typedef struct _kgl_command
{
	const char *vh;
	const char *cmd;
	const char *dir;
	kgl_command_env *env;
	kgl_process_std std;
} kgl_command;
typedef struct _kgl_thread
{
	kev_result (*thread_function)(void *param,int msec);
	void *param;
	void *worker;
} kgl_thread;
typedef struct _kgl_timer
{
	void (WINAPI *timer_run) (void *arg);
	int msec;
	void *arg;
	unsigned short selector;
} kgl_timer;
#define ASYNC_HOOK_OK          0
#define ASYNC_HOOK_ERROR       1
/* todo:读入post回调 */
typedef int (WINAPI *http_post_hook) (void *arg,char *buf,int len);
/* 返回0成功，其他错误*/
typedef int (WINAPI *http_header_hook)(void *arg,int code,struct KHttpHeader *header);
/* http内容回调 data=NULL 结束 ,返回0成功，其他错误*/
typedef int (WINAPI *http_body_hook)(void *arg,const char *data,int len);
#define KF_SIMULATE_CACHE 1
#define KF_SIMULATE_DELTA 2
#define KF_SIMULATE_LOCAL 4
#define KF_SIMULATE_GZIP  8
typedef struct _kgl_async_http
{
	void *arg;
	const char *host;
	unsigned short port;
	unsigned short selector;
	int life_time;
	const char *meth;
	const char *url;
	int postLen;
	int flags;
	struct KHttpHeader *rh;
	http_post_hook post;
	http_header_hook header;
	http_body_hook body;
} kgl_async_http;

/*******************************http filter ****************************************/

#define KGL_FILTER_REVISION    MAKELONG( 0, 6 )

#define KF_MAX_USERNAME         (256+1)
#define KF_MAX_PASSWORD         (256+1)
#define KF_MAX_AUTH_TYPE        (32+1)

#define KF_MAX_FILTER_DESC_LEN  (256+1)

typedef enum _KF_REQ_TYPE
{
	KF_REQ_DISABLE_NOTIFICATIONS = 8,
	KF_REQ_CONNECT_CLEAN         = 9,
	KF_REQ_REQUEST_CLEAN         = 10,
	KF_REQ_REWRITE_URL           = 11,
	KF_REQ_SAVE_POST             = 12,
	KF_REQ_RESTORE_POST          = 13,
	KF_REQ_UPSTREAM              = 14,
} KF_REQ_TYPE;

typedef enum _KF_STATUS_TYPE
{
	KF_STATUS_REQ_FINISHED             = 0x8000000,
	KF_STATUS_REQ_FINISHED_KEEP_CONN   = 0x8000001,
	KF_STATUS_REQ_HANDLED              = 0x8000003,
	KF_STATUS_REQ_ERROR                = 0x8000004,
	KF_STATUS_REQ_ACCESS_TRUE          = 0x8000006,
	KF_STATUS_REQ_ACCESS_FALSE         = 0x8000007,
} KF_STATUS_TYPE;

typedef enum _KGL_RESULT
{
	KGL_OK                         = 0,
	KGL_EINSUFFICIENT_BUFFER       = 1,
	KGL_ENO_DATA                   = 2,
	KGL_EINVALID_PARAMETER         = 3,
	KGL_EINVALID_INDEX             = 4,
	KGL_ENOT_READY                 = 5,
	KGL_EUNKNOW                    = 10,
	KGL_EHAS_SEND_HEADER           = 11,
	KGL_ENOT_SUPPORT               = 12
} KGL_RESULT;

typedef enum {
	KGL_VAR_HEADER = 0,
	KGL_VAR_SSL_VAR,
	KGL_VAR_HTTPS,//int
	KGL_VAR_SERVER_PROTOCOL,
	KGL_VAR_SERVER_NAME,
	KGL_VAR_REQUEST_METHOD,
	KGL_VAR_PATH_INFO,
	KGL_VAR_REQUEST_URI,
	KGL_VAR_SCRIPT_NAME,
	KGL_VAR_QUERY_STRING,
	KGL_VAR_SERVER_ADDR,
	KGL_VAR_SERVER_PORT,//uint16_t
	KGL_VAR_REMOTE_ADDR,
	KGL_VAR_REMOTE_PORT,//uint16_t
	KGL_VAR_PEER_ADDR,
	KGL_VAR_DOCUMENT_ROOT,
	KGL_VAR_CONTENT_LENGTH, //int64_t
	KGL_VAR_CONTENT_TYPE,
	KGL_VAR_IF_MODIFIED_SINCE,//time_t
	KGL_VAR_IF_NONE_MATCH,
	KGL_VAR_IF_RANGE_TIME,//time_t
	KGL_VAR_IF_RANGE_STRING,
	KGL_VAR_CONTENT_LEFT
} KGL_VAR;

typedef LPVOID KCONN;
typedef LPVOID KSOCKET;
typedef KGL_RESULT(*kgl_get_variable_f) (KCONN cn, KGL_VAR type, LPSTR  name, LPVOID value, LPDWORD size);

typedef enum _KF_ALLOC_MEMORY_TYPE
{
	KF_ALLOC_CONNECT = 0,
	KF_ALLOC_REQUEST = 1
} KF_ALLOC_MEMORY_TYPE;

typedef struct _kgl_access_context
{
	DWORD          size;
	DWORD          ver;
	KCONN          cn;
	PVOID          model_ctx;
	kgl_get_variable_f get_variable;
	KGL_RESULT(*support_function) (
		KCONN cn,
		KF_REQ_TYPE					 kfReq,		
		PVOID                        pData,
		PVOID                        *ret);

	KGL_RESULT (*write_client) (
		KCONN cn,
		LPVOID                       lpvBuffer,
		LPDWORD                      lpdwSize);
	VOID * (*alloc_memory) (
		KCONN cn,
		DWORD                        cbSize,
		KF_ALLOC_MEMORY_TYPE         memory_type);
	KGL_RESULT(*set_header) (
		KCONN cn,
		LPSTR                         lpszName,
		LPSTR                         lpszValue);
	KGL_RESULT(*add_header) (
		KCONN cn,
		LPSTR                         lpszName,
		LPSTR                         lpszValue);
	KGL_RESULT(*response_header) (
		KCONN cn,
		const char *attr,
		hlen_t attr_len,
		const char *val,
		hlen_t val_len);
}  kgl_access_context;


typedef struct _kgl_async_context {
	DWORD         size;
	DWORD         ver;
	KCONN         cn;
	PVOID         model_ctx;
	kgl_get_variable_f get_variable;
	KGL_RESULT(*response_unknow_header) (
		KCONN cn,
		const char *attr,
		hlen_t attr_len,
		const char *val,
		hlen_t val_len);
	KGL_RESULT (*response_know_header)();

	kev_result (*start_response_body) (KCONN cn);

	KGL_RESULT (*push_data) (
		KCONN    cn,
		LPVOID   Buffer,
		DWORD    lpdwBytes);

	kev_result(*next_upstream)(
		KCONN cn,
		void *queue);

	kev_result (*try_write) (
		KCONN    cn);

	kev_result (*read_post)  (
		KCONN    cn,
		LPVOID   lpvBuffer,
		DWORD    lpdwSize);

	kev_result (*end_request) (
		KCONN    cn,
		BOOL expected_end);

	KGL_RESULT (*support_function) (
		KCONN      cn,
		DWORD      dwHSERequest,
		LPVOID     lpvBuffer,
		LPDWORD    lpdwSize,
		LPDWORD    lpdwDataType);

}kgl_async_context;

typedef struct _kgl_filter_data
{
	PVOID       buffer;
	DWORD       length;
	DWORD       reserved;
	PVOID       stack_ctx;
	PVOID       (*stack_alloc)(PVOID stack_ctx,DWORD size);
} kgl_filter_data;


typedef struct _kgl_filter_url_map
{
	const CHAR * pszURL;
	CHAR *       pszPhysicalPath;
	DWORD        cbPathBuff;
} kgl_filter_url_map;

typedef struct _kgl_call_back
{
	void (*call_back)(void *arg);
	void *arg;
} kgl_call_back;
#define KF_NOTIFY_READ_DATA                 0x00008000
#define KF_NOTIFY_REQUEST                   0x00004000
#define KF_NOTIFY_URL_MAP                   0x00001000
#define KF_NOTIFY_RESPONSE                  0x00000040
#define KF_NOTIFY_SEND_DATA                 0x00000400
#define KF_NOTIFY_END_REQUEST               0x00000080
#define KF_NOTIFY_END_CONNECT               0x00000100


#define KF_NOTIFY_REQUEST_MARK            0x00100000
#define KF_NOTIFY_RESPONSE_MARK           0x00200000
#define KF_NOTIFY_REQUEST_ACL             0x00400000
#define KF_NOTIFY_RESPONSE_ACL            0x00800000

#define KGL_BUILD_HTML_ENCODE   1
typedef struct _kgl_access_build
{
	void *server_ctx;
	void *model_ctx;
	int (* write_string) (
		void *server_ctx,
		const char *str,
		int len,
		int build_flags
		);
} kgl_access_build;
typedef struct _kgl_access_parse
{
	void *server_ctx;
	void *model_ctx;
	const char *(*get_value) (
		void *server_ctx,
		const char *name);

} kgl_access_parse;
typedef enum _KF_ACCESS_BUILD_TYPE
{
	KF_ACCESS_BUILD_HTML = 0,
	KF_ACCESS_BUILD_SHORT,
	KF_ACCESS_BUILD_XML,
} KF_ACCESS_BUILD_TYPE;

typedef struct _kgl_access
{
	const char *name;
	int32_t flags;
	void *(*create_ctx)();
	void (* free_ctx)(void *ctx);
	KGL_RESULT (* build)(kgl_access_build *build_ctx,KF_ACCESS_BUILD_TYPE build_type);
	KGL_RESULT (* parse)(kgl_access_parse *parse_ctx);
	KF_STATUS_TYPE(*process)(kgl_access_context *ctx,DWORD notify);
} kgl_access;

#define KF_UPSTREAM_SYNC 1
typedef struct {
	const char *name;
	int32_t flags;
} kgl_upstream;

typedef struct _kgl_sync_upstream
{
	const char *name;
	int32_t flags;
	void *(*create_ctx)();
	void (*free_ctx)(void *ctx);
	DWORD (*process)(kgl_access_context *ctx);
} kgl_sync_upstream;

typedef struct _kgl_async_upstream
{
	const char *name;
	int32_t flags;
	void *(*create_ctx)();
	void (*free_ctx)(void *ctx);
	kev_result (*open)(kgl_async_context *ctx);
	kev_result (*read_body)(kgl_async_context *ctx);
	void (*close)(kgl_async_context *ctx);
	kev_result (*read_post_callback)(kgl_async_context *ctx,int got);	
} kgl_async_upstream;

typedef kev_result(*socket_callback)(void *arg, int result);

typedef struct _kgl_dso_version
{
	DWORD  server_version;
	DWORD  dso_version;
	CHAR   dso_desc[KF_MAX_FILTER_DESC_LEN];
	DWORD  flags;
	PVOID   ctx;	
	KGL_RESULT (*global_support_function) (
		PVOID                        ctx,
		DWORD                        req,
		PVOID                        data,
		PVOID                        *ret
		);
	KGL_RESULT(*get_variable) (
		PVOID                        ctx,
		LPSTR                        lpszVariableName,
		LPVOID                       lpvBuffer,
		LPDWORD                      lpdwSize
		);
	kev_result (*write_socket) (
		KSOCKET s,
		LPWSABUF buf,
		int bc,
		socket_callback cb,
		void *arg);
	kev_result (*read_socket)(
		KSOCKET s,
		LPDWORD buf,
		DWORD len,
		socket_callback cb,
		void *arg);
	void (*bind_socket)(KSOCKET s);
	KSOCKET (*create_socket)(DWORD flags);
	kev_result(*connect_socket)(KSOCKET s, const char *host, int port, const char *ssl, socket_callback cb,void *arg);
	void(*close_socket)(KSOCKET s);
	int(*get_selector_count)();
	int(*get_selector_index)();
} kgl_dso_version;

#define KGL_REGISTER_ACCESS(dso_version,access) dso_version->global_support_function(dso_version->ctx,KGL_REQ_REGISTER_ACCESS,access,NULL)
#define KGL_REGISTER_SYNC_UPSTREAM(dso_version,us) dso_version->global_support_function(dso_version->ctx,KGL_REQ_REGISTER_SYNC_UPSTREAM,us,NULL)
#define KGL_REGISTER_ASYNC_UPSTREAM(dso_version,us) dso_version->global_support_function(dso_version->ctx,KGL_REQ_REGISTER_ASYNC_UPSTREAM,us,NULL)

DLL_PUBLIC BOOL kgl_dso_init(kgl_dso_version * ver);
DLL_PUBLIC BOOL kgl_dso_finit(DWORD flag);
#ifdef __cplusplus
 }
#endif
#endif /* SAPI_H_ */
