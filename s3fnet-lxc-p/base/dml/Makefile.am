# 
# Makefile.am :- automake script for building the DML library.
#

SUBDIRS = . test

lib_LIBRARIES = libdml.a

libdml_a_SOURCES = \
	dml.h dml.cc \
	dml-dictionary.h dml-dictionary.cc \
	dml-locinfo.h dml-locinfo.cc \
	dml-tree.h dml-tree.cc \
	dml-exception.h dml-exception.cc \
	dml-parse.h dml-lex.ll dml-syntax.yy

EXTRA_libdml_a_SOURCES = \
	pre-dml-lex.cc \
	pre-dml-syntax.h \
	pre-dml-syntax.cc

BUILT_SOURCES = \
	dml-lex.cc \
	dml-syntax.h \
	dml-syntax.cc

CLEANFILES = \
	$(BUILT_SOURCES) \
	lex.yy.c \
	y.tab.h \
	y.tab.cc \
	y.output

AM_YFLAGS = -dt -v

# we probably should make this configurable also
MOVE = mv -f
COPY = cp

# we have to set up the following dependencies manually

# if flex or bison is not available, we use the lexical scanner or the
# syntax analyzer from previous runs
if HAVE_FLEX
dml-lex.cc:	dml-lex.ll
	$(LEX) $(LFLAGS) $(AM_LFLAGS) dml-lex.ll
	$(MOVE) lex.yy.c dml-lex.cc
else
dml-lex.cc:	pre-dml-lex.cc
	$(COPY) pre-dml-lex.cc dml-lex.cc
endif
if HAVE_BISON
dml-syntax.h dml-syntax.cc:	dml-syntax.yy
	$(YACC) $(YFLAGS) $(AM_YFLAGS) dml-syntax.yy
	$(MOVE) y.tab.c dml-syntax.cc
	$(MOVE) y.tab.h dml-syntax.h
else
dml-syntax.h dml-syntax.cc:	pre-dml-syntax.h pre-dml-syntax.cc
	$(COPY) pre-dml-syntax.h dml-syntax.h
	$(COPY) pre-dml-syntax.cc dml-syntax.cc
endif

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
