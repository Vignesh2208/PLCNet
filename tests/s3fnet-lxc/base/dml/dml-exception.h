/**
 * \file dml-exception.h
 * \brief DML exception handler.
 */

#ifndef __PRIME_DML_EXCEPTION_H__
#define __PRIME_DML_EXCEPTION_H__

#include <exception>

namespace s3f {
namespace dml {

/**
 * \brief DML exceptions that may be thrown by the DML library.
 *
 * The DML library throws exceptions when errors are encountered. The
 * user is expected to catch these exceptions when invoking the
 * public function through the DML API if the user wants to handle the
 * errors in a customed fashion, rather than simply printing the error
 * messages and quitting the program.
 */
class dml_exception : public virtual std::exception {
 public:
  /// All DML execeptions are defined here.
  enum exception_type_t {
    no_exception,		///< no exception or uninitialized exception
    other_exception,		///< unknown derived exception
    open_dml_file,		///< cannot open DML file
    parse_error,		///< parse error
    illegal_attribute_key,	///< illegal attribute key
    missing_attachment,		///< attachment not found
    nonlist_attachment,		///< not a list attachment
    recursive_expansion,	///< recursive expansion
    total_exceptions		///< total number of exceptions defined in this class
  };

  /**
   * \brief The constructor, typically called by the dml_throw_exception() method.
   * \param c is the error code.
   * \param s is a string, which will be appended to the standard error message.
   */
  dml_exception(int c, const char* s);

  /// Returns the error code.
  virtual int type() { return code; }

  /// Returns the error message of the exception.
  virtual const char* what() const throw() {
    return explanation;
  }

 protected:
  dml_exception() : code(no_exception) { explanation[0] = 0; }

  int code;
  char explanation[512];
}; /*class dml_exception*/

/**
 * \brief Throw an exception of the given type and with a customized error message.
 * \param c is the error code (an enumerated value of exception_type_t).
 * \param s is the customized message that will be appended to the standard error string.
 */
inline void dml_throw_exception(int c, const char* s = 0) { throw dml_exception(c, s); }

}; // namespace dml
}; // namespace s3f

#endif /*__PRIME_DML_EXCEPTION_H__*/

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
