/**
 * \file errhandle.h
 * \brief Header file for internal error handling functions.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __ERRHANDLE_H__
#define __ERRHANDLE_H__

#include <stdarg.h>

namespace s3f {
namespace s3fnet {

  /**
   * An error handler is a function that consists of three arguments:
   * the first indicates whether the error is caused by a system call
   * or not, the second argument is the format string (like the one in
   * printf), and the last one is an argument list for the format
   * string.
   */
  typedef void (*ErrorHandler)(int syserr, const char* msg, va_list ap);

  /**
   * Set the error handler, which is a user-defined error handling
   * function, and return the old handler (NULL if undefined
   * previously).
   */
  extern ErrorHandler set_errhandler(ErrorHandler h);

  /**
   * This function is called when there's a fatal error related to a
   * system call. It's different from the syserr_dump function: this
   * function does not dump core. The function simply prints the error
   * message and terminates the program.
   */
  extern void syserr_quit(const char*, ...);

  /**
   * This function is called when there's a fatal error related to a
   * system call. The function prints the error message, dumps the
   * core, and then terminates the program.
   */
  extern void syserr_dump(const char*, ...);

  /**
   * This function is called when there's a non-fatal error related to
   * a system call. The function simply prints the message and returns
   * immediately. It can be used to print a warning message.
   */
  extern void syserr_retn(const char*, ...);

  /**
   * This function is called when there's a fatal error unrelated to a
   * system call. It's different from the error_dump function: this
   * function does not dump core. The function simply prints the error
   * message and terminates the program.
   */
  extern void error_quit(const char*, ...);

  /**
   * This function is called when there's a fatal error unrelated to a
   * system call. The function prints the error message, dumps the
   * core, and then terminates the program.
   */
  extern void error_dump(const char*, ...);

  /**
   * This function is called when there's a non-fatal error unrelated
   * to a system call. The function simply prints the message and
   * returns immediately. It can be used to print a warning message.
   */
  extern void error_retn(const char*, ...);

}; // namespace s3fnet
}; // namespace s3f

#endif /*__ERRHANDLE_H__*/
