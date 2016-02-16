/*
 * dml-dictionary.h :- dictionary for DML strings.
 *
 * The DML library maintains a dictionary for all strings encountered
 * during parsing. The DML tree structure only contains pointers to
 * the strings in the dictionary. A reference counter scheme is used
 * to save memory in case of duplicate strings.
 */

#ifndef __PRIME_DML_DICTIONARY_H__
#define __PRIME_DML_DICTIONARY_H__

#include <map>
#include <string>

namespace s3f {
namespace dml {

  /*
   * Dictionary is used to keep track of all strings created during
   * parsing. These strings are maintaining using a reference counter
   * scheme.
   */
  class Dictionary {
  public:
    // Constructor: do nothing.
    Dictionary() {}

    // Destructor: reclaim all strings stored in the dictionary.
    ~Dictionary();

    /*
     * This method enters the string into the dictionary.  If there
     * exists a references to the same string in the dictionary, we
     * only increment the reference count of this string. The pointer
     * to the string stored in the dictionary is returned.
     */
    const char* enter_string(const char*);

    /*
     * Decrement the reference count of the string by one.  If the
     * reference count reaches zero, it means no one's using the
     * string and therefore the string should be reclaimed. This
     * method returns a pointer to the string if the its reference
     * counter is still greater than zero.  Otherwise, NULL is
     * returned.
     */
    const char* scrap_string(const char*);

  private:
    // This is a map from string to reference count. We use a map
    // here for fast lookup. [FUTURE: we may use a hash table to
    // further improve its performance.]
    std::map<std::string,int> lexicon;
  }; /*Dictionary*/

}; // namespace dml
}; // namespace s3f

#endif /*__PRIME_DML_DICTIONARY_H__*/

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
