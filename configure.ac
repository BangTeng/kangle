# Process this file with autoconf to produce a configure script.
AC_INIT([kangle],[3.5.13.2])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE
AM_CONFIG_HEADER([src/config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_MAKE_SET

# Checks for libraries.

# Checks for header files.
test "$prefix" = "NONE" && prefix=/usr/local
#etc dir
AH_TEMPLATE([KANGLE_ETC_DIR],[])
if test ! "${sysconfdir}" = "\${prefix}/etc" ; then
	SYSCONFDIR=`eval echo ${sysconfdir}`
	AC_DEFINE_UNQUOTED([KANGLE_ETC_DIR],["$SYSCONFDIR"])
fi
#tmp dir
AH_TEMPLATE([KANGLE_TMP_DIR],[])
DEFAULT_TMP_DIR="$prefix/tmp"
AC_ARG_WITH(tmpdir,
  AS_HELP_STRING([--with-tmpdir=PATH],
    [Default location for kangle tmp. default: PREFIX/tmp]), [
case $withval in
  yes|no)
	AC_MSG_ERROR( --with-tmpdir requires a directory PATH. --with-tmpdir=PATH )
    ;;
  *)
	DEFAULT_TMP_DIR="$withval"
	AC_DEFINE_UNQUOTED([KANGLE_TMP_DIR],["$DEFAULT_TMP_DIR"])
    ;;
esac
 ]
)
AC_SUBST(DEFAULT_TMP_DIR)

#ext dir
AH_TEMPLATE([KANGLE_EXT_DIR],[])
DEFAULT_EXT_DIR="$prefix/ext"
AC_ARG_WITH(extdir,
  AS_HELP_STRING([--with-extdir=PATH],
    [Default dir for kangle scan config files. default: PREFIX/ext]), [
case $withval in
  yes|no)
    AC_MSG_ERROR( --with-extdir requires a directory PATH. --with-extdir=PATH )
    ;;
  *)
    DEFAULT_EXT_DIR="$withval"
	AC_DEFINE_UNQUOTED([KANGLE_EXT_DIR],["$DEFAULT_EXT_DIR"])
    ;;
esac
 ]
)
AC_SUBST(DEFAULT_EXT_DIR)
#var dir
AH_TEMPLATE([KANGLE_VAR_DIR],[])
DEFAULT_VAR_DIR="$prefix/var"
AC_ARG_WITH(vardir,
  AS_HELP_STRING([--with-vardir=PATH],
    [Default location for kangle var(logs and pid file). default: PREFIX/var]), [
case $withval in
  yes|no)
	AC_MSG_ERROR( --with-vardir requires a directory PATH. --with-vardir=PATH )
    ;;
  *)
	DEFAULT_VAR_DIR="$withval"
	AC_DEFINE_UNQUOTED([KANGLE_VAR_DIR],["$DEFAULT_VAR_DIR"])
    ;;
esac
 ]
)
AC_SUBST(DEFAULT_VAR_DIR)
#webadmin dir
AH_TEMPLATE([KANGLE_WEBADMIN_DIR],[])
DEFAULT_WEBADMIN_DIR="$prefix/webadmin"
AC_ARG_WITH(webadmindir,
  AS_HELP_STRING([--with-webadmindir=PATH],
    [Default location for kangle webadmin. default: PREFIX/webadmin]), [
case $withval in
  yes|no)
	AC_MSG_ERROR( --with-webadmindir requires a directory PATH. --with-webadmindir=PATH )
    ;;
  *)
	DEFAULT_WEBADMIN_DIR="$withval"
	AC_DEFINE_UNQUOTED([KANGLE_WEBADMIN_DIR],["$DEFAULT_WEBADMIN_DIR"])
    ;;
esac
 ]
)
AC_SUBST(DEFAULT_WEBADMIN_DIR)



AC_LANG(C++) 
AC_C_INLINE

AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h locale.h netdb.h netinet/in.h stdlib.h string.h strings.h sys/file.h sys/ioctl.h sys/socket.h sys/time.h syslog.h unistd.h zlib.h sys/epoll.h port.h])
AH_TEMPLATE([HAVE_SOCKLEN_T],[define if it have socklen_t])
AH_TEMPLATE([HAVE_GETADDRINFO_A],[define if it have getaddrinfo_a])
AH_TEMPLATE([HAVE_GMTOFF],[])
AH_TEMPLATE([MALLOCDEBUG],[malloc debug])
AH_TEMPLATE([DEAD_LOCK],[dead lock debug])
AH_TEMPLATE([HAVE___GMTOFF],[])
AH_TEMPLATE([CTIME_R_3],[define ctime_r])
AH_TEMPLATE([KSOCKET_IPV6],[enable ipv6 support])
AH_TEMPLATE([KSOCKET_SSL],[enable ssl support])
AH_TEMPLATE([ENABLE_HTTP2],[enable http2 support])
AH_TEMPLATE([CONTENT_FILTER],[enable content filter])
AH_TEMPLATE([ENABLE_LARGE_FILE],[large files support])
AH_TEMPLATE([ENABLE_VH_RS_LIMIT],[virtualhost resource limit])
AH_TEMPLATE([ENABLE_DISK_CACHE],[enable disk cache support])
AH_TEMPLATE([ENABLE_TCMALLOC],[tcmalloc])
AH_TEMPLATE([ENABLE_JEMALLOC],[jemalloc])

#AH_TEMPLATE([ONLY_HTTP],[only compiled http proxy])
#AC_CHECK_HEADER([dlfcn.h],
#                [AC_DEFINE([HAVE_DLFCN_H])\
#		 LIBS="$LIBS -ldl"],
#                [])
AC_CHECK_HEADERS([sstream])
# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM


AC_CHECK_TYPE(u_int8_t,unsigned char)
AC_CHECK_TYPE(u_int16_t,unsigned short)
AC_CHECK_TYPE(u_int32_t,unsigned int)
AC_MSG_CHECKING(for ctime_r have len)
AC_TRY_COMPILE([
#include <time.h>
],[
     const time_t *timep;
     char *buf;
     ctime_r(timep,buf,3);
],[
AC_DEFINE(CTIME_R_3,1) AC_MSG_RESULT(yes)
],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for getaddrinfo_a)
AC_TRY_COMPILE([
#define _GNU_SOURCE
#include <netdb.h>
],[
getaddrinfo_a(0,0,0,0);
],[
AC_DEFINE(HAVE_GETADDRINFO_A,1) AC_MSG_RESULT(yes)
],[AC_MSG_RESULT(no)])


AC_MSG_CHECKING(for socklen_t)
AC_TRY_COMPILE([
#include        <stdio.h>
#include        <stdlib.h>
#include        <signal.h>
#include        <sys/types.h>
#include        <sys/socket.h>
#include        <netinet/in.h>
],[
socklen_t       test;],[
AC_DEFINE(HAVE_SOCKLEN_T,1) AC_MSG_RESULT(yes)
],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for tm_gmtoff)
AC_TRY_COMPILE([
#include        <stdio.h>
#include        <stdlib.h>
#include        <time.h>
],[
struct  tm      tm;
tm.tm_gmtoff = tm.tm_gmtoff*2;
],[
AC_DEFINE(HAVE_GMTOFF) AC_MSG_RESULT(yes)
],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING(for __tm_gmtoff)
AC_TRY_COMPILE([
#include        <stdio.h>
#include        <stdlib.h>
#include        <time.h>
],[
struct  tm      tm;
tm.__tm_gmtoff = tm.__tm_gmtoff*2;
],[
AC_DEFINE(HAVE___GMTOFF) AC_MSG_RESULT(yes)
],[AC_MSG_RESULT(no)])

CXXFLAGS="-O2 -g -DNDEBUG"
# Checks for library functions.
AC_ARG_ENABLE([ipv6],AC_HELP_STRING([--enable-ipv6],[enable ipv6 support.]),[AC_DEFINE(KSOCKET_IPV6,1)],[])
AC_ARG_ENABLE([disk-cache],AC_HELP_STRING([--enable-disk-cache],[Compile with disk cache support.]),
[
	AC_DEFINE(ENABLE_DISK_CACHE,1)
	LIBS="$LIBS -lsqlite3"
])
AC_ARG_ENABLE([vh-limit],AC_HELP_STRING([--enable-vh-limit],[Enable virtualhost resource limit and logs.]),[AC_DEFINE(ENABLE_VH_RS_LIMIT,1)],[])
AC_ARG_ENABLE([large-files],AC_HELP_STRING([--enable-large-files],[Enable support for large files (logs etc).]),
[
	CXXFLAGS="$CXXFLAGS -D_LARGE_FILE -D_FILE_OFFSET_BITS=64 -D__USE_FILE_OFFSET64"
])
AC_ARG_ENABLE([debug],AC_HELP_STRING([--enable-debug],[Enable debug compile.]),
[
	CXXFLAGS="-O0 -g -Wall"
])
AC_ARG_ENABLE([malloc-debug],AC_HELP_STRING([--enable-malloc-debug],[Enable malloc debug.]),
[
	AC_DEFINE(MALLOCDEBUG,1)
])
AC_ARG_ENABLE([dead-lock],AC_HELP_STRING([--enable-dead-lock],[Enable dead lock detected.]),
[
	AC_DEFINE(MALLOCDEBUG,1)
	AC_DEFINE(DEAD_LOCK,1)
])
AC_ARG_ENABLE([ssl],AC_HELP_STRING([--enable-ssl],[Enable ssl support]),
[
	AC_DEFINE(KSOCKET_SSL,1)
	LIBS="$LIBS -lssl -lcrypto"
])
AC_ARG_ENABLE([http2],AC_HELP_STRING([--enable-http2],[Enable http2 support]),
[
	AC_DEFINE(ENABLE_HTTP2,1)
])
#AC_FUNC_FORK
#AC_PROG_GCC_TRADITIONAL
#AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_MKTIME
#AC_TYPE_SIGNAL
AC_FUNC_STRERROR_R
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([gethostbyname_r gettimeofday inet_ntoa localtime_r memchr memset select socket strcasecmp strchr strdup strncasecmp strrchr strstr strtol ctime_r daemon localtime_r gmtime_r poll timegm accept4 writev])
AC_CHECK_LIB(iconv,iconv_open,[
        LIBS="$LIBS -liconv"
],[])

AC_ARG_WITH([jemalloc],
 [AS_HELP_STRING([--with-jemalloc=dir],
                 [compile program use jemalloc.])],
 [
	LIBS="$LIBS -ljemalloc -lrt -L$withval/lib/"
        CXXFLAGS="$CXXFLAGS -I$withval/include/"
	AC_DEFINE(ENABLE_JEMALLOC,1)
],[])

AC_ARG_WITH([tcmalloc],
 [AS_HELP_STRING([--with-tcmalloc=dir],
                 [compile program use tcmalloc.])],
 [
        LIBS="$LIBS -ltcmalloc -L$withval/lib/"
        AC_DEFINE(ENABLE_TCMALLOC,1)
],[])

#LIBS="$LIBS `mysql_config --libs_r`"
#CXXFLAGS="$CXXFLAGS `mysql_config --include`"
LIBS="$EXTLIBS $LIBS \$(EXTLIBS) -lpthread `pcre-config --libs` -lz" 
CXXFLAGS="$CXXFLAGS `pcre-config --cflags`"
case "$host_os" in
linux*)
	CXXFLAGS="$CXXFLAGS -D_REENTRANT -DLINUX"
	LIBS="$LIBS -ldl -laio"
	# LDFLAGS="$LDFLAGS -Xlinker --hash-style=both"
	;;
netbsd*)
	CXXFLAGS="$CXXFLAGS -DNETBSD"
        LIBS="$LIBS `pcre-config --libs` $DEBUG_LIBS"
	;;
openbsd*)
	CXXFLAGS="$CXXFLAGS -DOPENBSD"
        LIBS="$LIBS `pcre-config --libs` -liconv $DEBUG_LIBS"
	;;
freebsd*) 
	LIBS="$LIBS -liconv `pcre-config --libs` $DEBUG_LIBS"
	CXXFLAGS="$CXXFLAGS -DFREEBSD `pcre-config --cflags`"
        if test "X$LIBGB_NAME" != "X" ; then
              LIBGB_NAME="$LIBGB_NAME -lstdc++"
        fi
        ;;
solaris*) 
	CXXFLAGS="$CXXFLAGS -D_REENTRANT -DSOLARIS"
        LIBS="$LIBS -lsocket -lnsl"
        if test "$large_files" = "yes" ; then
        	CFLAGS="$CFLAGS -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -DWITH_LARGE_FILES"
        fi
        ;;

hpux*)
	 CXXFLAGS="$CXXFLAGS -D_REENTRANT -DHPUX"
	;;
esac
CXXFLAGS="$CXXFLAGS -D_LARGE_FILE -D_FILE_OFFSET_BITS=64 -D__USE_FILE_OFFSET64"
AC_OUTPUT([
	Makefile
	src/Makefile
	extworker/Makefile
	module/webdav/Makefile
	])
