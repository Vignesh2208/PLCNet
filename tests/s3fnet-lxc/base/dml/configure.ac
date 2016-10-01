#
# configure.ac :- autoconf script, process this file with autoconf to
#                 produce a configure script.
#

AC_PREREQ(2.57)

AC_INIT([dml], [2.1.0], [liux@cis.fiu.edu])
AC_CONFIG_SRCDIR([dml.h])

AM_INIT_AUTOMAKE(prime-dml, 2.1.0)
AC_CONFIG_HEADER([dml-config.h:dml-config-h.in])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_LEX
AC_PROG_YACC
AC_PROG_RANLIB

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([malloc.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE

# Checks for library functions.
#AC_FUNC_ALLOCA
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC
AC_CHECK_FUNCS([strcasecmp strdup])

# check the availability of flex and bison
if test "$LEX" != "flex"; then
  HAVE_FLEX="no"
else
  HAVE_FLEX="yes"
fi
AM_CONDITIONAL([HAVE_FLEX], [test "$HAVE_FLEX" = "yes"])
if test "$YACC" != "bison -y"; then
  HAVE_BISON="no"
else
  HAVE_BISON="yes"
fi
AM_CONDITIONAL([HAVE_BISON], [test "$HAVE_BISON" = "yes"])

# dml debugging information
AC_ARG_ENABLE(dml-debug,    [  --enable-dml-debug      enable DML debugging information],
  ENABLE_DML_DEBUG=$enableval, ENABLE_DML_DEBUG="no")
if test "$ENABLE_DML_DEBUG" = yes; then
  AC_DEFINE([PRIME_DML_DEBUG], 1, [DML debugging information])
else
  AC_DEFINE([PRIME_DML_DEBUG], 0, [DML debugging information])
fi
AM_CONDITIONAL([ENABLE_DML_DEBUG], [test x"$ENABLE_DML_DEBUG" = x"yes"])

# dml location information
AC_ARG_ENABLE(dml-locinfo,  [  --enable-dml-locinfo    enable DML location information],
  ENABLE_DML_LOCINFO=$enableval, ENABLE_DML_LOCINFO="yes")
if test "$ENABLE_DML_LOCINFO" = yes; then
  AC_DEFINE([PRIME_DML_LOCINFO], 1, [DML location information])
else
  AC_DEFINE([PRIME_DML_LOCINFO], 0, [DML location information])
fi
AM_CONDITIONAL([ENABLE_DML_LOCINFO], [test x"$ENABLE_DML_LOCINFO" = x"yes"])

AC_CONFIG_FILES([Makefile test/dml2xml/Makefile test/dumpdml/Makefile test/listdml/Makefile])
AC_OUTPUT

echo " "
echo "----------------------------------------------------------------------";
echo "PRIME DML has been configured successfully with the following options:";
echo " "
echo "general debug information ...... $ENABLE_DML_DEBUG";
echo "location information ........... $ENABLE_DML_LOCINFO";
echo "----------------------------------------------------------------------";
echo " "

#
# Copyright (c) 2007 Florida International University.
#
# Permission is hereby granted, free of charge, to any individual or
# institution obtaining a copy of this software and associated
# documentation files (the "software"), to use, copy, modify, and
# distribute without restriction.
# 
# The software is provided "as is", without warranty of any kind,
# express or implied, including but not limited to the warranties of
# merchantability, fitness for a particular purpose and
# noninfringement. In no event shall Florida International University be
# liable for any claim, damages or other liability, whether in an
# action of contract, tort or otherwise, arising from, out of or in
# connection with the software or the use or other dealings in the
# software.
# 
# This software is developed and maintained by
#
#   The PRIME Research Group
#   School of Computing and Information Sciences
#   Florida International University
#   Miami, FL 33199, USA
#
# Contact Jason Liu <liux@cis.fiu.edu> for questions regarding the use
# of this software.
#

#
# $Id$
#
