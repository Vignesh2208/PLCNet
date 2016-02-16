/**
 * \file errhandle.cc
 * \brief Source file for internal error handling functions.
 *
 * authors : Dong (Kevin) Jin
 */

#include "util/errhandle.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


namespace s3f {
namespace s3fnet {

#if defined(_WIN32) || defined(_WIN64)
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

// printf format string buffer size
#define S3FNET_ERRHANDLE_PRTFMT_BUFSIZ 4096

// this function is of s3fnet::ErrorHandler type
static void default_errhandler(int syserr, const char* fmt, va_list ap)
{
   // Stack memory instead of heap memory is desired here as the error
   // could come from the memory system. However, this could lead to
   // buffer overflow problem if not dealt with carefully. We must
   // truncate the error message if there's a buffer overflow.
  char buf[S3FNET_ERRHANDLE_PRTFMT_BUFSIZ];

  if(fmt) {
    // We must save the error number in case it's overwritten by
    // other system calls in this routine.
    int errno_save = errno;

    // Write the message to the buffer.
    vsnprintf(buf, S3FNET_ERRHANDLE_PRTFMT_BUFSIZ, fmt, ap);
    buf[S3FNET_ERRHANDLE_PRTFMT_BUFSIZ-1] = 0; // safety
    int offset = strlen(buf);

    // Append system error messages if this is related to a system
    // call. Applicable only if there's enough buffer space.
    if(syserr && offset < S3FNET_ERRHANDLE_PRTFMT_BUFSIZ-1) {
      snprintf(buf+offset, S3FNET_ERRHANDLE_PRTFMT_BUFSIZ-1-offset,
	       ": %s", strerror(errno_save));
      buf[S3FNET_ERRHANDLE_PRTFMT_BUFSIZ-1] = 0; // safety
      offset = strlen(buf);
    }

    // We add a newline at the end of the message. So, no need to add
    // a newline in the format string!
    if(offset < S3FNET_ERRHANDLE_PRTFMT_BUFSIZ-1)
      strcat(buf, "\n");

    // Flush all file streams, including stdout, so that regular
    // messages are printed out before the error message.
    fflush(0);

    // Print out error message to stderr.
    fputs(buf, stderr); 

    // Flush one more time.
    fflush(0);
  }
}

// Global variables must be dealt with caution in s3fnet, since they may
// not be shared by all processes in a multi-processor environment.
// Actually there are two possible cases. In the first case, there is
// one instance of a global variable defined for each process. The
// global data segment is not shared, but copied instead at the moment
// of spawning child processes. In the second case, there is exactly
// one variable shared by all processors, unprotected from
// simultaneous access. We initialize the global error handler to be
// the default one. We must make sure that, if we want to replace it
// with a user-defined error handler, it must be done BEFORE the child
// processes are created. Another consequence is that the error
// handler may not be available at the C global initialization
// phase. Also it is possible some global variables may be initialized
// before this assignment happens. In any case we assume the handler
// is NULL initially. But if it's not, we'll be in trouble!
static ErrorHandler errhandler = default_errhandler;

// Must call before child processes are created (see above).
ErrorHandler set_errhandler(ErrorHandler h)
{
  ErrorHandler hd = errhandler;
  errhandler = h;
  return hd;
}

void syserr_quit(const char* fmt, ...)
{
  if(errhandler) {
    va_list ap;
    va_start(ap, fmt);
    (*errhandler)(1, fmt, ap);
    va_end(ap);
  }
  exit(1);
}

void syserr_dump(const char* fmt, ...)
{
  if(errhandler) {
    va_list ap;
    va_start(ap, fmt);
    (*errhandler)(1, fmt, ap);
    va_end(ap);
  }
  abort(); // will dump core
}

void syserr_retn(const char* fmt, ...)
{
  if(errhandler) {
    va_list ap;
    va_start(ap, fmt);
    (*errhandler)(1, fmt, ap);
    va_end(ap);
  }
}

void error_quit(const char* fmt, ...)
{
  if(errhandler) {
    va_list ap;
    va_start(ap, fmt);
    (*errhandler)(0, fmt, ap);
    va_end(ap);
  }
  exit(1);
}

void error_dump(const char* fmt, ...)
{
  if(errhandler) {
    va_list ap;
    va_start(ap, fmt);
    (*errhandler)(0, fmt, ap);
    va_end(ap);
  }
  abort(); // will dump core
}
void error_retn(const char* fmt, ...)
{
  if(errhandler) {
    va_list ap;
    va_start(ap, fmt);
    (*errhandler)(0, fmt, ap);
    va_end(ap);
  }
}

}; // namespace s3fnet
}; // namespace s3f
