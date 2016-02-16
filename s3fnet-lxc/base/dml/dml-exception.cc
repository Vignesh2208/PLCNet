/**
 * dml-exception.cc :- exception handler.
 */

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#if HAVE_STRING_H
#include <string.h>
#else
#error "Missing header file: string.h"
#endif

#include "dml-exception.h"

namespace s3f {
namespace dml {

dml_exception::dml_exception(int c, const char* s) : code(c) {
  explanation[0] = 0; strcat(explanation, "ERROR: ");
  switch(code) {
  case no_exception: strcat(explanation, "uninitialized exception"); break;
  case other_exception: break;
  case open_dml_file: strcat(explanation, "can't open DML file"); break;
  case parse_error: strcat(explanation, "parse error"); break;
  case illegal_attribute_key: strcat(explanation, "illegal attribute key"); break;
  case missing_attachment: strcat(explanation, "attachment not found"); break;
  case nonlist_attachment: strcat(explanation, "not list attachment"); break;
  case recursive_expansion: strcat(explanation, "recursive expansion"); break;
  default: strcat(explanation, "programming error");
  }
  if(s) { strcat(explanation, " "); strcat(explanation, s); } // possible overflow
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
