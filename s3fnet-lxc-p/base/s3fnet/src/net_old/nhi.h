/**
 * \file nhi.h
 * \brief Header file for the Nhi class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __NHI_H__
#define __NHI_H__

#include "dml.h"
#include "util/shstl.h"

namespace s3f {
namespace s3fnet {

/**
 * \brief The NHI address used in DML.
 * 
 * NHI stands for Network Host Interface. This NHI class provides
 * utility functions that process NHI address and handle conversion of
 * NHI addresses to and from character strings.
 */ 
class Nhi {
 public:
  /** Type of the NHI address. */
  enum {
    NHI_INTERFACE	= 1, ///< network interface type
    NHI_MACHINE		= 2, ///< host or router type
    NHI_NET		= 4, ///< a sub-network type
    NHI_INVALID		= 8  ///< invalid type
  };

  /** The constructor. */
  Nhi();

  /** The constructor. */
  Nhi(char* str, int type);

  /** The constructor. */
  Nhi(Nhi& rhs);

  /** The destructor. */
  virtual ~Nhi();
  
  /** Override the assign operator. */
  Nhi& operator=(Nhi& rhs);

  /** Override the += operator. Append an id to the end of the NHI. */
  Nhi& operator+=(int id);

  /** Override the == operator. */
  friend bool operator==(Nhi& n1, Nhi&n2);

  /**
   * Returns a char array representation of this NHI. The buffer
   * pointed to by buf MUST have enough storage space.
   */
  char* toString(char* buf);

  /**
   * The method is the same as the method with the same name except
   * that it uses an internal static buffer.
   */
  char* const toString();

  /**
   * Returns an STL string representation of this NHI.
   */
  S3FNET_STRING toStlString();

  /**
   * Prints out this NHI address to standard output. (Maybe we can
   * make this more generic to any file streams; but the necessity
   * never arised.)
   */
  void display();

  /**
   * Convert from a string representation. The type specifies what
   * kind of NHI this is so that we can do some error checking. This
   * method return non-zero if the conversion fails.
   */
  int convert(char* str, int type);

  /**
   * Checks whether this NHI address contains the given NHI.
   */
  bool contains(Nhi& nhi);

  /**
   * Checks whether this NHI address contains the given NHI given as a
   * string.
   */
  bool contains(char* nhiAddr);

  /**
   * Function to read the contents of an nhi_range attribute into a
   * vector. The caller is responsible to release the memory that is
   * allocated in this function for the pVector.
   * 
   * Example of the DML that it handles:
   * <code>
   *    [port 10 nhi_range [from 1:2(0) to 1:5(0)]
   * </code>
   */
  static int readNhiRange(S3FNET_POINTER_VECTOR& pVector,
			  s3f::dml::Configuration* cfg);

  /**
   * Changes the NHI so that it's given relative to the parameter.
   * For this to work, parent must contain this NHI address.
   */
  void setRelativeTo(Nhi& parent);

  /**
   * Vector of successive ids. The last one could be the interface id.
   */
  S3FNET_LONG_VECTOR ids;
  
  /**
   * This is an index into ids. It is a start position of the NHI.
   * Useful for altering the NHI during address resolution. @see
   * Net::Resolve.
   */
  int start;

  /** The type of the NHI address. */
  int type;

 private:
  static char internal_buf[50];
};

}; // namespace s3fnet
}; // namespace s3f

#endif // __DML_NHI_H__
