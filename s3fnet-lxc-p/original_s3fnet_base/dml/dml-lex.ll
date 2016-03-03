%{
/*
 * dml-lex.ll :- DML lexical scanner.
 *
 * We use yacc and lex to parse DML scripts. This source code is
 * written in lex and is used to generate the lexical scanner for DML
 * language.
 */

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#else
#error "Missing header file: string.h"
#endif

#include "dml-parser.h"
#include "dml-exception.h"

/* yyless() return types: YYLESS_RETURN_TYPE and
   YYLESS_RETURN_EXPRESSION must be defined together. */
#ifndef YYLESS_RETURN_TYPE
#ifndef YYLESS_RETURN_EXPRESSION
#if defined(VOID_YYLESS)
#define YYLESS_RETURN_TYPE        void
#define YYLESS_RETURN_EXPRESSION  return
#elif defined(INT_YYLESS)
#define YYLESS_RETURN_TYPE        int
#define YYLESS_RETURN_EXPRESSION  return(0)
#else
#define YYLESS_RETURN_TYPE        void
#define YYLESS_RETURN_EXPRESSION  return
#endif
#endif /*YYLESS_RETURN_EXPRESSION*/
#endif /*YYLESS_RETURN_TYPE*/

/* yyunput() return type */
#ifndef  YYUNPUT_RETURN_TYPE
#if defined(VOID_YYUNPUT)
#define YYUNPUT_RETURN_TYPE  void
#elif defined(INT_YYUNPUT)
#define YYUNPUT_RETURN_TYPE  int
#else
#define YYUNPUT_RETURN_TYPE  void
#endif
#endif /*YYUNPUT_RETURN_TYPE*/

/* flex declares these itself and they would conflict if we left them
   defined. */
#ifndef FLEX_SCANNER
extern int yylook();
extern int yywrap();
extern int yyback(int* p, int m);
extern YYLESS_RETURN_TYPE  yyless(int);
extern YYUNPUT_RETURN_TYPE yyunput(int);
#endif /*FLEX_SCANNER*/

/* This one changes the way how column number is computed. */
#define TAB_WIDTH 8

/* Gets input (from file stream yyin) and stuffs it into "buf". The
   number of characters read, or YY_NULL, is returned in "result". */
#undef YY_INPUT
#define YY_INPUT(buf, result, max_size) \
  if(fgets(buf, max_size, yyin) == 0) { result = 0; } \
  else { s3f::dml::lex_line_start = (char*)buf; result = strlen(buf); \
    s3f::dml::lex_line_pos += s3f::dml::lex_line_length; \
    s3f::dml::lex_line_length = result; }

/* We redefine yywrap(), which always returns 1. */
#undef yywrap

/* location information recorded only if required */
#if PRIME_DML_LOCINFO
#define PRIME_DML_SET_LOCALE  yylval.locinfo = s3f::dml::lex_location()
#define PRIME_DML_MARK_LOCALE s3f::dml::lex_location(yylval.keyvalue->locinfo)
#else
#define PRIME_DML_SET_LOCALE  yylval.locinfo = 0
#define PRIME_DML_MARK_LOCALE
#endif

#if PRIME_DML_DEBUG
#if PRIME_DML_LOCINFO
#define PRIME_DML_PRINT_SINGLE(token) \
  fprintf(stdout, "%s:%d(%d) >> \"%s\" -> %d\n", \
          yylval.locinfo->filename(), yylval.locinfo->lineno(), \
          yylval.locinfo->offset(), yytext, token);
#define PRIME_DML_PRINT_KEYVAL(token) \
  fprintf(stdout, "%s:%d(%d) >> \"%s\" -> %d\n", \
          yylval.keyvalue->locinfo.filename(), \
          yylval.keyvalue->locinfo.lineno(), \
          yylval.keyvalue->locinfo.offset(), \
          yytext, token);
#else
#define PRIME_DML_PRINT_SINGLE(token) \
  fprintf(stdout, "\"%s\" -> %d\n", yytext, token);
#define PRIME_DML_PRINT_KEYVAL(token) \
  fprintf(stdout, "\"%s\" -> %d\n", yytext, token);
#endif
#else
#define PRIME_DML_PRINT_SINGLE(token)
#define PRIME_DML_PRINT_KEYVAL(token)
#endif /*PRIME_DML_DEBUG*/

namespace s3f {
namespace dml {

/* the start of the current input line */
static char* lex_line_start;

/* current processing location */
static char  lex_filename[1024]; // max file name length is 1024
static int   lex_lineno;

#ifdef DML_LOCINFO_FILEPOS
static int lex_line_pos;
static int lex_line_length;
#endif
  
}; // namespace dml
}; // namespace s3f

%}

%START CMT NORM BAB

/* single-character tokens: '[' and ']' */
SINGLE  ([\[\]])

/* key names may be started with a digit or '-' */
KEYNAME    ([a-zA-Z0-9_-]+)

/* character strings in dml may not be in quotes */
UNQUOTED_STRING  ([^ \t\n\[\]#]+)
QUOTED_STRING  (\"([^\\"]|\\.)*\")
STRING    ({UNQUOTED_STRING}|{QUOTED_STRING})

%%

<CMT>.     ;
<CMT>\n    { ++s3f::dml::lex_lineno; BEGIN NORM; }

<NORM>#    { BEGIN CMT; }

<NORM>{SINGLE}  { PRIME_DML_SET_LOCALE;
                  PRIME_DML_PRINT_SINGLE(yytext[0])
                  return yytext[0]; }

<NORM>{KEYNAME}  { yylval.keyvalue = new s3f::dml::KeyValue(yytext);
                   PRIME_DML_MARK_LOCALE;
                   PRIME_DML_PRINT_KEYVAL(LX_IDENT);
                   return LX_IDENT; }

<NORM>{STRING}  { yylval.keyvalue = new s3f::dml::KeyValue(yytext);
                  PRIME_DML_MARK_LOCALE;
                  PRIME_DML_PRINT_KEYVAL(LX_STRING);
                  return LX_STRING; }

<NORM>\n  { ++s3f::dml::lex_lineno; }
<NORM>.   ;

.     { BEGIN NORM; yyless(0); }
\n    { BEGIN NORM; yyless(0); }

%%

extern void yyrestart(FILE* file);
extern YY_BUFFER_STATE yy_create_buffer(FILE *file, int size);
extern void yy_switch_to_buffer(YY_BUFFER_STATE new_buffer);
extern void yy_delete_buffer(YY_BUFFER_STATE buffer);

namespace s3f {
namespace dml {

int lex_setup(const char* file)
{
  strncpy(lex_filename, file, 1023); 
  lex_filename[1023] = 0;
  lex_lineno = 1;
#ifdef DML_LOCINFO_FILEPOS
  lex_line_pos = lex_line_length = 0;
#endif
  yyin = fopen(file, "r");
  if(!yyin) {
    dml_throw_exception(dml_exception::open_dml_file, file);
    return 1;
  }
  yyrestart(yyin);
  /*yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));*/
  return 0;
}

void lex_teardown()
{
  lex_filename[0] = 0;
  lex_lineno = 0;
#ifdef DML_LOCINFO_FILEPOS
  lex_line_pos = lex_line_length = 0;
#endif
  yy_delete_buffer(YY_CURRENT_BUFFER);
  fclose(yyin);
  yy_switch_to_buffer(0);
}

#if PRIME_DML_LOCINFO
LocInfo* lex_location()
{
  int offset = 1;
  for(char* s = lex_line_start; s && s < yytext; s++) {
    if(*s == '\t') 
      offset = ((offset/TAB_WIDTH)+1)*TAB_WIDTH;
    else offset++;
  }
#ifdef DML_LOCINFO_FILEPOS
  int pos = lex_line_pos + yytext - lex_line_start;
  return new LocInfo(lex_filename, lex_lineno, offset, pos, pos+strlen(yytext));
#else
  return new LocInfo(lex_filename, lex_lineno, offset);
#endif
}

void lex_location(LocInfo& locinfo)
{
  int offset = 1;
  for(char* s = lex_line_start; s && s < yytext; s++) {
    if(*s == '\t') 
      offset = ((offset/TAB_WIDTH)+1)*TAB_WIDTH;
    else offset++;
  }
#ifdef DML_LOCINFO_FILEPOS
  int pos = lex_line_pos + yytext - lex_line_start;
  locinfo.set(lex_filename, lex_lineno, offset, pos, pos+strlen(yytext));
#else
  locinfo.set(lex_filename, lex_lineno, offset);
#endif
}
#endif /*PRIME_DML_LOCINFO*/

}; // namespace dml
}; // namespace s3f

int yywrap() { return 1; }

#ifndef FLEX_SCANNER
// flex has its own yyless
YYLESS_RETURN_TYPE yyless(int n)
{
  int i;
  extern int yyprevious;

  if (n >= yyleng) n = yyleng-1;
  if (n < 0) n = 0;
  for (i = yyleng-1; i >= n; --i) yyunput(yytext[i]);
  yytext[n] = 0;
  if (n != 0) yyprevious = yytext[n-1];
  yyleng = n;
  YYLESS_RETURN_EXPRESSION;
}
#endif /*FLEX_SCANNER*/

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
