/**
 * dml-locinfo.cc :- location information of DML symbols.
 *
 * The location information contains file name, line number, and
 * column number, for each parsed symbols in DML. The location
 * information is only used when the macro PRIME_DML_LOCINFO is
 * defined as we compile this code.
 */

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#include <assert.h>
#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#else
#error "Missing header file: string.h"
#endif

#include "dml.h"

namespace s3f {
namespace dml {

/**
 * The location of a DML symbol consists of the name of the DML
 * file, a line number, and a column number.
 */
struct LocInfoImpl {
  const char* filename;
  int lineno;
  int offset;
#ifdef DML_LOCINFO_FILEPOS
  int startpos;
  int endpos;
#endif
};

LocInfo::LocInfo() {
#if PRIME_DML_LOCINFO
  locinfo_impl = new LocInfoImpl;
#ifdef DML_LOCINFO_FILEPOS
  set(0, 0, 0, 0, 0); 
#else
  set(0, 0, 0); 
#endif
#else
  locinfo_impl = 0;
#endif
}

#ifdef DML_LOCINFO_FILEPOS
LocInfo::LocInfo(const char* f, int l, int o, int sp, int ep) {
#if PRIME_DML_LOCINFO
  locinfo_impl = new LocInfoImpl;
  set(f, l, o, sp, ep);
#else
  locinfo_impl = 0;
#endif
}
#else
LocInfo::LocInfo(const char* f, int l, int o) {
#if PRIME_DML_LOCINFO
  locinfo_impl = new LocInfoImpl;
  set(f, l, o);
#else
  locinfo_impl = 0;
#endif
}
#endif

LocInfo::~LocInfo() {
  if(locinfo_impl) delete (LocInfoImpl*)locinfo_impl;
}

void LocInfo::set(const LocInfo& locinfo) {
  if(locinfo_impl) {
    assert(locinfo.locinfo_impl);
    ((LocInfoImpl*)locinfo_impl)->filename = dmlConfig::dictionary()->enter_string
      (((LocInfoImpl*)locinfo.locinfo_impl)->filename);
    ((LocInfoImpl*)locinfo_impl)->lineno = ((LocInfoImpl*)locinfo.locinfo_impl)->lineno;
    ((LocInfoImpl*)locinfo_impl)->offset = ((LocInfoImpl*)locinfo.locinfo_impl)->offset;
#ifdef DML_LOCINFO_FILEPOS
    ((LocInfoImpl*)locinfo_impl)->startpos = ((LocInfoImpl*)locinfo.locinfo_impl)->startpos;
    ((LocInfoImpl*)locinfo_impl)->endpos = ((LocInfoImpl*)locinfo.locinfo_impl)->endpos;
#endif
  }
}

#ifdef DML_LOCINFO_FILEPOS
void LocInfo::set_first(const LocInfo* locinfo) {
  if(locinfo_impl) {
    assert(locinfo->locinfo_impl);
    ((LocInfoImpl*)locinfo_impl)->filename = dmlConfig::dictionary()->enter_string
      (((LocInfoImpl*)locinfo->locinfo_impl)->filename);
    ((LocInfoImpl*)locinfo_impl)->lineno = ((LocInfoImpl*)locinfo->locinfo_impl)->lineno;
    ((LocInfoImpl*)locinfo_impl)->offset = ((LocInfoImpl*)locinfo->locinfo_impl)->offset;
    ((LocInfoImpl*)locinfo_impl)->startpos = ((LocInfoImpl*)locinfo->locinfo_impl)->startpos;
  }
}

void LocInfo::set_last(const LocInfo* locinfo) {
  if(locinfo_impl) {
    assert(locinfo->locinfo_impl);
    ((LocInfoImpl*)locinfo_impl)->endpos = ((LocInfoImpl*)locinfo->locinfo_impl)->endpos;
  }
}

void LocInfo::set(const char* filename, int lineno, int offset, int startpos, int endpos) { 
  if(locinfo_impl) {
    ((LocInfoImpl*)locinfo_impl)->filename = dmlConfig::dictionary()->enter_string(filename);
    ((LocInfoImpl*)locinfo_impl)->lineno = lineno;
    ((LocInfoImpl*)locinfo_impl)->offset = offset;
    ((LocInfoImpl*)locinfo_impl)->startpos = startpos;
    ((LocInfoImpl*)locinfo_impl)->endpos = endpos;
  }
}
#else
void LocInfo::set(const char* filename, int lineno, int offset) { 
  if(locinfo_impl) {
    ((LocInfoImpl*)locinfo_impl)->filename = dmlConfig::dictionary()->enter_string(filename);
    ((LocInfoImpl*)locinfo_impl)->lineno = lineno;
    ((LocInfoImpl*)locinfo_impl)->offset = offset;
  }
}
#endif

const char* LocInfo::filename() const { 
  if(locinfo_impl) return ((LocInfoImpl*)locinfo_impl)->filename; 
  else return "unknown file";
}

int LocInfo::lineno() const { 
  if(locinfo_impl) return ((LocInfoImpl*)locinfo_impl)->lineno; 
  else return 0;
}

int LocInfo::offset() const {
  if(locinfo_impl) return ((LocInfoImpl*)locinfo_impl)->offset;
  else return 0;
}

#ifdef DML_LOCINFO_FILEPOS
int LocInfo::startpos() const { 
  if(locinfo_impl) return ((LocInfoImpl*)locinfo_impl)->startpos; 
  else return 0;
}

int LocInfo::endpos() const {
  if(locinfo_impl) return ((LocInfoImpl*)locinfo_impl)->endpos;
  else return 0;
}
#endif

char* LocInfo::toString(char* buf, int bufsiz) const
{
  if(locinfo_impl) {
    if(((LocInfoImpl*)locinfo_impl)->filename) { // prepend the file name if specified
      if(!buf) {
	bufsiz = strlen(((LocInfoImpl*)locinfo_impl)->filename)+100;
	buf = new char[bufsiz];
	assert(buf);
      }
      if(bufsiz > 0) {
	snprintf(buf, bufsiz, "\"%s\":%d(%d)", ((LocInfoImpl*)locinfo_impl)->filename, 
		 ((LocInfoImpl*)locinfo_impl)->lineno, ((LocInfoImpl*)locinfo_impl)->offset);
	buf[bufsiz-1] = 0; // make sure everything fits into the buffer
	return buf;
      } else return 0;
    } else { // prepend "unknown file" if unspecified
      if(!buf) {
	bufsiz = 100;
	buf = new char[bufsiz];
	assert(buf);
      }
      if(bufsiz > 0) {
	if(((LocInfoImpl*)locinfo_impl)->lineno > 0)
	  snprintf(buf, bufsiz, "unknown file: %d(%d)", 
		   ((LocInfoImpl*)locinfo_impl)->lineno, ((LocInfoImpl*)locinfo_impl)->offset);
	else strncpy(buf, "unknown location", bufsiz);
	buf[bufsiz-1] = 0; // make sure everything fits into the buffer
	return buf;
      } else return 0;
    }
  } else {
    if(!buf) {
      bufsiz = 100;
      buf = new char[bufsiz];
      assert(buf);
    }
    strncpy(buf, "unknown location", bufsiz);
    return buf;
  }
}

int operator ==(const LocInfo& loc1, const LocInfo& loc2) {
  if(loc1.locinfo_impl) {
    assert(loc2.locinfo_impl);
#ifdef DML_LOCINFO_FILEPOS
    return !((LocInfoImpl*)loc1.locinfo_impl)->filename && !((LocInfoImpl*)loc2.locinfo_impl)->filename &&
      !strcmp(((LocInfoImpl*)loc1.locinfo_impl)->filename, ((LocInfoImpl*)loc2.locinfo_impl)->filename) &&
      ((LocInfoImpl*)loc1.locinfo_impl)->lineno == ((LocInfoImpl*)loc2.locinfo_impl)->lineno &&
      ((LocInfoImpl*)loc1.locinfo_impl)->offset == ((LocInfoImpl*)loc2.locinfo_impl)->offset &&
      ((LocInfoImpl*)loc1.locinfo_impl)->startpos == ((LocInfoImpl*)loc2.locinfo_impl)->startpos &&
      ((LocInfoImpl*)loc1.locinfo_impl)->endpos == ((LocInfoImpl*)loc2.locinfo_impl)->endpos;
#else
    return !((LocInfoImpl*)loc1.locinfo_impl)->filename && !((LocInfoImpl*)loc2.locinfo_impl)->filename &&
      !strcmp(((LocInfoImpl*)loc1.locinfo_impl)->filename, ((LocInfoImpl*)loc2.locinfo_impl)->filename) &&
      ((LocInfoImpl*)loc1.locinfo_impl)->lineno == ((LocInfoImpl*)loc2.locinfo_impl)->lineno &&
      ((LocInfoImpl*)loc1.locinfo_impl)->offset == ((LocInfoImpl*)loc2.locinfo_impl)->offset;
#endif
  } else return 0;
}

}; // namespace dml
}; // namespace s3f

/*
 * Copyright (c) 2007 Florida International University.
 *
 * Permission is hereby granted, free of charge, to any individual or
 * institution obtaining a copy of this software and associated
 * documentation files (the "software"), to use, copy, modify, and
 * distribute without restriction.
 * 
 * The software is provided "as is", without warranty of any kind,
 * express or implied, including but not limited to the warranties of
 * merchantability, fitness for a particular purpose and
 * noninfringement. In no event shall Florida International University be
 * liable for any claim, damages or other liability, whether in an
 * action of contract, tort or otherwise, arising from, out of or in
 * connection with the software or the use or other dealings in the
 * software.
 * 
 * This software is developed and maintained by
 *
 *   The PRIME Research Group
 *   School of Computing and Information Sciences
 *   Florida International University
 *   Miami, FL 33199, USA
 *
 * Contact Jason Liu <liux@cis.fiu.edu> for questions regarding the use
 * of this software.
 */

/*
 * $Id$
 */
