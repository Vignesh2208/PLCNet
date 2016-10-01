/*
 * dml-parser.h :- header file for the DML syntax analyzer.
 *
 * This header file contains type definitions as well as function
 * declarations used by the syntax analyzer to connect to lexical
 * scanner. The parse function is also declared here to be referenced
 * from the DML interface.
 */

#ifndef __PRIME_DML_PARSE_H__
#define __PRIME_DML_PARSE_H__

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#include <stdio.h>

#include "dml.h"

// these types are aliases and used in the syntax analyzer
typedef class s3f::dml::dmlConfig* PRIME_DML_SYM_DMLCONFIG;
typedef class s3f::dml::AttrKey*   PRIME_DML_SYM_ATTRKEY;
typedef class s3f::dml::LocInfo*   PRIME_DML_SYM_LOCINFO;
typedef class s3f::dml::KeyValue*  PRIME_DML_SYM_KEYVALUE;

#ifndef PRIME_DML_SYNTAX_Y
#include "dml-syntax.h"
#endif

namespace s3f {
namespace dml {

  /*
   * This function is called to set up the lexical scanner at each
   * time when a DML file is to be parsed. The function returns true
   * if an error occurs during parsing.
   */
  extern int lex_setup(const char* filename);

  /*
   * After we finish parsing a DML file, we need to finalize and wrap
   * up the state of the lexical scanner using this function.
   */
  extern void lex_teardown();

#if PRIME_DML_LOCINFO
  /*
   * Returns the location of the current symbol. The location
   * information is a newly created object.
   */
  extern LocInfo* lex_location();

  /*
   * Set the location of the current symbol. The location information
   * should be put into an object provided as a reference.
   */
  extern void lex_location(LocInfo&);
#endif /*PRIME_DML_LOCINFO*/

  /*
   * This is the function called by the DML interface when a DML file
   * is loaded in to be parsed. The function returns the root of the
   * DML tree if the file is parsed successfully. Otherwise, a NULL
   * pointer is returned.  The file descriptor for the error stream
   * and the name of the DML file are provided as arguments.
   */
  extern dmlConfig* parse(FILE* err, const char* filename);

}; // namespace dml
}; // namespace s3f

#endif /*__PRIME_DML_PARSE_H__*/

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
