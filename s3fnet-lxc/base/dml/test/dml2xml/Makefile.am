# 
# Makefile.am :- automake script for building DML tests.
#

TESTS = dml2xml.run

INCLUDES = $(DML_DEBUG_CFLAGS) $(DML_LOCINFO_CFLAGS)

EXTRA_DIST = $(TESTS) \
	test1.in \
	test1.ok \
	test2-1.in \
	test2-2.in \
	test2-3.in \
	test2-4.in \
	test2-5.in \
	test2-6.in \
	test2-7.in \
	test2.ok

CLEANFILES = \
	.dirstamp \
	test1.out \
	test2.out

LDADD = -L../.. -ldml
check_PROGRAMS = dml2xml
all_PROGRAMS = dml2xml
alldir=.
dml2xml_SOURCES = dml2xml.cc

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
