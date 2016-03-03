/**
 * dml-tree.cc :- source code implementing the DML tree structure.
 *
 * A DML script can be viewed as a tree structure. Each node in the
 * tree, which can be identified and retrieved via a keypath, is
 * either an attribute or a list of attributes. This module contains
 * the definition of classes and data structures necessary to
 * construct and traverse the DML tree.
 */

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#include <ctype.h>
#include <stdio.h>

#if HAVE_STRING_H
#include <string.h>
#else
#error "Missing header file: string.h"
#endif

#if HAVE_STRINGS_H
#include <strings.h>
#else
#error "Missing header file: strings.h"
#endif

#include "dml.h"

namespace s3f {
namespace dml {

  /*
   * IMPORTANT: We use a common buffer to store the string for the
   * location of a DML tree node. That is, if you want to print out
   * two locations consecutively, you need to separate the call into
   * two printf statements. Otherwise the latter one will overwrite
   * the previous call.
   */
  #define PRIME_DML_LOCINFO_BUFSIZ 256
  static char PRIME_DML_LOCINFO_BUFFER[PRIME_DML_LOCINFO_BUFSIZ];
  #define PRIME_DML_LOCATION_STRING(node) \
    node->locinfo.toString(PRIME_DML_LOCINFO_BUFFER, \
                           PRIME_DML_LOCINFO_BUFSIZ)

  // used to store error message
  #define ERRSTR_LENGTH 256
  static char errstr[ERRSTR_LENGTH];

/* The following is used to enable simple pattern matching for strings 
   with wildcards (such as * and ?). */

/* ***************************************************************************
 *
 *          Copyright 1992-2002 by Pete Wilson All Rights Reserved
 *           50 Staples Street : Lowell Massachusetts 01851 : USA
 *        http://www.pwilson.net/   pete@pwilson.net   +1 978-454-4547
 *
 * This item is free software: you can redistribute it and/or modify it as 
 * long as you preserve this copyright notice. Pete Wilson prepared this item 
 * hoping it might be useful, but it has NO WARRANTY WHATEVER, not even any 
 * implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *
 *************************************************************************** */

/* ***************************************************************************
 *
 *                            WC_STRNCMP.H
 *
 * Defines for string searches. Revision History:
 *
 *     DATE      VER                     DESCRIPTION
 * -----------  ------  ----------------------------------------------------
 *  8-nov-2001   1.06   rewrite
 * 20-jan-2002   1.07   rearrange wc test; general dust and vacuum
 *
 *************************************************************************** */

static 
int wc_strncmp(const char * pattern,   /* match this str (can contain *,?) */
               const char * candidate, /*   against this one. */
               int count,              /* require count chars in pattern */
               int do_case,            /* 0 = no case, !0 = cased compare */ 
               int do_wildcards);      /* 0 = no wc's, !0 = honor * and ? */

#define WC_QUES '?'
#define WC_STAR '*'

/* string and character match/no-match return codes */

enum {
  WC_MATCH = 0,     /* char/string-match succeed */
  WC_MISMATCH,      /* general char/string-match fail */
  WC_PAT_NULL_PTR,  /* (char *) pattern == NULL */
  WC_CAN_NULL_PTR,  /* (char *) candidate == NULL */
  WC_PAT_TOO_SHORT, /* too few pattern chars to satisfy count */
  WC_CAN_TOO_SHORT  /* too few candidate chars to satisy '?' in pattern */
};

/* ***************************************************************************
 *
 *          Copyright 1992-2002 by Pete Wilson All Rights Reserved
 *           50 Staples Street : Lowell Massachusetts 01851 : USA
 *       http://www.pwilson.net/   pete@pwilson.net   +1 978-454-4547
 *
 * This item is free software; you can redistribute it and/or modify it under 
 * the terms of the GNU General Public License as published by the 
 * Free Software Foundation; either version 2.1 of the License, or (at your 
 * option) any later version.
 *
 * Pete Wilson prepared this item in the hope that it might be useful, but it 
 * has NO WARRANTY WHATEVER, nor even any implied warranty of MERCHANTABILITY 
 * or FITNESS FOR A PARTICULAR PURPOSE. 
 *
 *************************************************************************** */

/* ***************************************************************************
 *
 *                          WC_STRNCMP.C
 *
 * Public function to match two strings with or without wildcards:
 *    int wc_strncmp()  return zero if string match, else non-zero.
 *
 * Local private function to match two characters:
 *    int ch_eq()       return zero if char match, else non-zero.       
 *
 * Revision History:
 *
 *     DATE      VER                     DESCRIPTION
 * -----------  ------  ----------------------------------------------------
 *  8-nov-2001   1.06   rewrite
 * 20-jan-2002   1.07   rearrange wc test; general cleanup
 * 14-feb-2002   1.08   fix bug (thanks, steph!): wc test outside ?,* tests
 * 20-feb-2002   1.09   rearrange commentary as many suggested
 * 25-feb-2002   1.10   wc_strlen() return size_t per dt, excise from here
 *
 *************************************************************************** */

static int ch_eq(int c1, int c2, int do_case); /* compare two characters */

/* ***************************************************************************
 *
 * Function:   wc_strncmp(
 *               const char * pattern,   // match this string (can contain ?, *)
 *               const char * candidate, //   against this one.
 *               int count,              // require at least count chars in pattern
 *               int do_case,            // 0 = no case, !0 = cased compare
 *               int do_wildcards)       // 0 = no wc's, !0 = honor ? and *
 *
 * Action:     See if the string pointed by candidate meets the criteria    
 *             given in the string pointed by pattern. The pattern string   
 *             can contain the special wild-card characters:                
 *             * -- meaning match any number of characters, including zero, 
 *               in the candidate string;                                   
 *             ? -- meaning match any single character, which must be present 
 *               to satisfy the match, in the candidate string.
 *
 *             The int arg count tells the minimum length of the pattern
 *             string, after '*' expansion, that will satisfy the string
 *             compare in this call. If count is negative, then forbid
 *             abbreviations: pattern and candidate strings must match exactly
 *             in content and length to give a good compare. If count is 0, 
 *             then an empty pattern string (pattern == "") returns success. 
 *             If count is positive, then must match "count" characters
 *             in the two strings to succeed, except we yield success if:
 *             -- pattern and candidate strings are the same length; or
 *             -- pattern string is shorter than "count"; or
 *             -- do_wildcards > 0 and final pattern char == '*'. 
 * 
 *             If the integer argument do_case == 0, then ignore the case of 
 *             each character: compare the tolower() of each character; if 
 *             do_case != 0, then consider case in character compares.
 *
 *             If the int arg do_wildcards == 0, then treat the wildcard 
 *             characters '*' and '?' just like any others: do a normal 
 *             char-for-char string compare. But if do_wildcards is nonzero, 
 *             then the string compare uses those wildcard characters as 
 *             you'd expect.
 *
 * Returns:    WC_MATCH on successful match. If not WC_MATCH, then the
 *             return val conveys a little more info: see wc_strncmp.h
 *
 * Note on Resynchronization: After a span of one or more pattern-string '*' 
 *             found; and an immediately following span of zero [sic: 0] or 
 *             more '?', we have to resynchronize the candidate string 
 *             with the pattern string: need to find a place in the 
 *             candidate to restart our compares. We've not a clue how many 
 *             chars a span of '*' will match/absorb/represent until we find 
 *             the next matching character in the candidate string. 
 *             For example:
 *       
 * -------------------------------------------------------------------------
 *   patt   cand   resync at 'A'           what's going on?
 *  ------ ------- ------------- ------------------------------------------
 *  "***A" "AbcdA"   "A----"     "***" matches zero characters
 *  "**A"  "aAcdA"   "-A---"     "**" absorbs just one char: "a"
 *  "*??A" "abcdA"   "----A"     "*" absorbs "ab", "??" skips "cd"
 *  "**?A" "abAdA"   "--A--"     "**" absorbs "a", "?" skips "b" 
 * -------------------------------------------------------------------------
 *      
 *             During any resync phase, we'll naturally be looking for the 
 *             end of the candidate string and fail if we see it.
 *   
 *************************************************************************** */
int 
wc_strncmp(const char * pattern, 
           const char * candidate, 
           int count, 
           int do_case, 
           int do_wildcards) 
{
  const char * can_start;
  unsigned int star_was_prev_char;
  unsigned int ques_was_prev_char;
  unsigned int retval;

  if (pattern == NULL)           /* avoid any pesky coredump, please */
  {
    return WC_PAT_NULL_PTR;
  }

  if (candidate == NULL) 
  {
    return WC_CAN_NULL_PTR;
  }

  /* match loop runs, comparing pattern and candidate strings char by char,
   until (1) the end of the pattern string is found or (2) somebody found
   some kind of mismatch condition. we deal with four cases in this loop: 
     -- pattern char is '?'; or 
     -- pattern char is '*'; or 
     -- candidate string is exhausted; or
     -- none of the above, pattern char is just a normal char.  */

  can_start = candidate;         /* to calc n chars compared at exit time */
  star_was_prev_char = 0;        /* show previous character was not '*' */
  ques_was_prev_char = 0;        /* and it wasn't '?', either */
  retval = WC_MATCH;             /* assume success */

  while (retval == WC_MATCH && *pattern != '\0') 
  {

    if (do_wildcards != 0)       /* honoring wildcards? */
    { 

      /* first: pattern-string character == '?' */

      if (*pattern == WC_QUES)   /* better be another char in candidate */
      { 
        ques_was_prev_char = 1;  /* but we don't care what it is */
      }
    
      /* second: pattern-string character == '*' */

      else if (*pattern == WC_STAR) 
      {  
        star_was_prev_char = 1;  /* we'll need to resync later */
        ++pattern;
        continue;                /* rid adjacent stars */ 
      }
    }

    /* third: any more characters in candidate string? */

    if (*candidate == '\0') 
    {
      retval = WC_CAN_TOO_SHORT;
    }

    else if (ques_was_prev_char > 0) /* all set if we just saw ques */
    {
      ques_was_prev_char = 0;        /* reset and go check next pair */
    }

    /* fourth: plain old char compare; but resync first if necessary */

    else 
    {
      if (star_was_prev_char > 0) 
      { 
        star_was_prev_char = 0;

        /* resynchronize (see note in header) candidate with pattern */

        while (WC_MISMATCH == ch_eq(*pattern, *candidate, do_case)) 
        {
          if (*candidate++ == '\0') 
          {
            retval = WC_CAN_TOO_SHORT;
            break;
          }
        }
      }           /* end of re-sync, resume normal-type scan */

      /* finally, after all that rigamarole upstairs: the main char compare */

      else  /* if star or ques was not last previous character */
      {
        retval = ch_eq(*pattern, *candidate, do_case);
      }
    }

    ++candidate;  /* point next chars in candidate and pattern strings */
    ++pattern;    /*   and go for next compare */

  }               /* while (retval == WC_MATCH && *pattern != '\0') */


  if (retval == WC_MATCH) 
  {

  /* we found end of pattern string, so we've a match to this point; now make
     sure "count" arg is satisfied. we'll deem it so and succeed the call if: 
     - we matched at least "count" chars; or
     - we matched fewer than "count" chars and:
       - pattern is same length as candidate; or
       - we're honoring wildcards and the final pattern char was star. 
     spell these tests right out /completely/ in the code */

    int min;
    int nmatch;

    min = (count < 0) ?                  /* if count negative, no abbrev: */
           strlen(can_start) :           /*   must match two entire strings */
           count;                        /* but count >= 0, can abbreviate */

    nmatch = candidate - can_start;      /* n chars we did in fact match */

    if (nmatch >= min)                   /* matched enough? */
    {                                    /* yes; retval == WC_MATCH here */
      ;                                  
    }
    else if (*candidate == '\0')         /* or cand same length as pattern? */
    {                                    /* yes, all set */
      ;                                  
    }
    else if (star_was_prev_char != 0)    /* or final char was star? */
    {                                    /* yes, succeed that case, too */
      ;                                  
    }                          

    else                                 /* otherwise, fail */
    {
      retval = WC_PAT_TOO_SHORT;           
    }
  }

  return retval;
}

/* ***************************************************************************
 *
 * Function:   int ch_eq(int c1, int c2, int do_case)                  
 *
 * Action:     Compare two characters, c1 and c2, and return WC_MATCH if     
 *             chars are equal, WC_MISMATCH if chars are unequal. If the 
 *             integer arg do_case == 0, treat upper-case same as lower-case.
 *
 * Returns:    WC_MATCH or WC_MISMATCH
 *
 *************************************************************************** */
static int   
ch_eq(int c1, 
      int c2, 
      int do_case) 
{
  if (do_case == 0)      /* do_case == 0, ignore  case */
  {                
    c1 = tolower(c1);
    c2 = tolower(c2);
  }
  return (c1 == c2) ? WC_MATCH : WC_MISMATCH;
}

#define my_wc_strncmp(pattern, candidate) \
  wc_strncmp(pattern, candidate, strlen(candidate), 0, 1)

/* ************************** END OF WC_STRNCMP *************************** */


/* methods of KeyValue class */

KeyValue::KeyValue(const char* yytext)
{
  // the string taken from the lexical scanner is entered into the
  // dictionary; we use the reference counter scheme to save memory
  // space by duplicate strings
  assert(yytext && yytext[0]);
  ident = dmlConfig::dictionary()->enter_string(yytext);
}

KeyValue::~KeyValue()
{
  // decrement the reference counter; reclaim it if there's no more
  // reference exist
  assert(ident);
  dmlConfig::dictionary()->scrap_string(ident);
}

/* methods of AttrKey class */

AttrKey::AttrKey(KeyValue* keyval)
{
  assert(keyval && keyval->ident);

  // only IDENT type has the ident variable not NULL
  key_ident = 0;
  if(!strcasecmp(keyval->ident, "_extends"))
    key_type = DML_KEYTYPE_EXTENDS;
  else if(!strcasecmp(keyval->ident, "_find"))
    key_type = DML_KEYTYPE_FIND;
  else if(!strcasecmp(keyval->ident, "_schema"))
    key_type = DML_KEYTYPE_SCHEMA;
  else {
    key_type = DML_KEYTYPE_IDENT;
    key_ident = dmlConfig::dictionary()->enter_string
      (keyval->ident); // only a reference here
  }
  locinfo.set(keyval->locinfo);
  delete keyval;
}

AttrKey::~AttrKey()
{
  if(key_ident) {
    assert(key_type == DML_KEYTYPE_IDENT);
    dmlConfig::dictionary()->scrap_string(key_ident);
  }
}

AttrKey::AttrKey(const AttrKey& key)
{
  key_type = key.key_type;
  if(key.key_ident) {
    assert(key_type == DML_KEYTYPE_IDENT);
    key_ident = dmlConfig::dictionary()->enter_string
      (key.key_ident); // only a reference
  } else key_ident = 0;
  locinfo.set(key.locinfo);
}

AttrKey* AttrKey::clone()
{
  AttrKey* newkey = new AttrKey(*this);
  assert(newkey);
  return newkey;
}

void AttrKey::print(FILE* fp, int space) const
{
  if(space>0) fprintf(fp, "%*c", space, ' ');
  fprintf(fp, "%s", ident());
}

/* methods of AttrNode class */

// parse the C-style string (possibly with escape characters, and
// octal or hex representations) one character at a time
int AttrNode::next_char(const char*& s)
{
  int ch,v;

  ch = *s++;
  if (ch == '\\') {
    ch = *s++;
    if (ch >= '0' && ch <= '7') {
      v = ch - '0';
      if (*s >= '0' && *s <= '7') {
        v = (v<<3) + *s++ - '0';
        if (*s >= '0' && *s <= '7') v = (v<<3) + *s++ - '0';
      }
      ch = v;
    }
    else if (ch == 'x') {
      v = 0;
      for ( ; ; ) {
        if (isdigit(*s)) v = (v<<4) + *s++ - '0';
        else if (*s >= 'a' && *s <= 'f') v = (v<<4) + *s++ - 'a' + 10;
        else if (*s >= 'A' && *s <= 'F') v = (v<<4) + *s++ - 'A' + 10;
        else break;
      }
      ch = v;
    }
    else switch (ch) {
    case 'n' :
      ch = '\n';
      break;
    case 'b' :
      ch = '\b';
      break;
    case 'r' :
      ch = '\r';
      break;
    case 't' :
      ch = '\t';
      break;
    case 'f' :
      ch = '\f';
      break;
    case 'v' :
      ch = '\v';
      break;
    case 'a' :
      ch = '\a';
      break;
    default :
      break;
    }
  }

  return ch;
}

// parse the DML string; translate the string into an internal
// representation
const char* AttrNode::make_string(KeyValue* keyval) const
{
  // this function can only be called if the attribute is a singleton
  // (of type STRING); because of this, the attribute must have a key
  // (i.e., it must not be a root)
  assert(attr_type == DML_ATTRTYPE_STRING);
  assert(keyval && keyval->ident);

  int i = strlen(keyval->ident);
  if(keyval->ident[0] != '"') {
    // the string is not in quotes, so it must be a C-style string; in
    // this case, we use this string verbatim
    return dmlConfig::dictionary()->enter_string(keyval->ident);
  } else {
    // the string is in quotes, we need to translate the string into
    // internal representation by filtering out the special characters
    const char* s = keyval->ident; // we don't touch the original
    ++s; --i; // skip the first "
    assert(s[i-1] == '"'); --i; // skip the last "
    const char* t = &s[i]; 
    // by now, s and t mark the beginning and the end of the string

    // the new translated string can only be shorter
    char* buf = new char[i+1]; assert(buf);
    
    // scan through the string and replace escape characters
    char* p = buf;
    while(s < t) {
      i = next_char(s); // s is incremented automatically
      *p++ = (char)(i&0xff); // since i is a signed integer
    }
    *p++ = 0;
    
    const char* q = dmlConfig::dictionary()->enter_string(buf);
    delete[] buf;
    return q;
  }
}

AttrNode* AttrNode::search_attribute(char* keypath, AttrNode* root)
{
  assert(attr_type == DML_ATTRTYPE_LIST);
  assert(keypath);
  assert(root);

  if(*keypath == 0) return 0;
  else if(*keypath == '.') { // absolute path
    keypath++;
    if(*keypath == '.') return 0; // consecutive '.' not allowed
    else return root->search_attribute(keypath, root);
  } else { // relative path
    // make the key string before the first '.'
    char* mykey = new char[strlen(keypath)+1];
    assert(mykey);
    register int i;
    for(i=0; keypath[i] != 0 && keypath[i] != '.'; i++)
      mykey[i] = keypath[i];
    mykey[i] = 0;

    // no keyword allowed in key path
    if(!strcasecmp(mykey, "_extends") ||
       !strcasecmp(mykey, "_find") ||
       !strcasecmp(mykey, "_schema")) {
      delete[] mykey;
      return 0; 
    }

    // adjust the key path for the next level
    keypath += strlen(mykey);
    if(*keypath == '.') keypath++;
    if(*keypath == '.') { // consecutive '.' not allowed
      delete[] mykey;
      return 0;
    }
       
    // iterate for each child attribute
    for(std::vector<AttrNode*>::iterator iter = attr_value_list->begin();
        iter != attr_value_list->end(); iter++) {
      assert((*iter)->attr_key);
      if(!strcasecmp(mykey, (*iter)->attr_key->ident())) { // find a match
        if(*keypath == 0) { // no more keys: this is it!
          delete[] mykey;
          return (*iter);
        } else if((*iter)->attr_type == DML_ATTRTYPE_LIST) {
	  // call the same function recursively
          AttrNode* found = (*iter)->search_attribute(keypath, root);
          if(found) {
            delete[] mykey;
            return found;
          }
          // we need to find a match; maybe we followed the wrong path
          // previously; we should continue searching the rest of child
          // attributes!
        }
      }
    }

    // didn't find the attribute
    delete[] mykey;
    return 0;
  }
}

AttrNode::AttrNode(AttrNode* firstkid)
{
  // if firstkid is NULL, we make an empty list node
  refcnt = 1;
  attr_type = DML_ATTRTYPE_LIST;
  attr_parent = 0;
  attr_key = 0;
  if(firstkid) {
    firstkid->attr_parent = this;
    attr_value_list = new std::vector<AttrNode*>();
    assert(attr_value_list);
    attr_value_list->push_back(firstkid);
  } else attr_value_list = 0;

  attr_expand = 0; // the node has not been expanded
  expanding = 0;   // we are not expanding now

  if(firstkid) locinfo.set(firstkid->location());
}

AttrNode::AttrNode(AttrKey* key, KeyValue* keyval)
{
  assert(key && keyval);
  refcnt = 1;
  attr_type = DML_ATTRTYPE_STRING;
  attr_parent = 0;
  attr_key = key;
  attr_value_string = make_string(keyval);

  attr_expand = 0; // the node has not been expanded
  expanding = 0;   // we are not expanding now

#ifdef DML_LOCINFO_FILEPOS
  locinfo.set_first(&key->locinfo);
  locinfo.set_last(&keyval->locinfo);
#else
  locinfo.set(keyval->locinfo);
#endif
  delete keyval;
}

AttrNode::AttrNode(const AttrNode& node)
{
  refcnt = 1;
  attr_type = node.attr_type;
  attr_parent = 0;
  attr_key = node.attr_key ? node.attr_key->clone() : 0;
  if(attr_type == DML_ATTRTYPE_LIST) {
    if(node.attr_value_list) {
      attr_value_list = new std::vector<AttrNode*>();
      assert(attr_value_list);
      for(std::vector<AttrNode*>::iterator iter = node.attr_value_list->begin();
          iter != node.attr_value_list->end(); iter++) {
        AttrNode* newnode = (*iter)->clone();
        newnode->attr_parent = this;
        attr_value_list->push_back(newnode);
      }
    } else attr_value_list = 0; // empty list
  } else {
    assert(node.attr_value_string);
    attr_value_string = dmlConfig::dictionary()->enter_string
      (node.attr_value_string);
  }

  expanding = 0;   // we are not expanding now
  assert(!node.expanding); // we don't allow cloning a node that is currently expanding
  if(node.attr_expand) { 
    // if the node to be cloned has already been expanded, we expand
    // the new node also
    if(node.attr_expand == &node) attr_expand = this; // not really an expansion
    else {
      // Normally, reference expansion should be recalculated on the
      // new tree, starting from its root. But here we just make a
      // copy of the old tree in case the cloning of the tree does not
      // include this portion of the tree. This could cause the memory
      // consumption to increase dramatically.
      attr_expand = node.attr_expand->clone();
    }
  } else attr_expand = 0; // the node has not been expanded

  locinfo.set(node.locinfo);
}

AttrNode::~AttrNode()
{
  if(refcnt > 1) {
    fprintf(stderr, "WARNING: dangling references result from deleting DML configuration;"
	    "use the dispose() method instead of delete");
    refcnt--; // doesn't matter here, the object is gone!
  } else cleanup();
}

void AttrNode::dispose()
{
  assert(refcnt >= 1);
  if(refcnt > 1) refcnt--;
  else delete this;
}

void AttrNode::cleanup()
{
  if(attr_key) {
    delete attr_key;
    attr_key = 0;
  }

  if(attr_value_list) {
    if(attr_type == DML_ATTRTYPE_LIST) {
      for(std::vector<AttrNode*>::iterator iter = attr_value_list->begin();
          iter != attr_value_list->end(); iter++) {
	(*iter)->dispose(); // dispose is friendlier to reference counters than delete
      }
      delete attr_value_list;
      attr_value_list = 0;
    } else {
      dmlConfig::dictionary()->scrap_string(attr_value_string);
      attr_value_string = 0;
    }
  }

  if(attr_expand && attr_expand != this) 
    attr_expand->dispose(); // if this is a true expansion
  attr_expand = 0;
  expanding = 0;
}

AttrNode* AttrNode::clone()
{
  assert(refcnt >= 1);
  refcnt++;
  return this;
}

AttrNode* AttrNode::add_key(AttrKey* key, LocInfo* bracket_locinfo)
{
  assert(key);
  assert(!attr_key); // must have not done this before
  attr_key = key;
#ifdef DML_LOCINFO_FILEPOS
  locinfo.set_first(&key->locinfo);
  if(bracket_locinfo) locinfo.set_last(bracket_locinfo);
#else
  locinfo.set(key->locinfo);
#endif
  return this;
}

AttrNode* AttrNode::append_kid(AttrNode* newkid)
{
  assert(newkid);
  assert(attr_type == DML_ATTRTYPE_LIST);
  // we don't allow adding children to an empty list node, which mean
  // there should be at least one child registered already
  assert(attr_value_list && attr_value_list->size()>0);
  attr_value_list->push_back(newkid);
  newkid->attr_parent = this;
#ifdef DML_LOCINFO_FILEPOS
  locinfo.set_last(&newkid->locinfo);
#endif
  return this;
}

int AttrNode::expand(AttrNode* root)
{
  if(attr_expand) return 0; // has been expanded earlier, skip now

  // search for the tree root if it's not provided
  if(!root) {
    root = this;
    while(root->attr_parent) 
      root = root->attr_parent;
  }
  
  if(attr_type == DML_ATTRTYPE_LIST) {

    // _extends and _find shouldn't be followed by a list
    if(key_type() == AttrKey::DML_KEYTYPE_EXTENDS ||
       key_type() == AttrKey::DML_KEYTYPE_FIND) {
      assert(attr_key);
      sprintf(errstr, "at %s", PRIME_DML_LOCATION_STRING(attr_key));
      dml_throw_exception(dml_exception::illegal_attribute_key, errstr);
      return 1;
    }

    // expand child attributes, recursively
    if(key_type() != AttrKey::DML_KEYTYPE_SCHEMA && attr_value_list) {
      for(std::vector<AttrNode*>::iterator iter = attr_value_list->begin();
          iter != attr_value_list->end(); iter++) {
        assert((*iter)->attr_parent == this);
        if((*iter)->expand(root)) return 1;
      }
    }

    // mark that this node has been expanded (children of schema are
    // left untouched)
    attr_expand = this;

  } else { // string type

    if(key_type() == AttrKey::DML_KEYTYPE_EXTENDS) {

      char* keypath = (char*)attr_value_string;
      // only static key path (no expansion) is allowed here
      AttrNode* found = attr_parent->search_attribute(keypath, root);
      if(!found) {
	sprintf(errstr, ": _extends %s at %s", keypath, 
		PRIME_DML_LOCATION_STRING(this));
	dml_throw_exception(dml_exception::missing_attachment, errstr);
        return 1;
      }
      if(found->attr_type != DML_ATTRTYPE_LIST) {
	sprintf(errstr, ": _extends %s at %s", keypath, 
		PRIME_DML_LOCATION_STRING(this));
	dml_throw_exception(dml_exception::nonlist_attachment, errstr);
        return 1;
      }

      // Set up the reference link. It's possible that a circle of
      // reference may be created here. We postpone its detection
      // until a bit later.
      assert(found != this);
      attr_expand = found->clone();

    } else if(key_type() == AttrKey::DML_KEYTYPE_FIND) {

      char* keypath = (char*)attr_value_string;
      // only static key path is allowed here
      AttrNode* found = attr_parent->search_attribute(keypath, root);
      if(!found) { 
	sprintf(errstr, ": _find %s at %s", keypath, 
		PRIME_DML_LOCATION_STRING(this));
	dml_throw_exception(dml_exception::missing_attachment, errstr);
        return 1;
      }

      // set up the link
      assert(found != this);
      attr_expand = found->clone();

    } else {
      // Ignore the attribute. Simply marked the attribute as it has
      // been expanded (by pointing to itself).
      attr_expand = this;
    }
  }

  return 0;
}

void AttrNode::find_attributes(char* keypath, std::vector<AttrNode*>& collected, 
                               int onematch, AttrNode* root)
{
  assert(attr_type == DML_ATTRTYPE_LIST);
  assert(keypath);

  char* mykey = 0;

  // search for the tree root if it's not provided
  if(!root) {
    root = this;
    while(root->attr_parent) 
      root = root->attr_parent;
  }

  if(*keypath == 0) goto clear_and_return; // hit the end
  else if(*keypath == '.') { // absolute path
    keypath++;
    if(*keypath == '.') goto clear_and_return; // consecutive '.' not allowed
    else root->find_attributes(keypath, collected, onematch, root);
  } else { // relative path
    // make a string before the next '.' or the end
    mykey = new char[strlen(keypath)+1]; assert(mykey);
    register int i;
    for(i=0; keypath[i] != 0 && keypath[i] != '.'; i++)
      mykey[i] = keypath[i];
    mykey[i] = 0;

    // no keyword allowed in key path
    if(!strcasecmp(mykey, "_extends") ||
       !strcasecmp(mykey, "_find") ||
       !strcasecmp(mykey, "_schema")) {
      goto clear_and_return;
    }

    // adjust the key path for the next level
    char* newkeypath = keypath+strlen(mykey);
    if(*newkeypath == '.') newkeypath++;
    if(*newkeypath == '.') { // consecutive '.' not allowed
      goto clear_and_return;
    }

    // search for each child attribute
    if(attr_value_list) {
      for(std::vector<AttrNode*>::iterator iter = attr_value_list->begin();
          iter != attr_value_list->end(); iter++) {
        if((*iter)->key_type() == AttrKey::DML_KEYTYPE_EXTENDS) {
          assert((*iter)->attr_expand); // has been expanded normally
          assert((*iter)->attr_expand->attr_type == DML_ATTRTYPE_LIST);
          (*iter)->attr_expand->find_attributes(keypath, collected, onematch, root);
	  if(onematch && !collected.empty()) goto clear_and_return;
        } else if((*iter)->key_type() == AttrKey::DML_KEYTYPE_FIND) {
          assert((*iter)->attr_expand); // has been expanded normally
          assert((*iter)->attr_expand->attr_key); // cannot be the root
          if(*newkeypath == 0 && // must be the very last in the keypath
             !my_wc_strncmp(mykey, (*iter)->attr_expand->attr_key->ident())) {
            collected.push_back((*iter)->attr_expand);
	    if(onematch) goto clear_and_return;
          }
        } else if((*iter)->key_type() == AttrKey::DML_KEYTYPE_IDENT &&
                  !my_wc_strncmp(mykey, (*iter)->attr_key->ident())) {
          if(*newkeypath == 0) { // match to the very last key
            collected.push_back(*iter);
	    if(onematch) goto clear_and_return;
          } else if((*iter)->attr_type == DML_ATTRTYPE_LIST) { // there's more to match
            (*iter)->find_attributes(newkeypath, collected, onematch, root);
	    if(onematch && !collected.empty()) goto clear_and_return;
          }
        }
      }
    }
  }

 clear_and_return:
  if(mykey) delete[] mykey;
}

const char* AttrNode::key() const
{
  if(attr_key) // non-root must have a key
    return attr_key->ident();
  else return 0; // root does not have a key
}

const char* AttrNode::value() const
{
  if(attr_type == DML_ATTRTYPE_STRING)
    return attr_value_string;
  else return 0; // list attribute does not have a value
}

int AttrNode::traverse_recursion()
{
  assert(attr_expand); // the node should have been expanded already!

  expanding = 1;
  if(key_type() == AttrKey::DML_KEYTYPE_FIND ||
     key_type() == AttrKey::DML_KEYTYPE_EXTENDS) {
    if(attr_expand->expanding) {
      sprintf(errstr, "at %s", PRIME_DML_LOCATION_STRING(this));
      dml_throw_exception(dml_exception::recursive_expansion, errstr);
      return 1;
    }
    return attr_expand->traverse_recursion();
  } else {
    if(key_type() != AttrKey::DML_KEYTYPE_SCHEMA &&
       attr_type == DML_ATTRTYPE_LIST && attr_value_list) {
      for(std::vector<AttrNode*>::iterator iter = attr_value_list->begin();
          iter != attr_value_list->end(); iter++) {
        if((*iter)->key_type() == AttrKey::DML_KEYTYPE_SCHEMA) continue;
        if((*iter)->traverse_recursion()) return 1;
      }
    }
  }
  expanding = 0;

  return 0;
}

void AttrNode::print(FILE* fp, int space, int noexpand)
{
  if(!noexpand && attr_expand &&
     key_type() == AttrKey::DML_KEYTYPE_FIND) {
    attr_expand->print(fp, space, noexpand);
  } else if(!noexpand && attr_expand &&
            key_type() == AttrKey::DML_KEYTYPE_EXTENDS) {
    if(attr_expand->attr_value_list) {
      for(std::vector<AttrNode*>::iterator iter =
          attr_expand->attr_value_list->begin();
          iter != attr_expand->attr_value_list->end(); iter++) {
        (*iter)->print(fp, space, noexpand);
      }
    }
  } else {
    if(attr_key) { attr_key->print(fp, space); }
    else { if(space>0) fprintf(fp, "%*c", space, ' '); }
    if(attr_type == DML_ATTRTYPE_LIST) {
      noexpand |= (key_type() == AttrKey::DML_KEYTYPE_SCHEMA);
      if(attr_key) fprintf(fp, " [");
      if(attr_value_list) {
        if(attr_key) fprintf(fp, "\n");
        for(std::vector<AttrNode*>::iterator iter = attr_value_list->begin();
            iter != attr_value_list->end(); iter++) {
          if(attr_key) (*iter)->print(fp, space+2, noexpand);
          else (*iter)->print(fp, space, noexpand);
        }
      }
      if(attr_key) {
        if(space>0) fprintf(fp, "%*c", space, ' ');
        fprintf(fp, "]\n");
      }
    } else fprintf(fp, "\t\"%s\"\n", attr_value_string);
  }
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
