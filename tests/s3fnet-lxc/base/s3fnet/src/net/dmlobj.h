/**
 * \file dmlobj.h
 * \brief Header file for the DmlObject class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __DML_OBJECT_H__
#define __DML_OBJECT_H__

#include "dml.h"
#include "net/nhi.h"

namespace s3f {
namespace s3fnet {

/** The number of white spaces used by indentation for printing. */
#define DML_OBJECT_INDENT 2

/** 
 * \brief An abstract base class for all DML identifiable objects.
 */
class DmlObject : public s3f::dml::Configurable {
 public:
  /** The constructor. */
  DmlObject(DmlObject* parent, long id = -1);

  /** The destructor. */
  virtual ~DmlObject();

  /**
   * Print out the name of this DmlObject to the standard output.
   * This function should be overridden by the subclasses. For
   * example, a host should print something like "Host 3". The
   * parameter indent indicates the number of white spaces to be used
   * before the name is printed out. This is so that we can print out
   * the entire network with correct indentations. (Maybe we should
   * print out the name to an arbitrary file stream. But it's not yet
   * useful at this point).
   */
  virtual void display(int indent=0);

  /** Every DmlObject object must have an ID obtained from the DML. */
  long id;

  /** The nhi address to identify the object. */
  Nhi nhi;

  /** The parent. DML objects are nested. */
  DmlObject* myParent;
};

}; // namespace s3fnet
}; // namespace s3f

#endif // __DML_DMLOBJECT_H__
