/*
 * ksapi.h
 *
 *  Created on: 2010-6-13
 *      Author: keengo
 */

#ifndef KSAPI_H_
#define KSAPI_H_
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
#define   KGL_REQ_REGISTER_ACCESS                  (KGL_REQ_RESERV_COMMAND+8)
#define   KGL_REQ_ONREADY                          (KGL_REQ_RESERV_COMMAND+9)
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
	KTHREAD_FUNCTION (* thread_function)(void *param,int msec);
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
/* http头回调,code>1000，为kangle内部产生的错误代码 */
typedef int (WINAPI *http_header_hook)(void *arg,int code,struct KHttpHeader *header);
/* http内容回调 data=NULL 结束 */
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

enum KF_REQ_TYPE
{
	KF_REQ_DISABLE_NOTIFICATIONS = 8,
	KF_REQ_CONNECT_CLEAN         = 9,
	KF_REQ_REQUEST_CLEAN         = 10,
	KF_REQ_REWRITE_URL           = 11,
	KF_REQ_SAVE_POST             = 12,
	KF_REQ_RESTORE_POST          = 13,
} ;
enum KF_STATUS_TYPE
{
	KF_STATUS_REQ_FINISHED             = 0x8000000,
	KF_STATUS_REQ_FINISHED_KEEP_CONN   = 0x8000001,
	KF_STATUS_REQ_NEXT_NOTIFICATION    = 0x8000002,
	KF_STATUS_REQ_HANDLED_NOTIFICATION = 0x8000003,
	KF_STATUS_REQ_ERROR                = 0x8000004,
	KF_STATUS_REQ_READ_NEXT            = 0x8000005,
	KF_STATUS_REQ_ACCESS_TRUE          = 0x8000006,
	KF_STATUS_REQ_ACCESS_FALSE         = 0x8000007,
};
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
} KGL_RESULT;
typedef enum _KF_ALLOC_MEMORY_TYPE
{
	KF_ALLOC_CONNECT = 0,
	KF_ALLOC_REQUEST = 1
} KF_ALLOC_MEMORY_TYPE;
typedef struct _kgl_filter_context
{
	DWORD          cbSize;
	DWORD          Revision;
	PVOID          ServerContext;
	DWORD          ulReserved;
	PVOID          pModelContext;
	PVOID          pFilterContext;
	KGL_RESULT (WINAPI * get_variable) (
		struct _kgl_filter_context * pfc,
		LPSTR                        lpszVariableName,
		LPVOID                       lpvBuffer,
		LPDWORD                      lpdwSize
	);
	KGL_RESULT (WINAPI * add_headers) (
		struct _kgl_filter_context * pfc,
		LPSTR                        attr,
		LPSTR                        val,
		DWORD                        dwReserved
	);
	KGL_RESULT (WINAPI * write_client) (
		struct _kgl_filter_context * pfc,
		LPVOID                       Buffer,
		LPDWORD                      lpdwBytes,
		DWORD                        dwReserved
	);
	VOID * (WINAPI * alloc_memory) (
		struct _kgl_filter_context * pfc,
		DWORD                        cbSize,
		KF_ALLOC_MEMORY_TYPE         memory_type
	);
	KGL_RESULT (WINAPI * support_function) (
		struct _kgl_filter_context * pfc,
		enum KF_REQ_TYPE             kfReq,
		PVOID                        pData,
		DWORD                        ul1,
		DWORD                        ul2
	);
}  kgl_filter_context;

typedef struct _kgl_filter_data
{
	PVOID       buffer;
	DWORD       length;
	DWORD       reserved;
	PVOID       stack_ctx;
	PVOID       (WINAPI *stack_alloc)(PVOID stack_ctx,DWORD size);
} kgl_filter_data;

typedef struct _kgl_filter_request
{
	KGL_RESULT (WINAPI * get_header) (
		kgl_filter_context * pfc,
		LPSTR                         lpszName,
		LPVOID                        lpvBuffer,
		LPDWORD                       lpdwSize
	);
	KGL_RESULT (WINAPI * set_header) (
		kgl_filter_context* pfc,
		LPSTR                         lpszName,
		LPSTR                         lpszValue
	);
	KGL_RESULT (WINAPI * add_header) (
		kgl_filter_context * pfc,
		LPSTR                         lpszName,
		LPSTR                         lpszValue
	);
	DWORD HttpStatus;              
	DWORD dwReserved;
} kgl_filter_request;

typedef kgl_filter_request kgl_filter_response;


typedef struct _kgl_filter_url_map
{
	const CHAR * pszURL;
	CHAR *       pszPhysicalPath;
	DWORD        cbPathBuff;
} kgl_filter_url_map;
typedef struct _kgl_call_back
{
	void (WINAPI * call_back)(void *arg);
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

typedef struct _kgl_filter_version
{
	DWORD  server_filter_version;
	DWORD  filter_version;
	CHAR   filter_desc[KF_MAX_FILTER_DESC_LEN];
	DWORD  flags;
	PVOID   ctx;
	KGL_RESULT (WINAPI * global_support_function) (
		PVOID                        ctx,
		DWORD                        req,
		PVOID                        data,
		PVOID                        *ret
	);
	KGL_RESULT (WINAPI * get_variable) (
		PVOID                        ctx,
		LPSTR                        lpszVariableName,
		LPVOID                       lpvBuffer,
		LPDWORD                      lpdwSize
	);
} kgl_filter_version;
#define KGL_BUILD_HTML_ENCODE   1
typedef struct _kgl_access_build
{
	void *server_ctx;
	void *model_ctx;
	int (WINAPI * write_string) (
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
	const char *(WINAPI * get_value) (
		void *server_ctx,
		const char *name);

} kgl_access_parse;
enum KF_ACCESS_BUILD_TYPE
{
	KF_ACCESS_BUILD_HTML = 0,
	KF_ACCESS_BUILD_SHORT,
	KF_ACCESS_BUILD_XML,
};
typedef struct _kgl_access
{
	const char *name;
	int flags;
	void *(WINAPI * create_ctx)();
	void (WINAPI * free_ctx)(void *ctx);
	KGL_RESULT (WINAPI * build)(kgl_access_build *build_ctx,enum KF_ACCESS_BUILD_TYPE build_type);
	KGL_RESULT (WINAPI * parse)(kgl_access_parse *parse_ctx);
	DWORD (WINAPI *process)(kgl_filter_context * ctx, DWORD  type, void * data);
} kgl_access;
DLL_PUBLIC DWORD WINAPI kgl_filter_process(kgl_filter_context * ctx,DWORD  type, void * data);
DLL_PUBLIC BOOL  WINAPI kgl_filter_init(kgl_filter_version * ver);
DLL_PUBLIC BOOL  WINAPI kgl_filter_finit(DWORD flag);
#ifdef __cplusplus
 }
#endif
#endif /* SAPI_H_ */
