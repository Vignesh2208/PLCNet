/**
 * \file namesvc.h
 * \brief Header file for the NameService class.
 *
 * authors : Dong (Kevin) Jin
 */

#ifndef __NAMESVC_H__
#define __NAMESVC_H__

#include "dml.h"
#include "net/cidr.h"

namespace s3f {
namespace s3fnet {

class Net;
class IPPrefix;
class Host;
class NetworkInterface;
class Mac48Address;

/**
 * \brief Global name resolution service.
 *
 * This class keeps global information. It can translate between host
 * name, ip address, nhi address, and the alignment names.
 */
class NameService {
  friend class Net;

 public:
  typedef S3FNET_MAP(S3FNET_STRING, S3FNET_STRING) STR2STRMAP;
  typedef S3FNET_MAP(S3FNET_STRING, IPADDR) NHI2IPMAP;
  typedef S3FNET_MAP(S3FNET_STRING, int) NHI2IDMAP;
  typedef S3FNET_MAP(IPADDR, S3FNET_STRING) IP2NHIMAP;
  typedef S3FNET_MAP(IPADDR, NetworkInterface*) IP2IFACE_MAP;

  /** Resolve host name to alignment name. */
  const char* hostname2align(const string& str);

  /** Resolve Host Nhi or NIC Nhi (which is converted to Host Nhi) to alignment name. */
  const char* nhi2align(const string& nhi);

  /** Resolve host name to IPADDR. */
  IPADDR hostname2ip(const S3FNET_STRING& str);

  /** Resolve Host or NIC Nhi to IPADDR. */
  IPADDR nhi2ip(const S3FNET_STRING& nhi);

  /** Resolve Host Nhi or NIC Nhi (converted to Host Nhi) to host name, if defined. */
  const char* nhi2hostname(const S3FNET_STRING& nhi);

  /** Resolve IPADDR to the corresponding NIC Nhi. */
  const char* ip2nicnhi(IPADDR addr);

  /** Resolve IPADDR to alignment name. */
  const char* ip2align(IPADDR addr);

  /** Resolve Net Nhi to a prefix; return true if successful. */
  bool netnhi2prefix(const S3FNET_STRING& nhi, IPPrefix& prefix);

  /** Resolve Net Nhi to a prefix; return true if successful. */
  bool netnhi2prefix(const char* nhi, IPPrefix& prefix);

  /** Resolve IPADDR to the corresponding NetworkInterface object. */
  NetworkInterface* ip2iface(IPADDR addr);

  /** Resolve IPADDR to the corresponding Host object. */
  Host* ip2hostobj(IPADDR addr);

  /** Resolve NIC Nhi to the corresponding NetworkInterface object. */
  NetworkInterface* nicnhi2iface(const S3FNET_STRING& nhi);

  /** Resolve Host Nhi to the corresponding Host object. */
  Host* hostnhi2hostobj(const S3FNET_STRING& nhi);

  /** Resolve IPADDR to Mac48Address */
  Mac48Address* ip2mac48(IPADDR addr);

  /** Return the ip2iface_map */
  IP2IFACE_MAP* get_ip2iface_map() { return ip2iface_map; }

  /** Print the ip2iface_map */
  void print_ip2iface_map();

 protected:
  /** CIDR block for the entire network. */
  CidrBlock top_cidr_block;

  STR2STRMAP* hostname2nicnhi_map; ///< map from host name to nic NHI address
  STR2STRMAP* hostnhi2hostname_map; ///< map from host NHI address to host name
  STR2STRMAP* hostnhi2align_map; ///< map from host NHI to the name of the alignment it belongs
  NHI2IDMAP* hostnhi2id_map; ///< map from host NHI to index of its default interface
  NHI2IPMAP* nhi2ip_map; ///< map from nic NHI to IPADDR
  IP2NHIMAP* ip2nicnhi_map; ///< map from IPADDR to NHI
  IP2IFACE_MAP* ip2iface_map; ///< map from IP to NetworkInterface* object

  /** The constructor. */
  NameService();

  /** The destructor. */
  virtual ~NameService();

  /** Configure from DML. */
  virtual void config(s3f::dml::Configuration* cfg);

  // helper function for the config method
  void config_topnet_cidr(s3f::dml::Configuration* cfg);
  void config_net_cidr(s3f::dml::Configuration* cfg, CidrBlock* cidr);
  void config_hosts(s3f::dml::Configuration* cfg);
  void config_nhi_addr(s3f::dml::Configuration* cfg);
};

}; // namespace s3fnet
}; // namespace s3f

#endif /*__NAMESVC_H__*/
