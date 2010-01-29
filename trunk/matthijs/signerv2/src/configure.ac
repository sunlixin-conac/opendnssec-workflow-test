#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([signer engine], [2.0], [http://trac.opendnssec.org/newticket])
AC_CONFIG_SRCDIR([ods-signerd.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

sinclude(m4/acx_ldns.m4)
sinclude(m4/acx_libxml2.m4)
sinclude(m4/acx_pthread.m4)
# sinclude(m4/acx_rt.m4)

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

# Checks for libraries.
# check this first, so that the pthread lib does not get linked in via
# lib*, and thus distorts the tests, and we end up using the non-threadsafe
# C libraries.
AC_ARG_WITH(pthreads, AC_HELP_STRING([--with-pthreads],
 [use pthreads library, or --without-pthreads to disable threading support.]),
 [ ],[ withval="yes" ])
se_have_pthreads=no
if test x_$withval != x_no; then
    ACX_PTHREAD([
        AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.])
        LIBS="$PTHREAD_LIBS $LIBS"
        CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
        CC="$PTHREAD_CC"
        se_have_pthreads=yes
        AC_CHECK_TYPES([pthread_spinlock_t, pthread_rwlock_t],,,[#include <pthread.h>])
        ])
fi

ACX_LDNS
ACX_LIBXML2
# ACX_LIBRT

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h strings.h sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_GETGROUPS
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([bzero clock_gettime endgrent endpwent mkdir select socket strchr strdup strerror strncasecmp strrchr tzset])

dnl
dnl Features
dnl

AC_ARG_ENABLE(tools, AC_HELP_STRING([--disable-tools], [Disables signer tools]))
case "$enable_tools" in
        no)
                ;;
        yes|*)
        AC_DEFINE_UNQUOTED([USE_TOOLS], [1], [Define this to enable signer tools.])
        ;;
esac

AC_ARG_WITH(libhsm, AC_HELP_STRING([--with-libhsm=PATH],
        [specify prefix of path of hsm library to use]),
        [ speciallhsmdir="$withval"
        if test "$withval" != ""; then
                CPPFLAGS="-I$withval/include $CPPFLAGS"
                LDFLAGS="-L$withval -L$withval/lib $LDFLAGS"
        fi
        lhsmdir="$withval"
])

AH_BOTTOM([

#define MAX_LINE 1024
#define SETEUID_BREAKS_SETUID 1
#define SE_CONFFILE "/opt/opendnssec/etc/opendnssec/conf.xml"
#define SE_PIDFILE "/opt/opendnssec/var/run/opendnssec/signerengine.pid"
#define SE_CLISOCKFILE "/opt/opendnssec/var/run/opendnssec/signerengine.sock"
#define SE_WORKINGDIR "/opt/opendnssec/var/opendnssec/"
#define SE_WORKERTHREADS 4

#define RNG_CONFFILE "/opt/opendnssec/share/opendnssec/conf.rng"
#define RNG_ZONELISTFILE "/opt/opendnssec/share/opendnssec/zonelist.rng"
#define RNG_SIGNCONFFILE "/opt/opendnssec/share/opendnssec/signconf.rng"

#define SHUTDOWN_RESPONSE "Engine shut down."
])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
