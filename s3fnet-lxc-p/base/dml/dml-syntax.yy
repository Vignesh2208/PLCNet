%{
/*
 * dml-syntax.yy :- DML syntax analyzer.
 *
 * We use yacc and lex to parse DML files. This source code is written
 * in yacc and is used to generate the syntax analyzer for DML. The
 * parser is designed to start by invoking the function dml::parse(),
 * which returns a pointer to root of the dml tree, if the DML is
 * successfully parsed. Note that the parser code is not reentrant,
 * which means that the parser can only be called within a single
 * thread. In systems with multiple threads of control, it must be
 * called using some kind of mutual exclusion.
 */

#define PRIME_DML_SYNTAX_Y

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#include <stdio.h>

#if HAVE_STDLIB_H
#include <stdlib.h>
#else
#error "Missing header file: stdlib.h"
#endif

#include "dml-parser.h"
%}

%union {
  PRIME_DML_SYM_DMLCONFIG dmlconfig;
  PRIME_DML_SYM_ATTRKEY   attrkey;
  PRIME_DML_SYM_LOCINFO   locinfo;
  PRIME_DML_SYM_KEYVALUE  keyvalue;
}

%token <keyvalue> LX_IDENT LX_STRING
%token <locinfo>  '[' ']'

%start dml_script

%type <dmlconfig> nonempty_attribute_list
%type <dmlconfig> attribute_list
%type <dmlconfig> attribute
%type <attrkey>   attribute_key

%{

#ifndef YYLEX_EXTERN
#if defined(EXTERN_C_YYLEX)
#define YYLEX_EXTERN extern "C"
#else
#define YYLEX_EXTERN extern
#endif
#endif

#ifndef YYPARSE_EXTERN
#if defined(EXTERN_C_YYPARSE)
#define YYPARSE_EXTERN extern "C"
#else
#define YYPARSE_EXTERN extern
#endif
#endif

/* this makes it work with yacc on irix */
#define yylex yylex

YYLEX_EXTERN int yylex();
YYPARSE_EXTERN int yyparse();

namespace s3f {
namespace dml {

static FILE* dml_error_stream;
static dmlConfig* dml_root;

}; // namespace dml
}; // namespace s3f

using namespace s3f;
using namespace dml;

static void yyerror(const char* msg)
{
  char errstr[256];
#if PRIME_DML_LOCINFO
  LocInfo* loc = lex_location();
  char* locstr = loc->toString();
  if(msg) sprintf(errstr, "at %s (%s)", locstr, msg);
  else sprintf(errstr, "at %s", locstr);
  delete[] locstr;
  delete loc;
#else
  if(msg)  sprintf(errstr, "at unknown location (%s)", msg);
  else strcpy(errstr, "at unknown location");
#endif
  dml_throw_exception(dml_exception::parse_error, errstr);
  lex_teardown();
  if(dml_root) delete dml_root;
  dml_root = 0;
}

%}

%%

dml_script :
    nonempty_attribute_list {
      /* an DML language cannot be empty */
      dml_root = $1;
    }
  ;

nonempty_attribute_list :
    attribute {
      /* add the first child attribute */ 
      $$ = new dmlConfig($1);
    }
  | nonempty_attribute_list attribute {
      /* add more child attributes one after another */ 
      $$ = (dmlConfig*)$1->append_kid($2);
    }
  ;

attribute_list :
    /* empty */ { 
      $$ = new dmlConfig((dmlConfig*)0);
    }
  | nonempty_attribute_list
  ;

attribute : 
    attribute_key LX_IDENT {
      $$ = new dmlConfig($1, $2);
    }
  | attribute_key LX_STRING {
      $$ = new dmlConfig($1, $2);
    }
  | attribute_key '[' attribute_list ']' {
      $$ = (dmlConfig*)$3->add_key($1, $4);
      if($2) delete $2; /* it's possible '[' returns NULL */
      if($4) delete $4; /* it's possible ']' returns NULL */
    }
  ;

attribute_key :
    LX_IDENT
      { $$ = new AttrKey($1); }
  ;

%%

namespace s3f {
namespace dml {

dmlConfig* parse(FILE* errs, const char* file)
{
  dml_error_stream = errs;
  if(lex_setup(file)) return 0;
  yyparse();
  lex_teardown();
  return dml_root;
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
