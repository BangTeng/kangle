#ifndef _HTTPEXT_H_
#define _HTTPEXT_H_
#include "ksapi.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifndef _WIN32
typedef wchar_t             WCHAR;
typedef wchar_t *           LPWSTR;
#endif
#define   HSE_VERSION_MAJOR          7     
#define   HSE_VERSION_MINOR          0     
#define   HSE_LOG_BUFFER_LEN         80
#define   HSE_MAX_EXT_DLL_NAME_LEN  256

#define   HSE_VERSION     MAKELONG( HSE_VERSION_MINOR, HSE_VERSION_MAJOR )

#define   HSE_STATUS_SUCCESS                       1
#define   HSE_STATUS_SUCCESS_AND_KEEP_CONN         2
#define   HSE_STATUS_PENDING                       3
#define   HSE_STATUS_ERROR                         4



#define   HSE_REQ_BASE                             0
#define   HSE_REQ_SEND_URL_REDIRECT_RESP           ( HSE_REQ_BASE + 1 )
#define   HSE_REQ_SEND_URL                         ( HSE_REQ_BASE + 2 )
#define   HSE_REQ_SEND_RESPONSE_HEADER             ( HSE_REQ_BASE + 3 )
#define   HSE_REQ_DONE_WITH_SESSION                ( HSE_REQ_BASE + 4 )

#define   HSE_REQ_UTF8_TO_LOCALE                   100
#define   HSE_REQ_GET_KREQUEST                     101
#define   HSE_GET_VH_TOKEN                         102
#define   HSE_GET_VH_ENV                           103

#define   HSE_REQ_END_RESERVED                     1000


#define   HSE_REQ_MAP_URL_TO_PATH                  (HSE_REQ_END_RESERVED+1)
#define   HSE_REQ_GET_SSPI_INFO                    (HSE_REQ_END_RESERVED+2)
#define   HSE_APPEND_LOG_PARAMETER                 (HSE_REQ_END_RESERVED+3)
#define   HSE_REQ_IO_COMPLETION                    (HSE_REQ_END_RESERVED+5)
#define   HSE_REQ_TRANSMIT_FILE                    (HSE_REQ_END_RESERVED+6)
#define   HSE_REQ_REFRESH_ISAPI_ACL                (HSE_REQ_END_RESERVED+7)
#define   HSE_REQ_IS_KEEP_CONN                     (HSE_REQ_END_RESERVED+8)
#define   HSE_REQ_ASYNC_READ_CLIENT                (HSE_REQ_END_RESERVED+10)
#define   HSE_REQ_GET_IMPERSONATION_TOKEN          (HSE_REQ_END_RESERVED+11)
#define   HSE_REQ_MAP_URL_TO_PATH_EX               (HSE_REQ_END_RESERVED+12)
#define   HSE_REQ_ABORTIVE_CLOSE                   (HSE_REQ_END_RESERVED+14)
#define   HSE_REQ_GET_CERT_INFO_EX                 (HSE_REQ_END_RESERVED+15)
#define   HSE_REQ_SEND_RESPONSE_HEADER_EX          (HSE_REQ_END_RESERVED+16)
#define   HSE_REQ_CLOSE_CONNECTION                 (HSE_REQ_END_RESERVED+17)
#define   HSE_REQ_IS_CONNECTED                     (HSE_REQ_END_RESERVED+18)
#define   HSE_REQ_MAP_UNICODE_URL_TO_PATH          (HSE_REQ_END_RESERVED+23)
#define   HSE_REQ_MAP_UNICODE_URL_TO_PATH_EX       (HSE_REQ_END_RESERVED+24)
#define   HSE_REQ_EXEC_UNICODE_URL                 (HSE_REQ_END_RESERVED+25)
#define   HSE_REQ_EXEC_URL                         (HSE_REQ_END_RESERVED+26)
#define   HSE_REQ_GET_EXEC_URL_STATUS              (HSE_REQ_END_RESERVED+27)
#define   HSE_REQ_SEND_CUSTOM_ERROR                (HSE_REQ_END_RESERVED+28)
#define   HSE_REQ_IS_IN_PROCESS                    (HSE_REQ_END_RESERVED+30)
#define   HSE_REQ_REPORT_UNHEALTHY                 (HSE_REQ_END_RESERVED+32)
#define   HSE_REQ_NORMALIZE_URL                    (HSE_REQ_END_RESERVED+33)
#define   HSE_REQ_VECTOR_SEND                      (HSE_REQ_END_RESERVED+37)
#define   HSE_REQ_GET_ANONYMOUS_TOKEN              (HSE_REQ_END_RESERVED+38)
#define   HSE_REQ_GET_CACHE_INVALIDATION_CALLBACK  (HSE_REQ_END_RESERVED+40)
#define   HSE_REQ_GET_UNICODE_ANONYMOUS_TOKEN      (HSE_REQ_END_RESERVED+41)
#define   HSE_REQ_GET_TRACE_INFO                   (HSE_REQ_END_RESERVED+42)



#define HSE_TERM_ADVISORY_UNLOAD                   0x00000001
#define HSE_TERM_MUST_UNLOAD                       0x00000002



# define HSE_IO_SYNC                      0x00000001   
# define HSE_IO_ASYNC                     0x00000002   
# define HSE_IO_DISCONNECT_AFTER_SEND     0x00000004   
# define HSE_IO_SEND_HEADERS              0x00000008   
# define HSE_IO_NODELAY                   0x00001000   
# define HSE_IO_FINAL_SEND                0x00000010
# define HSE_IO_CACHE_RESPONSE            0x00000020
typedef   LPVOID          HCONN;

typedef BOOL (WINAPI * ServerSupportFunctionf)( HCONN  hConn,
                                       DWORD      dwHSERequest,
                                       LPVOID     lpvBuffer,
                                       void **ret );
typedef void (WINAPI *ServerFreef)(HCONN hConn,void *ptr);
typedef struct _HSE_VERSION_CONTROL_BLOCK {
	CHAR      lpszExtensionDesc[HSE_MAX_EXT_DLL_NAME_LEN - 4];
	DWORD     cbSize;
	HCONN     ConnID;
	ServerFreef ServerFree;
	ServerSupportFunctionf ServerSupportFunction;
} HSE_VERSION_CONTROL_BLOCK;

typedef struct   _HSE_VERSION_INFO {
	DWORD  dwExtensionVersion;
	union {
		CHAR   lpszExtensionDesc[HSE_MAX_EXT_DLL_NAME_LEN];
		HSE_VERSION_CONTROL_BLOCK hvcb;
	};
} HSE_VERSION_INFO, *LPHSE_VERSION_INFO;

typedef struct _EXTENSION_CONTROL_BLOCK {

    DWORD     cbSize;                
    DWORD     dwVersion;             
    HCONN     ConnID;                
    DWORD     dwHttpStatusCode;      
    CHAR      lpszLogData[HSE_LOG_BUFFER_LEN];

    LPSTR     lpszMethod;            
    LPSTR     lpszQueryString;       
    LPSTR     lpszPathInfo;          
    LPSTR     lpszPathTranslated;    

    DWORD     cbTotalBytes;          
    DWORD     cbAvailable;           
    LPBYTE    lpbData;               

    LPSTR     lpszContentType;       

    BOOL (WINAPI * GetServerVariable) ( HCONN       hConn,
                                        LPSTR       lpszVariableName,
                                        LPVOID      lpvBuffer,
                                        LPDWORD     lpdwSize );

    BOOL (WINAPI * WriteClient)  ( HCONN      ConnID,
                                   LPVOID     Buffer,
                                   LPDWORD    lpdwBytes,
                                   DWORD      dwReserved );

    BOOL (WINAPI * ReadClient)  ( HCONN      ConnID,
                                  LPVOID     lpvBuffer,
                                  LPDWORD    lpdwSize );

    BOOL (WINAPI * ServerSupportFunction)( HCONN      hConn,
                                           DWORD      dwHSERequest,
                                           LPVOID     lpvBuffer,
                                           LPDWORD    lpdwSize,
                                           LPDWORD    lpdwDataType );

} EXTENSION_CONTROL_BLOCK, *LPEXTENSION_CONTROL_BLOCK;





#define HSE_URL_FLAGS_READ          0x00000001    
#define HSE_URL_FLAGS_WRITE         0x00000002    
#define HSE_URL_FLAGS_EXECUTE       0x00000004    
#define HSE_URL_FLAGS_SSL           0x00000008    
#define HSE_URL_FLAGS_DONT_CACHE    0x00000010    
#define HSE_URL_FLAGS_NEGO_CERT     0x00000020    
#define HSE_URL_FLAGS_REQUIRE_CERT  0x00000040    
#define HSE_URL_FLAGS_MAP_CERT      0x00000080    
#define HSE_URL_FLAGS_SSL128        0x00000100    
#define HSE_URL_FLAGS_SCRIPT        0x00000200     

#define HSE_URL_FLAGS_MASK          0x000003ff

typedef struct _HSE_URL_MAPEX_INFO {

    CHAR   lpszPath[MAX_PATH]; 
    DWORD  dwFlags;            
    DWORD  cchMatchingPath;    
    DWORD  cchMatchingURL;      

    DWORD  dwReserved1;
    DWORD  dwReserved2;

} HSE_URL_MAPEX_INFO, * LPHSE_URL_MAPEX_INFO;


typedef struct _HSE_UNICODE_URL_MAPEX_INFO {

    WCHAR  lpszPath[MAX_PATH]; 
    DWORD  dwFlags;            
    DWORD  cchMatchingPath;    
    DWORD  cchMatchingURL;      

} HSE_UNICODE_URL_MAPEX_INFO, * LPHSE_UNICODE_URL_MAPEX_INFO;



typedef VOID
  (WINAPI * PFN_HSE_IO_COMPLETION)(
                                   IN EXTENSION_CONTROL_BLOCK * pECB,
                                   IN PVOID    pContext,
                                   IN DWORD    cbIO,
                                   IN DWORD    dwError
                                   );




typedef struct _HSE_TF_INFO  {


    PFN_HSE_IO_COMPLETION   pfnHseIO;
    PVOID  pContext;

    HANDLE hFile;


    LPCSTR pszStatusCode; 

    DWORD  BytesToWrite;  
    DWORD  Offset;        

    PVOID  pHead;         
    DWORD  HeadLength;    
    PVOID  pTail;         
    DWORD  TailLength;    

    DWORD  dwFlags;       

} HSE_TF_INFO, * LPHSE_TF_INFO;



typedef struct _HSE_SEND_HEADER_EX_INFO  {

    LPCSTR  pszStatus; 
    LPCSTR  pszHeader; 

    DWORD   cchStatus; 
    DWORD   cchHeader; 

    BOOL    fKeepConn; 

} HSE_SEND_HEADER_EX_INFO, * LPHSE_SEND_HEADER_EX_INFO;


#define HSE_EXEC_URL_NO_HEADERS                     0x02
#define HSE_EXEC_URL_IGNORE_CURRENT_INTERCEPTOR     0x04
#define HSE_EXEC_URL_IGNORE_VALIDATION_AND_RANGE    0x10
#define HSE_EXEC_URL_DISABLE_CUSTOM_ERROR           0x20
#define HSE_EXEC_URL_SSI_CMD                        0x40

typedef struct _HSE_EXEC_URL_USER_INFO  {

    HANDLE hImpersonationToken;
    LPSTR pszCustomUserName;
    LPSTR pszCustomAuthType;

} HSE_EXEC_URL_USER_INFO, * LPHSE_EXEC_URL_USER_INFO;

typedef struct _HSE_EXEC_URL_ENTITY_INFO  {
    
    DWORD cbAvailable;
    LPVOID lpbData;
    
} HSE_EXEC_URL_ENTITY_INFO, * LPHSE_EXEC_URL_ENTITY_INFO;


typedef struct _HSE_EXEC_URL_STATUS  {

    USHORT uHttpStatusCode;
    USHORT uHttpSubStatus;
    DWORD dwWin32Error;

} HSE_EXEC_URL_STATUS, * LPHSE_EXEC_URL_STATUS;


typedef struct _HSE_EXEC_URL_INFO  {

    LPSTR pszUrl;                       
    LPSTR pszMethod;                    
    LPSTR pszChildHeaders;              
    LPHSE_EXEC_URL_USER_INFO pUserInfo; 
    LPHSE_EXEC_URL_ENTITY_INFO pEntity; 
    DWORD dwExecUrlFlags;               

} HSE_EXEC_URL_INFO, * LPHSE_EXEC_URL_INFO;



typedef struct _HSE_EXEC_UNICODE_URL_USER_INFO  {

    HANDLE hImpersonationToken;
    LPWSTR pszCustomUserName;
    LPSTR  pszCustomAuthType;

} HSE_EXEC_UNICODE_URL_USER_INFO, * LPHSE_EXEC_UNICODE_URL_USER_INFO;


typedef struct _HSE_EXEC_UNICODE_URL_INFO  {

    LPWSTR pszUrl;                              
    LPSTR  pszMethod;                           
    LPSTR  pszChildHeaders;                     
    LPHSE_EXEC_UNICODE_URL_USER_INFO pUserInfo; 
    LPHSE_EXEC_URL_ENTITY_INFO pEntity;         
    DWORD  dwExecUrlFlags;                      

} HSE_EXEC_UNICODE_URL_INFO, * LPHSE_EXEC_UNICODE_URL_INFO;


typedef struct _HSE_CUSTOM_ERROR_INFO  {

    CHAR * pszStatus;
    USHORT uHttpSubError;
    BOOL fAsync;

} HSE_CUSTOM_ERROR_INFO, * LPHSE_CUSTOM_ERROR_INFO;



#define HSE_VECTOR_ELEMENT_TYPE_MEMORY_BUFFER       0
#define HSE_VECTOR_ELEMENT_TYPE_FILE_HANDLE         1



typedef struct _HSE_VECTOR_ELEMENT
{
    DWORD ElementType;  

    PVOID pvContext;    

    ULONGLONG cbOffset; 

    ULONGLONG cbSize;   
} HSE_VECTOR_ELEMENT, *LPHSE_VECTOR_ELEMENT;



typedef struct _HSE_RESPONSE_VECTOR
{
    DWORD dwFlags;                         

    LPSTR pszStatus;                       
    LPSTR pszHeaders;                      

    DWORD nElementCount;                   
    LPHSE_VECTOR_ELEMENT lpElementArray;   
} HSE_RESPONSE_VECTOR, *LPHSE_RESPONSE_VECTOR;


typedef HRESULT
  (WINAPI * PFN_HSE_CACHE_INVALIDATION_CALLBACK)(
        WCHAR *pszUrl);


#if(_WIN32_WINNT >= 0x400)
#include "wincrypt.h"


typedef struct _CERT_CONTEXT_EX {
    CERT_CONTEXT    CertContext;
    DWORD           cbAllocated;
    DWORD           dwCertificateFlags;
} CERT_CONTEXT_EX;
#endif



typedef struct _HSE_TRACE_INFO
{
    BOOL        fTraceRequest;
    BYTE        TraceContextId[16];
    DWORD       dwReserved1;
    DWORD       dwReserved2;
} HSE_TRACE_INFO, *LPHSE_TRACE_INFO;



#define HSE_APP_FLAG_IN_PROCESS   0
#define HSE_APP_FLAG_ISOLATED_OOP 1
#define HSE_APP_FLAG_POOLED_OOP   2



DLL_PUBLIC BOOL  WINAPI   GetExtensionVersion( HSE_VERSION_INFO  *pVer );
DLL_PUBLIC DWORD WINAPI   HttpExtensionProc(EXTENSION_CONTROL_BLOCK *pECB );
DLL_PUBLIC BOOL  WINAPI   TerminateExtension( DWORD dwFlags );
#ifndef _WIN32
DLL_PUBLIC BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
#endif

typedef BOOL
    (WINAPI * PFN_GETEXTENSIONVERSION)( HSE_VERSION_INFO  *pVer );

typedef DWORD 
    (WINAPI * PFN_HTTPEXTENSIONPROC )( EXTENSION_CONTROL_BLOCK * pECB );

typedef BOOL  (WINAPI * PFN_TERMINATEEXTENSION )( DWORD dwFlags );
#ifdef __cplusplus
 }
#endif
#endif 
