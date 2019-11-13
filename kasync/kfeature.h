#ifndef KGLOBAL_H_99
#define KGLOBAL_H_99
#ifndef _WIN32
#include "config.h"
#define INT64  long long
#define KSOCKET_UNIX
#else
#define HAVE_SOCKLEN_T 1
#pragma warning(disable: 4290 4996)
#endif
#ifdef  __cplusplus
	#define KBEGIN_DECLS  extern "C" {
	#define KEND_DECLS    }
	#define	INLINE	inline
#else
	#define KBEGIN_DECLS
	#define KEND_DECLS
	#ifdef _WIN32
	#define INLINE __forceinline
	#else
	#define INLINE	inline __attribute__((always_inline))
	#endif
#endif

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef char				int8_t;
typedef short				int16_t;
typedef int					int32_t;
typedef __int64				int64_t;
typedef unsigned char		uint8_t;
typedef unsigned short		uint16_t;
typedef unsigned long		uint32_t;
typedef unsigned __int64    uint64_t;
#define bool				uint8_t
#define true				1
#define false				0
#else
#include <stdint.h>
#include <stdbool.h>
#endif

#if defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD) || defined(DARWIN)
#define BSD_OS 1
#endif

#ifndef NDEBUG
#if LINUX || _WIN32
//#define MALLOCDEBUG	1
#endif
#endif
//#define KSOCKET_SSL     1
#define KSOCKET_IPV6	1
#define ENABLE_PROXY_PROTOCOL      1

#ifdef KSOCKET_SSL
#ifdef _WIN32
#define ENABLE_KSSL_BIO 1
#endif
#endif


#define SET(a,b)   ((a)|=(b))
#define CLR(a,b)   ((a)&=~(b))
#define TEST(a,b)  ((a)&(b))
#define SAFE_STRCPY(s,s1)   do {memset(s,0,sizeof(s));strncpy(s,s1,sizeof(s)-1);}while(0)
#ifndef MAX
#define MAX(a,b)  ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b)  ((a)>(b)?(b):(a))
#endif
#define        IS_SPACE(a)     isspace((unsigned char)a)
#define        IS_DIGIT(a)     isdigit((unsigned char)a)
#if defined(__GNUC__) && (__GNUC__ > 2)
# define likely(x)   __builtin_expect((x),1)
# define unlikely(x) __builtin_expect((x),0)
#else
# define likely(x)   (x)
# define unlikely(x) (x)
#endif
typedef enum {
	kev_ok, //selectable in selector event(read/write/next/connect) or timer or by user blocked.
	kev_err,//selectable not in any selector event/timer and not destroy
	kev_destroy//selectable not in any selector event/timer and destroied by result callback
} kev_result;

#define KEV_HANDLED(x) (x!=kev_err)
#define KEV_AVAILABLE(x) (x!=kev_destroy)

#endif
