/**
 * dml-dictionary.cc :- source code implementing the dictionary for
 *                      DML strings.
 *
 * The DML library maintains a dictionary for all strings encountered
 * during parsing. The DML tree structure only contains pointers to
 * the strings in the dictionary. A reference counter scheme is used
 * to save memory in case of duplicate strings.
 */

#ifndef __PRIME_DML_CONFIG_H__
#define __PRIME_DML_CONFIG_H__
#include "dml-config.h"
#endif /*__PRIME_DML_CONFIG_H__*/

#include <assert.h>
#include <stdio.h>

#include "dml-dictionary.h"

namespace s3f {
namespace dml {

Dictionary::~Dictionary()
{
  if(lexicon.size() > 0) {
    for(std::map<std::string,int>::iterator iter = lexicon.begin();
	iter != lexicon.end(); iter++) {
      fprintf(stderr, "leftover: %s, %d\n", (*iter).first.c_str(), (*iter).second);
    }
    lexicon.clear();
  }
}

const char* Dictionary::enter_string(const char* str)
{
  if(!str) return 0;
  std::map<std::string,int>::iterator iter = lexicon.find(str);
  if(iter != lexicon.end()) {
    (*iter).second++;
    return (*iter).first.c_str();
  } else {
    std::pair<std::map<std::string,int>::iterator,bool> iiter =
      lexicon.insert(std::make_pair(str,1));
    assert(iiter.second);
    return (*iiter.first).first.c_str();
  }
}

const char* Dictionary::scrap_string(const char* str)
{
  if(!str) return 0;
  std::map<std::string,int>::iterator iter = lexicon.find(str);
  assert(iter != lexicon.end());
  if((*iter).second-- > 1) {
    return (*iter).first.c_str();
  } else {
    lexicon.erase(iter);
    return 0;
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
