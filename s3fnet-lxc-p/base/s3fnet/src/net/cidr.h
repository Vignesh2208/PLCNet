/**
 * \file cidr.h
 * \brief Header file for the CidrBlock class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef _CIDR_BLOCK_H_
#define _CIDR_BLOCK_H_

#include "util/shstl.h"
#include "net/ip_prefix.h"

namespace s3f {
namespace s3fnet {

class NameService;

/**
 * \brief A block of IP addresses.
 *
 * We use the Classless Inter-Domain Routing (CIDR) for IP address assignment.
 * The CidrBlock class represents the hierarchical structure of the assigned IP addresses.
 */
class CidrBlock {
  friend class NameService;

 protected:
  typedef S3FNET_MAP(long, CidrBlock*) CidrTable;

  /** IP prefix for this block. */
  IPPrefix prefix;

  /** A mapping from id to CIDR sub-blocks. */
  CidrTable sub_blocks;

 public:
  /** The default constructor. */
  CidrBlock() {}

  /** The constructor with a set ip prefix. */
  CidrBlock(unsigned addr, int len) : 
    prefix(addr, len) {}

  /** The destructor. */
  virtual ~CidrBlock();

  /** Set the ip prefix of this CIDR block. */
  void setPrefix(unsigned addr, int len);

  /** Find a CIDR sub-block of a given id. */
  CidrBlock* getSubCidrBlock(long id);

  /** Add a CIDR sub-block with a given id. */
  void addSubCidrBlock(long id, CidrBlock* cb);

  /** Get the prefix of this CIDR block. */
  const IPPrefix& getPrefix() { return prefix; }

  /** Display the CIDR out to the given file stream. */
  void display(FILE* fp);
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__CIDR_BLOCK_H__*/
