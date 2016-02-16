/**
 * \file dmlobj.cc
 * \brief Source file for the DmlObject class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/dmlobj.h"

namespace s3f {
namespace s3fnet {

DmlObject::DmlObject(DmlObject* parent, long myid) :
  id(myid), myParent(parent)
{
  // the nhi address is not settled at this juncture; it is expected
  // the subclass should take care of it during configuration
}

DmlObject::~DmlObject() {}

void DmlObject::display(int indent)
{
  // it is expected that this method will be overridden
  printf("%*cDmlObject %lu", indent, ' ', id);
}

}; // namespace s3fnet
}; // namespace s3f
