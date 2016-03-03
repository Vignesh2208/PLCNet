/*
 * dml-locinfo.h :- location information of DML symbols.
 *
 * The location information contains file name, line number, and
 * column number, for each parsed symbols in DML. The location
 * information is only used when the macro PRIME_DML_LOCINFO is
 * defined as we compile this code.
 */

#ifndef __PRIME_DML_LOCINFO_H__
#define __PRIME_DML_LOCINFO_H__

namespace s3f {
namespace dml {

#define DML_LOCINFO_FILEPOS

  /*
   * This class is a wrapper data structure. The implementation is
   * postponed in the source file.
   */
  class LocInfo {
  public:
    LocInfo();
#ifdef DML_LOCINFO_FILEPOS
    LocInfo(const char* f, int l, int o, int spos, int epos);
#else
    LocInfo(const char* f, int l, int o);
#endif
    virtual ~LocInfo();

    void set(const LocInfo& locinfo);
#ifdef DML_LOCINFO_FILEPOS
    void set_first(const LocInfo* locinfo);
    void set_last(const LocInfo* locinfo);
    void set(const char* f, int l, int o, int spos, int epos);
#else
    void set(const char* f, int l, int o);
#endif

    const char* filename() const;
    int lineno() const;
    int offset() const;
#ifdef DML_LOCINFO_FILEPOS
    int startpos() const;
    int endpos() const;
#endif

    /*
     * Returns a string containing the file name, line number, and the
     * column number of this symbol. The returned string uses the
     * buffer passed as argument to this method. The user is
     * responsible to make sure that the buffer size is large
     * enough. If the buffer is NULL, the method creates a buffer for
     * the user. The memory is expected to be reclaimed by the user
     * (using 'delete[]' operator) afterwards.
     */
    char* toString(char* buf = 0, int bufsiz = 0) const;

    /*
     * Returns true if the two locations are the same. That is, their
     * file names, line numbers, column numbers are all identical, 
     * respectively. The locations are different if one of file names
     * is unspecified.
     */
    friend int operator ==(const LocInfo& loc1, const LocInfo& loc2);
    friend int operator !=(const LocInfo& loc1, const LocInfo& loc2) {
      return !(loc1 == loc2);
    }

  private:
    void* locinfo_impl;
  }; // class LocInfo

}; // namespace dml
}; // namespace s3f

#endif /*__PRIME_DML_LOCINFO_H__*/

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
