/**
 * \file cidr.cc
 * \brief Source file for the CidrBlock class.
 *
 * authors : Dong (Kevin) Jin
 */

#include "net/cidr.h"
#include "util/errhandle.h"


namespace s3f {
namespace s3fnet {

CidrBlock::~CidrBlock()
{
  if (sub_blocks.size()) {
    for (CidrTable::iterator iter = sub_blocks.begin();
         iter != sub_blocks.end(); iter++) {
      delete iter->second;
    }
  }
}

CidrBlock* CidrBlock::getSubCidrBlock(long id)
{
  CidrTable::iterator iter = sub_blocks.find(id);
  if (iter == sub_blocks.end())
    return NULL;
  return iter->second;
}

void CidrBlock::addSubCidrBlock(long id, CidrBlock* cb)
{
  assert(cb);
  if (!sub_blocks.insert(S3FNET_MAKE_PAIR(id, cb)).second) {
    error_quit("ERROR: CidrBlock::addSubCidrBlock(), "
	       "CIDR id exists for id %lu\n", id);
  }
}

void CidrBlock::setPrefix(unsigned addr, int len)
{
  prefix.addr = addr;
  prefix.len = len;
}

void CidrBlock::display(FILE* fp)
{
  fprintf(fp, "CIDR block, net prefix = %s/%d\n",
          IPPrefix::ip2txt(prefix.addr), prefix.len);
}

}; // namespace s3fnet
}; // namespace s3f
