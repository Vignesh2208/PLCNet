# CHECK_PROG_GNU_MAKE - determine whether make is GNU make
AC_DEFUN([CHECK_PROG_GNU_MAKE],
[AC_PREREQ(2.57)dnl
AC_MSG_CHECKING(whether ${MAKE-make} is GNU make)
set dummy ${MAKE-make};
AC_CACHE_VAL(ac_cv_prog_gnu_make,
[cat > conftestmake <<\EOF
TEST=1
OK=bogus
ifeq ($(TEST),1)
OK=is-gnu-make
endif

all:
	@echo ${OK}
EOF
changequote(, )dnl
# GNU make sometimes prints "make[1]: Entering...", which would confuse us.
ac_maketemp=`${MAKE-make} -f conftestmake 2>/dev/null | grep is-gnu-make`
changequote([, ])dnl
if test -n "$ac_maketemp"; then
  ac_cv_prog_gnu_make=yes
else
  ac_cv_prog_gnu_make=no
fi
rm -f conftestmake])dnl
if test "$ac_cv_prog_gnu_make" = "yes"; then
  AC_MSG_RESULT(yes)
  HAVE_GNU_MAKE=yes
else
  AC_MSG_RESULT(no)
  HAVE_GNU_MAKE=no
fi
AC_SUBST([HAVE_GNU_MAKE])dnl
])

dnl Link a function with #include <pthread.h> first
dnl CHECK_PTHREAD_FUNC(FUNCTION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
AC_DEFUN([CHECK_PTHREAD_FUNC],
[AC_PREREQ(2.57)dnl
AC_MSG_CHECKING([for $1 with LIBS="$LIBS"])
AC_CACHE_VAL(acx_cv_func_$1,
[AC_TRY_LINK([
#include <assert.h>
#include <pthread.h>
], [
$2;
], eval "ac_cv_func_$1=yes", eval "ac_cv_func_$1=no")])
if eval "test \"`echo '$ac_cv_func_'$1`\" = yes"; then
  AC_MSG_RESULT(yes)
  ifelse([$3], , :, [$3])
else
  AC_MSG_RESULT(no)
  ifelse([$4], , , [$4])dnl
fi
])dnl

dnl auxiliary function for checking pthreads
AC_DEFUN([PTHREAD_CHECK_LIB],
[AC_PREREQ(2.57)dnl
pthread_save_LIBS="$LIBS"
pthread_save_CFLAGS="$CFLAGS"
LIBS="$LIBS $1"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

pthread_ok=yes
CHECK_PTHREAD_FUNC(pthread_create, [pthread_create(0, 0, 0, 0)], , pthread_ok=no)
CHECK_PTHREAD_FUNC(pthread_join, [pthread_join(0, 0)], , pthread_ok=no)

LIBS="$pthread_save_LIBS"
CFLAGS="$pthread_save_CFLAGS"
if test "$pthread_ok" = "yes"; then
 	PTHREAD_LIBS="$1"
fi
])dnl

dnl determine whether pthreads work
AC_DEFUN([CHECK_PTHREAD],
[AC_PREREQ(2.57)dnl
pthread_ok=no

dnl First, check if the POSIX threads header, pthread.h, is available.
dnl If it isn't, don't bother looking for the threads libraries.
AC_CHECK_HEADER(pthread.h, , pthread_ok=noheader)
PTHREAD_CFLAGS=""

dnl More AIX/DEC lossage: must compile with -D_THREAD_SAFE
dnl (also on FreeBSD) or -D_REENTRANT: (cc_r subsumes this on AIX, 
dnl but it doesn't hurt to -D as well, esp. if cc_r is not available.)
AC_MSG_CHECKING([if more special flags are required for pthreads])
ok=no
AC_REQUIRE([AC_CANONICAL_HOST])
case "${host_cpu}-${host_os}" in
    *-freebsd*)
	ok="-pthread -D_THREAD_SAFE"
        PTHREAD_CFLAGS="$ok $PTHREAD_CFLAGS";;
    alpha*-osf*)  
        ok="-D_REENTRANT -D__C_ASM_H"
	PTHREAD_CFLAGS="$ok $PTHREAD_CFLAGS";;
esac
AC_MSG_RESULT(${ok})

if test "$pthread_ok" != "yes"; then
   PTHREAD_CHECK_LIB([-lpthread])
fi
if test "$pthread_ok" != "yes"; then
   PTHREAD_CHECK_LIB([-lpthreads])
fi
if test "$pthread_ok" != "yes"; then
   PTHREAD_CHECK_LIB([])
fi

AC_SUBST(PTHREAD_LIBS)
AC_SUBST(PTHREAD_CFLAGS)

dnl Finally, execute ACTION-IF-FOUND/ACTION-IF-NOT-FOUND:
if test "$pthread_ok" = "yes"; then
	ifelse([$1],,AC_DEFINE(HAVE_PTHREAD,1,[Define if you have POSIX threads libraries and header files.]),[$1])
	:
else
	pthread_ok=no
	$2
fi

])
