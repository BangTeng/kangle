/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 *
 * kangle web server              http://www.kangleweb.net/
 * ---------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111, USA.
 *  See COPYING file for detail.
 *
 *  Author: KangHongjiu <keengo99@gmail.com>
 */
#ifndef GLOBAL_H_asdfkjl23kj4234
#define GLOBAL_H_asdfkjl23kj4234
#ifndef _WIN32
#include "config.h"
#endif
#ifndef _WIN32
#define INT64  long long
#else
#define HAVE_SOCKLEN_T 1
#define ENABLE_DETECT_WORKER_LOCK    1
#endif

#ifndef PROGRAM_NAME
#ifdef HTTP_PROXY
#define PROGRAM_NAME    "kangle-proxy"
#else
#define PROGRAM_NAME     "kangle"
#endif
#endif
#ifndef VERSION
#define VERSION         "3.5.13.2"
#endif
#define VER_ID   VERSION
#ifndef MAX
#define MAX(a,b)  ((a)>(b)?(a):(b))
#endif
#ifndef MIN
#define MIN(a,b)  ((a)>(b)?(b):(a))
#endif
#define	 GC_SLEEP_TIME	5

#define  NBUFF_SIZE     8192

#define  JUMP_ALLOW     0
#define  JUMP_DENY      1
#define  JUMP_TABLE     2
#define  JUMP_SERVER    3
#define  JUMP_WBACK     4
#define  JUMP_VHS       5
#define  JUMP_FCGI      6
#define  JUMP_CONTINUE  7
#define  JUMP_PROXY     8
#define  RESPONSE_TABLE 9
#define  JUMP_CGI       10
#define  JUMP_API       11
#define  JUMP_RETURN    12
#define  JUMP_CMD       13
#define  JUMP_MSERVER   14
#define  JUMP_DROP      16
#define  JUMP_FINISHED  17
#define  JUMP_UNKNOW    100

#define  REQUEST          0
#define  RESPONSE         1
#define  REQUEST_RESPONSE 2
/***********************************/
#define WORK_MODEL_KA        (1)
#define WORK_MODEL_MANAGE    (1<<1)
#define WORK_MODEL_SSL       (1<<2)
#define WORK_MODEL_INTERNAL  (1<<3)
#define WORK_MODEL_REPLACE   (1<<5)
#define WORK_MODEL_TPROXY    (1<<7)

#define WORK_MODEL_SIMULATE  (1<<9)
#define WORK_MODEL_UNIX_SOCKET (1<<11)
#define WORK_MODEL_SKIP_ACCESS (1<<12)
/************************************/

#ifdef _WIN32
#define HAVE_SSTREAM 1
#define _LARGE_FILE  1
#endif


#define ENABLE_WRITE_BACK  1
#define ENABLE_LIMIT_SPEED 1
#define CONTENT_FILTER  1

#ifndef NDEBUG
#if !defined(_WIN32) && !defined(OPENBSD)
//#define MALLOCDEBUG   1
#endif
#if !defined(OPENBSD)
//#define DEAD_LOCK   1
#endif
#endif
#define PID_FILE   "/kangle.pid"

#include <assert.h>

#ifndef _WIN32
#include "environment.h"
#else
#pragma warning(disable: 4290 4996)
#endif
#define forever() for(;;)

#define IF_FREE(p) {if ( p ) xfree(p);p=NULL;}
#define IF_STRDUP(d,s) {if ( d ) xfree(d); d = xstrdup(s);}

#if !defined(ABS)
#define ABS(x)  ((x)>0?(x):(-(x)))
#endif
#define ROUND(x,b) ((x)%(b)?(((x)/(b)+1)*(b)):(x))

#define CHUNK_SIZE (512)

#define ROUND_CHUNKS(s) ((((s) / CHUNK_SIZE) + 1) * CHUNK_SIZE)
#define KGL_SSL_PARAM_SPLIT_CHAR  ';'
#define NET_HASH_SIZE (1024)
#define NET_HASH_MASK (NET_HASH_SIZE-1)
#define ENABLE_VH_RS_LIMIT         1
#define ENABLE_VH_FLOW             1
#define ENABLE_ATOM                1

#ifdef  KSOCKET_SSL
#define ENABLE_UPSTREAM_SSL        1
#endif
///////////////////////////////////////////////////////////////////
/**
* obj->index.flags
*/
#define FLAG_DEAD               1      /* */
#define FLAG_URL_FREE           (1<<1) /* */
#define FLAG_IN_MEM             (1<<2) /* */
#define FLAG_IN_DISK            (1<<3) 
//#define OBJ_GZIPED              (1<<4) /* gzip*/
#define FLAG_NO_BODY            (1<<5) /* body */
#define OBJ_MUST_REVALIDATE     (1<<6) /* must-revalidate */
#define OBJ_IS_STATIC2          (1<<7) /* */
#define OBJ_IS_READY            (1<<8) /* ׼ */
//////////////////////////////////////////////////////////////////
#define ANSW_HAS_EXPIRES        (1<<9) /* */
#define ANSW_NO_CACHE           (1<<10) /**/
#define ANSW_HAS_MAX_AGE        (1<<11) /**/
#define ANSW_LAST_MODIFIED      (1<<12) /* */
#define ANSW_HAS_CONTENT_LENGTH (1<<13) 
//#define OBJ_DEFLATED            (1<<14) 
#define ANSW_HAS_CONTENT_RANGE  (1<<15) 
#define ANSW_CHUNKED            (1<<16) 
/////////////////////////////////////////////////////////////////////
#define FLAG_RQ_INTERNAL        (1<<17) 
/////////////////////////////////////////////////////////////////////
#define FLAG_NO_DISK_CACHE      (1<<19) /*  */
#define FLAG_NO_NEED_CACHE      ANSW_NO_CACHE 
//#define FLAG_NEED_GZIP          (1<<20)  /*  */
#define OBJ_IS_GUEST            (1<<21)  /*  */
/////////////////////////////////////////////////////////////////////
//#define OBJ_IS_DELTA            (1<<22)
#define OBJ_HAS_ETAG            (1<<23)
/////////////////////////////////////////////////////////////////////
#define OBJ_CACHE_RESPONSE      (1<<24) 
#define ANSW_LOCAL_SERVER       (1<<25) 
#define ANSW_XSENDFILE          (1<<26) /* x-accel-redirect */
/////////////////////////////////////////////////////////////////////
#define FLAG_BIG_OBJECT_PROGRESS (1<<27)
#define FLAG_BIG_OBJECT          (1<<28)
#define OBJ_INDEX_SAVED          (1<<29)
#define OBJ_INDEX_UPDATE         (1<<30)
#define OBJ_NOT_OK               (1<<31)

#define STATUS_OK               200
#define STATUS_CREATED          201
#define STATUS_NO_CONTENT       204
#define STATUS_CONTENT_PARTIAL  206
#define STATUS_MULTI_STATUS     207
#define STATUS_MOVED            301
#define STATUS_FOUND            302
#define STATUS_NOT_MODIFIED     304
#define STATUS_TEMPORARY_REDIRECT 307
#define STATUS_BAD_REQUEST      400
#define STATUS_UNAUTH           401
#define STATUS_FORBIDEN         403
#define STATUS_NOT_FOUND        404
#define STATUS_METH_NOT_ALLOWED 405
#define STATUS_PROXY_UNAUTH     407
#define STATUS_CONFLICT         409
#define STATUS_PRECONDITION     412
#define STATUS_SERVER_ERROR     500
#define STATUS_NOT_IMPLEMENT    501
#define STATUS_BAD_GATEWAY      502
#define STATUS_SERVICE_UNAVAILABLE  503
#define STATUS_GATEWAY_TIMEOUT  504



/**
* rq->flags
*/
#define RQ_QUEUED              1
#define RQ_HAS_IF_MOD_SINCE    (1<<1)
#define RQ_HAS_IF_NONE_MATCH   (1<<2)
#define RQ_HAS_NO_CACHE        (1<<3)
#define RQ_BIG_OBJECT_CTX      (1<<5)
#define RQ_INPUT_CHUNKED       (1<<6)
#define RQ_SYNC                (1<<7)
#define RQ_HAS_ONLY_IF_CACHED  (1<<8)
#define RQ_HAS_AUTHORIZATION   (1<<9)
#define RQ_HAS_PROXY_AUTHORIZATION (1<<10)
#define RQ_HAS_KEEP_CONNECTION (1<<11)
#define RQ_IF_RANGE_DATE       (1<<14)
#define RQ_IF_RANGE_ETAG       (1<<15)
#define RQ_HAS_CONTENT_LEN     (1<<16)
#define RQ_OBJ_VERIFIED        (1<<17)
#define RQ_HAVE_RANGE          (1<<18)
#define RQ_TE_CHUNKED          (1<<19)
#define RQ_TE_GZIP             (1<<20)
#define RQ_HAS_SEND_HEADER     (1<<21)
#define RQ_HAS_CONNECTION_UPGRADE  (1<<22)
#define RQ_POST_UPLOAD         (1<<23)
#define RQ_CONNECTION_CLOSE    (1<<24)
#define RQ_OBJ_STORED          (1<<25)
#define RQ_HAVE_EXPECT         (1<<26)
#define RQ_TEMPFILE_HANDLED    (1<<28)
#define RQ_IS_ERROR_PAGE       (1<<30)
#define RQ_UPSTREAM_ERROR      (1<<31)



///////////////////////////////////////////////
//rq->filter_flag
///////////////////////////////////////////////
#define  RF_TPROXY_UPSTREAM  (1)
#ifdef ENABLE_TPROXY
#define  RF_TPROXY_TRUST_DNS (1<<1)
#endif
#define  RF_PROXY_RAW_URL    (1<<2) /*raw_url */
#define  RF_DOUBLE_CACHE_EXPIRE (1<<3)
#define  RF_UPSTREAM_NOKA    (1<<4)
#define  RF_ALWAYS_ONLINE    (1<<5)
#define  RF_GUEST            (1<<6)
#define  RQ_NO_EXTEND        (1<<7)
#define  RF_X_REAL_IP        (1<<8)
#define  RF_NO_X_FORWARDED_FOR  (1<<9)
#define  RF_LOG_DRILL        (1<<10)
#define  RF_NO_CACHE         (1<<12)
#define  RF_VIA              (1<<13)
#define  RF_X_CACHE          (1<<14)
#define  RF_NO_BUFFER        (1<<15)
#define  RF_NO_DISK_CACHE    (1<<16)
#define  RF_UPSTREAM_NOSNI   (1<<17)
#define  RQ_SEND_AUTH        (1<<18)
#define  RF_PROXY_FULL_URL   (1<<19)
#define  RF_FOLLOWLINK_ALL   (1<<21)
#define  RF_FOLLOWLINK_OWN   (1<<22)
#define  RF_NO_X_SENDFILE    (1<<23)
#define  RQ_URL_QS           (1<<25)
#define  RQ_FULL_PATH_INFO   (1<<26)
#define  RF_AGE              (1<<27)
#define  RQ_SWAP_OLD_OBJ     (1<<28)
#define  RQ_RESPONSE_DENY    (1<<29)
#define  RQF_CC_PASS         (1<<30)
#define  RQF_CC_HIT          (1<<31)


#define KGL_LIST_CONNECT          0
#define KGL_LIST_RW               1
#define KGL_LIST_SYNC             2
#define KGL_LIST_READY            3
#define KGL_LIST_BLOCK            4
#define KGL_LIST_NONE             5


#define ANSW_SIZE  (4*1024)
#define SET(a,b)   ((a)|=(b))
#define CLR(a,b)   ((a)&=~(b))
#define TEST(a,b)  ((a)&(b))

#define        IS_SPACE(a)     isspace((unsigned char)a)
#define        IS_DIGIT(a)     isdigit((unsigned char)a)
#define  CONNECT_TIME_OUT    20
#if defined(FREEBSD) || defined(NETBSD) || defined(OPENBSD)
#define BSD_OS 1
#endif
#define PROGRAM_NO_QUIT               0
#define PROGRAM_QUIT_IMMEDIATE        1
#define PROGRAM_QUIT_SHUTDOWN         2
#ifdef _WIN32
#define WHM_MODULE                 1
#endif
#define ENABLE_STAT_STUB           1
#define ENABLE_MULTI_TABLE         1
#define ENABLE_DIGEST_AUTH         1
#define ENABLE_MULTI_SERVER        1
#define ENABLE_SIMULATE_HTTP       1
#ifdef ENABLE_DISK_CACHE
	#define ENABLE_DB_DISK_INDEX       1
	#define ENABLE_SQLITE_DISK_INDEX   1
#endif
#ifndef HTTP_PROXY
	#define WHM_MODULE                 1
	#define ENABLE_VH_QUEUE            1
	#define ENABLE_TF_EXCHANGE         1
	#define ENABLE_SUBDIR_PROXY        1
#endif
#define ENABLE_INPUT_FILTER        1
#define ENABLE_FORCE_CACHE         1
#define ENABLE_REQUEST_QUEUE       1
#define ENABLE_USER_ACCESS         1
#define ENABLE_VH_RUN_AS           1
#define ENABLE_MANY_VH             1
#ifdef ENABLE_VH_RS_LIMIT
	#define ENABLE_VH_LOG_FILE         1
#endif
#define ENABLE_SUB_VIRTUALHOST     1
#define ENABLE_BASED_PORT_VH       1
#define ENABLE_LOG_DRILL           1
#define ENABLE_LOCAL_ERROR         1
//#define ENABLE_PIPE_LOG            1
#if defined(ENABLE_FORCE_CACHE) || defined(ENABLE_STATIC_URL)
	#define ENABLE_STATIC_ENGINE       1
#endif
#define DEFAULT_COOKIE_STICK_NAME  "kangle_runat"
#define X_REAL_IP_SIGN             "x-real-ip-sign"
#define VARY_URL_KEY               1
enum Proto_t {
	Proto_http, Proto_fcgi, Proto_ajp,Proto_uwsgi,Proto_scgi,Proto_hmux,Proto_spdy,Proto_tcp
};
/**
* 
*/
#define	HASH_SIZE	(1024)
#define	HASH_MASK	(HASH_SIZE-1)
#define MULTI_HASH      1
#ifdef  LINUX
	#define ENABLE_SENDFILE      1
#endif

//define likely and unlikely
#if defined(__GNUC__) && (__GNUC__ > 2)
	# define likely(x)   __builtin_expect((x),1)
	# define unlikely(x) __builtin_expect((x),0)
#else
	# define likely(x)   (x)
	# define unlikely(x) (x)
#endif
//end define likely and unlikely
#ifndef DISABLE_KSAPI_FILTER
	//enable ksapi filter
	#define ENABLE_KSAPI_FILTER 1
#endif //DISABLE_KSAPI_FILTER
//for test
//#define ENABLE_KSSL_BIO            1
#endif //global.h
