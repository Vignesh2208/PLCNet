#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string.h>

#include "extra/getopt.h"

#if defined(_WIN32) || defined(_WIN64)
#define STRCASECMP _stricmp
#else
#include <strings.h>
#define STRCASECMP strcasecmp
#endif

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
#include <deque>

#ifdef DMALLOC
#include <dmalloc.h>
#endif

#include "dml.h"

using namespace std;
using namespace s3f::dml;

#define IPLENGTH 100
static char ipbuf[IPLENGTH];

/** a unique ID for host / router */
long unique_device_id = 0;

#ifdef USE_FORWARDING_TABLE
// a simple function mapping from a text into an
// unsigned int. The text should be a.b.c.d
static unsigned txt2ip(char* str, int* len)
{
  int dot_num=0;  // how many dot are met
  unsigned base = 0, incr = 0;

  for(int i=0; 
      isdigit(str[i]) || str[i] == '\0' || str[i] == '/' 
	|| (str[i] == '.' && (dot_num <= 2)); 
      i++)
    {
      if((str[i] == '.') || (str[i] == '\0') || (str[i] == '/')) {
	// base = 256 * base + incr; 
	base *= 256; 
	base += incr; 
	incr = 0; 
	
	// at the end of the string
	if(str[i] == '\0') {
          return base;
	}
	// then the end of the part should be the length of mask
	if(str[i] == '/') {
	  // we should make sure that len is not NULL
	  if(len) {
	    // the rest is the length
	    *len = atoi(str + i + 1);
	  }
          return base;
	}

	// another dot
	dot_num ++;
      } else { // isdigit(str[i])
	incr *= 10;
	incr += (str[i] - '0'); 
      }
    }
//   IPADDR_DUMP(printf("IPPrefix::txt2ip(), str is %s, "
//                      //"tmp is 0x%s,"
//                      "ip is 0x%x\n",
//                      str, base
//                      //,ntohl(base)
//                      ));
  return base;
}

typedef map<int, unsigned> IfacesMap;
typedef map<string, IfacesMap*> GlobalIfacesMap;
GlobalIfacesMap all_ifaces;
void read_in_forwarding_table(Configuration* cfg);
#endif

static void printsp(int sp) {
  for(int i=0; i<sp; i++) printf(" ");
}

static char* ip2str(unsigned ip) {
  unsigned a1 = ip>>24;
  unsigned a2 = (ip>>16)&0xff;
  unsigned a3 = (ip>>8)&0xff;
  unsigned a4 = ip&0xff;
  sprintf(ipbuf, "%u.%u.%u.%u", a1, a2, a3, a4);
  //fprintf(stderr, "### %u => %s\n", ip, ipbuf);
  return ipbuf;
}

static char* sstrdup(const char* s) {
  if(!s) return 0;
  char* x = new char[strlen(s)+1];
  strcpy(x, s);
  return x;
}

typedef map<string, string> DNSMap;
DNSMap dns;

////////////////////////////////////////////////////////
//
// code that are needed to generate environment info
//
struct EnvironmentInfoLocal {
  unsigned src_ip;
  double delay;
  int link_prefix_len;
  vector<string> local_peers;
  vector<string> remote_align;    
};

struct EnvironmentInfoRemote {
  unsigned src_ip;
  double delay;
  vector<string> local_peers;
  
  /** start of wireless stuff */
  float xpos;
  float ypos; 
  float zpos;
  /** end of wireless stuff */
};

struct EnvironmentMap {
  /// each alignment has an env map
  virtual ~EnvironmentMap();
  /// part of the map the stores the links that use local hosts as senders.
  map<string, EnvironmentInfoLocal*> localMap;
  /// part of the map that stores the links that use remote hosts as senders.
  map<string, EnvironmentInfoRemote*> remoteMap;
  ///
  void display(int sp);
};

static map<string, EnvironmentMap*> envMaps;

EnvironmentMap::~EnvironmentMap()
{
  for (map<string, EnvironmentInfoLocal*>::iterator iter = localMap.begin();
       iter != localMap.end(); iter++) {
    delete (*iter).second;
  }
  localMap.clear();

  for (map<string, EnvironmentInfoRemote*>::iterator iter = remoteMap.begin();
       iter != remoteMap.end(); iter++) {
    delete (*iter).second;
  }
  remoteMap.clear();
}

void EnvironmentMap::display(int sp)
{
  for (map<string, EnvironmentInfoLocal*>::iterator iter = localMap.begin();
       iter != localMap.end(); iter++) {
    printsp(sp);
    EnvironmentInfoLocal* info = (*iter).second;
    printf("interface [ nhi %s ip %s delay %g link_prefix_len %d",
           (*iter).first.c_str(), ip2str(info->src_ip), 
	   info->delay, info->link_prefix_len);
    for (unsigned i=0; i<info->local_peers.size(); i++) {
      printf(" peer %s ", info->local_peers[i].c_str());
    }
    for (unsigned i=0; i<info->remote_align.size(); i++) {
      printf(" peer_align %s ", info->remote_align[i].c_str());
    }
    printf("]\n");
  }

  // print remote peer info.
  for (map<string, EnvironmentInfoRemote*>::iterator iter=remoteMap.begin();
       iter != remoteMap.end(); iter++) {
    printsp(sp);
    EnvironmentInfoRemote* info = (*iter).second;
    printf("remote_sender [ nhi %s ip %s xpos %.4f ypos %.4f zpos %.4f",
           (*iter).first.c_str(), ip2str(info->src_ip),
           info->xpos, info->ypos, info->zpos);
    for (unsigned i=0; i<info->local_peers.size(); i++) {
      printf(" peer %s ", info->local_peers[i].c_str());
    }
    printf("]\n");
  }
}
//
// end of code for generating env info
//////////////////////////////////////////////////

#define CLUSTER_CIDRSIZE 0
#define CLUSTER_NNODES 0
#define CLUSTER_NLINKS 0
#define CLUSTER_ATTACH_ID 0
#define CLUSTER_ATTACH_NHI "0(0)"
#define CLUSTER_FNAME "default.xml"
#define CLUSTER_DELAY 0.005
/*#define CLUSTER_BANDWIDTH 1e9*/

static unsigned cluster_cidrsize = CLUSTER_CIDRSIZE;
static unsigned cluster_nnodes = CLUSTER_NNODES;
static unsigned cluster_nlinks = CLUSTER_NLINKS;
static int cluster_attach_id = CLUSTER_ATTACH_ID;
static char* cluster_attach_nhi = sstrdup(CLUSTER_ATTACH_NHI);
static char* cluster_fname = sstrdup(CLUSTER_FNAME);
static double cluster_delay = CLUSTER_DELAY;
/*static double cluster_bandwidth = CLUSTER_BANDWIDTH;*/
static unsigned cluster_prefix_length;

static int rtname = 0;

class Net;
class Router;
class Link;

// since we're using only one buffer for id string for display, the
// get_full_id function must be called one at any given time.
#define IDLENGTH 100
static char idbuf[IDLENGTH];

static Router* peering_router = 0;
static int peering_iface = 0;

static void iftype2labels(char* str, double& bitrate, double& latency) {
  if(!STRCASECMP(str, "0")) { bitrate = 1e8; latency = 0.003; }
  else if(!STRCASECMP(str, "aal5")) { bitrate = 2e7; latency = 0.003; }
  else if(!STRCASECMP(str, "ethernetCsmacd")) { bitrate = 1e8; latency = 0.003; }
  else if(!STRCASECMP(str, "fddi")) { bitrate = 1e8; latency = 0.003; }
  else if(!STRCASECMP(str, "gigabitEthernet")) { bitrate = 1e9; latency = 0.005; }
  else if(!STRCASECMP(str, "iso88023csmacd")) { bitrate = 1e8; latency = 0.003; }
  else if(!STRCASECMP(str, "other")) { bitrate = 1e7; latency = 0.005; }
  else if(!STRCASECMP(str, "ppp")) { bitrate = 1.5e6; latency = 0.01; }
  else if(!STRCASECMP(str, "propPointToPointSerial")) { bitrate = 5.6e4; latency = 0.05; }
  else if(!STRCASECMP(str, "propVirtual")) { bitrate = 1e8; latency = 0.003; }
  else if(!STRCASECMP(str, "regular1822")) { bitrate = 6.4e4; latency = 0.05; }
  else if(!STRCASECMP(str, "sonet")) { bitrate = 1e8; latency = 0.003; }
  else if(!STRCASECMP(str, "starLan")) { bitrate = 1e7; latency = 0.005; }
  else if(!STRCASECMP(str, "virtualIpAddress")) { bitrate = 1e8; latency = 0.003; }
  else {
    fprintf(stderr, "ERROR: unknown interface type: %s\n", str);
    exit(1);
  }
}

Net* topnet;

class cidrBlock {
public:
  unsigned ip_prefix;
  unsigned ip_prefix_length;

  cidrBlock() {}
  virtual ~cidrBlock() {}

  unsigned reserved();
  virtual void compute_prefix() = 0;
  virtual void assign_prefix(unsigned baseaddr) = 0;

  unsigned prefix();
};

unsigned cidrBlock::reserved() {
  if(ip_prefix_length > 32) return 0;
  else return (1<<(32-ip_prefix_length));
}

unsigned cidrBlock::prefix() {
  return ((ip_prefix>>(32-ip_prefix_length))<<(32-ip_prefix_length));
}

class Router
{
public:
  int is_host;
  int has_routes;

  Net* owner; // owner network (not NULL)
  long id;    // relative id
  char* name; // name of the router
  // the nhi of a connected iterface,
  // this router name is mapped to the nhi of this interface.
  char* i_nhi;
#ifdef S3FNET_ALIGN_ROUTER
  string align_str;
#endif
  map<int,Link*>  ifaces;
  map<int,double> bitrate;
  map<int,double> latency;
  map<int,string> iftype;

  map<int,pair<cidrBlock*,int> > if_ip;
  
  /* host position for wireless */
  float xpos;
  float ypos;
  float zpos;

  unsigned ipaddr(int);
//   unsigned ippref(int);
//   unsigned ippreflen(int);

  dmlConfig* graphcfg; // used when output DML
  dmlConfig* nhicfg;

  Router(Net* _owner, long _id);
  virtual ~Router();

  void config(Configuration* cfg);

  void print_dml(int sp);

  void generate_environment_info();
  void print_envdml(int sp, string& align);

  void print_xml();
  void print_iplist();

  char* get_full_id(Net* relnet = topnet);
  char* get_full_id(int iface_id, Net* relnet = topnet);

  Link* add_link(int lnkid, Link* lnk);

  long newid; // newly assigned router id
  long idseq; // this enumerates all routers defined
  int nifaces; // used to derive new interface ids
  map<int,int> ifaces_newid; // map from old to new interface id
  void assign_new_ifaces();
  char* get_new_full_id(Net* relnet = topnet);

  class next_hop_t {
  public:
    Router* next_router;
    int myiface;
    int youriface;
    next_hop_t(Router* r, int m, int y) : 
      next_router(r), myiface(m), youriface(y) {}
  };
  int single_iface;
  map<Router*,double> distance;
  map<Router*,next_hop_t*> nexthop;
  bool visited;
};

class Link : public cidrBlock {
public:
  //int id; // for debugging purpose!
  //static int link_id_so_far;

  Net* owner; // owner network 
  map<Router*,int> attachments; // router* => iface id
  set<pair<Router*,int> > detachments; // overlapped interfaces
  double delay;
  double bandwidth;

  Link(Net* _owner);
  virtual ~Link();

  void config(Configuration* cfg);

  void collect_stats(map<double,int>* bdwdist, map<double,int>* dlydist);

  void merge_with(Link* lnk);

  void print_dml(int sp);
  void print_xml();

  virtual void compute_prefix();
  virtual void assign_prefix(unsigned baseaddr);
};

class Net : public cidrBlock {
public:
  Net* owner; // owner network, NULL if this is top-most
  long id;    // network id (unused if this is top-most)
  string align_str; //alignment string, used to partition the network in parellel simulation.

  map<long,Net*> nets; // list of subnets
  map<long,Router*> routers; // list of routers
  set<Link*> links; // list of links

  map<pair<Router*,int>,long> clusters; // used for attaching hosts

  unsigned cidr_prefix_len; // Maximum prefix length of this Network.
  Net(long _id, string align); // used by the topnet.
  Net(Net* _owner, long _id);
  virtual ~Net();

  void config(Configuration* cfg);

  void collect_stats(map<double,int>* bdwdist, map<double,int>* dlydist);

  void print_dml(int sp);

  void generate_environment_info();
  void print_envdml(int sp);
  void print_host_envdml(int sp);
  void print_net_cidr_envdml(int sp);
  void print_xml();
  void print_xml_attaches();
  void print_xml_routers();
  void print_xml_links();
  void print_iplist();

  static  void nhistr2vec(string nhi, vector<int>& nhivec);
  static bool nhiset_contains(set<string>* dstset, string id);
  void populate_routables(set<string>* dstset, set<Router*>& routables, set<Router*>& allrts);
  void print_ospfroutes();

  Router* find_router(char* str);
  void remove_link(Link*);
  char* get_full_id(Net* relnet = topnet);

  long newid; // newly assigned network id
  long nrouters; // used to derive new router/subnet ids
  void assign_new_ifaces();
  char* get_new_full_id(Net* relnet = topnet);

  vector<cidrBlock*> myvec;

  virtual void compute_prefix();
  virtual void assign_prefix(unsigned baseaddr);
};

static set<string> alignStrSet;

static int total_routers = 0;
static int total_links = 0;
static int total_nets = 0;

Router::Router(Net* _owner, long _id) : 
  is_host(0), has_routes(0), owner(_owner), id(_id),
  name(NULL), i_nhi(NULL)
#ifdef S3FNET_ALIGN_ROUTER
  ,align_str("")
#endif
{
  idseq = total_routers++;
  nifaces = 0;
}

Router::~Router() 
{ 
  if (name) delete[] name; 
  if (i_nhi) delete[] i_nhi;
}

void Router::config(Configuration* cfg) {
  graphcfg = (dmlConfig*)cfg->findSingle("GRAPH");
  nhicfg = (dmlConfig*)cfg->findSingle("NHI_ROUTE");
  if(nhicfg) has_routes = 1;

  char* str = (char*)cfg->findSingle("NAME");
  if (str) name = sstrdup(str);
  else {
    // check dns map
    string nhi = get_full_id();
    DNSMap::iterator iter = dns.find(nhi);
    if (iter != dns.end())
      name = sstrdup((char*)((*iter).second.c_str()));
    else name = sstrdup("");
  }

#ifdef S3FNET_ALIGN_ROUTER
  str = (char*)cfg->findSingle("alignment");
  if (str) align_str = str;
  else align_str = owner->align_str;
  if (alignStrSet.find(align_str) == alignStrSet.end()) {
    // a new alignment
    alignStrSet.insert(align_str);
  }
#endif

  /* host position for wireless mobility */
  str = (char*)cfg->findSingle("xpos");
  if(str) xpos = atof(str); else xpos = 0;
  str = (char*)cfg->findSingle("ypos");
  if(str) ypos = atof(str); else ypos = 0;
  str = (char*)cfg->findSingle("zpos");
  if(str) zpos = atof(str); else zpos = 0;

  Enumeration* ss = cfg->find("INTERFACE");
  while(ss->hasMoreElements()) {
    Configuration* icfg = (Configuration*)ss->nextElement();
    
    double bps, lat; bps = lat = 0.0;
    str = (char*)icfg->findSingle("TYPE");
    char* typstr = str ? sstrdup(str) : 0;
    if(str && STRCASECMP(str, "peering")) {
      iftype2labels(str, bps, lat);
    } else {
      str = (char*)icfg->findSingle("BITRATE");
      if(str) bps = atof(str);
      str = (char*)icfg->findSingle("LATENCY");
      if(str) lat = atof(str);
    }

    str = (char*)icfg->findSingle("id");
    if(str) { 
      int idd = atoi(str);
      if(!ifaces.insert(make_pair(idd, (Link*)0)).second) {
        fprintf(stderr, "ERROR: duplicate interface id %d in ROUTER id=%s\n",
                idd, get_full_id());
        exit(1);
      }
      bitrate.insert(make_pair(idd, bps));
      latency.insert(make_pair(idd, lat));
      if(typstr) {
        iftype.insert(make_pair(idd, typstr));
        if(!STRCASECMP(typstr, "peering")) {
          if(peering_router) { 
            fprintf(stderr, "ERROR: multiple peering points found\n"); 
            exit(1); 
          }
          peering_router = this;
          peering_iface = idd;
        }
      }
    } else {
      Configuration* idcfg = (Configuration*)icfg->findSingle("idrange");
      if(!idcfg) {
        if(owner) fprintf(stderr, "ERROR: missing INTERFACE.ID or INTERFACE.IDRANGE in ROUTER id=%s\n",
                          get_full_id());
        else fprintf(stderr, "ERROR: missing INTERFACE.ID or INTERFACE.IDRANGE\n");
        exit(1);
      }
      str = (char*)idcfg->findSingle("FROM");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.FROM\n"); exit(1); }
      int idfrom = atoi(str);
      str = (char*)idcfg->findSingle("TO");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.TO\n"); exit(1); }
      int idto = atoi(str);
      if(idfrom > idto) { fprintf(stderr, "ERROR: invalid IDRANGE\n"); exit(1); }

      for(int idd=idfrom; idd<=idto; idd++) {
        if(!ifaces.insert(make_pair(idd, (Link*)0)).second) {
          fprintf(stderr, "ERROR: duplicate interface id %d in ROUTER id=%s\n",
                  idd, get_full_id());
          exit(1);
        }
        bitrate.insert(make_pair(idd, bps));
        latency.insert(make_pair(idd, lat));
        if(typstr) {
          iftype.insert(make_pair(idd, typstr));
          if(!STRCASECMP(typstr, "peering")) {
            if(peering_router) { 
              fprintf(stderr, "ERROR: multiple peering points found\n"); 
              exit(1); 
            }
            peering_router = this;
            peering_iface = idd;
          }
        }
      }
    }
    if(typstr) delete[] typstr;
  }
  delete ss;
}

void Router::print_dml(int sp) {
  printsp(sp);
  if(is_host) printf("host [ id %ld", newid);
  else printf("router [ id %ld", newid);
  if(rtname) printf(" name \"%s\"", name);
  printf("\n");
  for(map<int,Link*>::iterator iter = ifaces.begin();
      iter != ifaces.end(); iter++) {
    double bps = (*bitrate.find((*iter).first)).second;
    //double lat = (*latency.find((*iter).first)).second;

    if(!(*iter).second) { // if dangled
      printsp(sp+2);
      printf("interface [ id %d", (*ifaces_newid.find((*iter).first)).second);
      if(this == peering_router && (*iter).first == peering_iface) {
        printf(" ip %s", ip2str(ipaddr((*iter).first)));
      } else {
        printf(" ip %s", ip2str(ipaddr((*iter).first)+cluster_cidrsize-3));
      }
      if(bps > 0) printf(" bitrate %.0lf", bps);
      //if(lat > 0) printf(" latency %lg", lat);
      map<int,string>::iterator titer = iftype.find((*iter).first);
      if(titer != iftype.end()) printf(" type %s", (*titer).second.c_str());
      printf(" ] # dangled\n");
    }
    else {
      map<Router*,int>::iterator liter =
        (*iter).second->attachments.find(this);
      if((*liter).second == (*iter).first) { // if not detached
        printsp(sp+2);
        printf("interface [ id %d ip %s",
               (*ifaces_newid.find((*iter).first)).second, 
               ip2str(ipaddr((*iter).first)));
        if(bps > 0) printf(" bitrate %.0lf", bps);
        //if(lat > 0) printf(" latency %lg", lat);
        map<int,string>::iterator titer = iftype.find((*iter).first);
        if(titer != iftype.end()) printf(" type %s", (*titer).second.c_str());
        printf(" ]\n");
      }
    }
  }
  if(graphcfg) {
    printsp(sp+2); printf("graph [\n");
    graphcfg->print(stdout, sp+2);
    printsp(sp+2); printf("]\n");
  }
  if(nhicfg) {
    printsp(sp+2); printf("nhi_route [\n");
    nhicfg->print(stdout, sp+2);
    printsp(sp+2); printf("]\n");
  }

  printsp(sp);
  printf("]\n");
}

void Router::print_xml() {
  if(is_host) printf("  <host id=\"%ld\">\n", idseq);
  else printf("  <router id=\"%ld\">\n", idseq);
  for(map<int,Link*>::iterator iter = ifaces.begin();
      iter != ifaces.end(); iter++) {
    if((*iter).second) {
      map<Router*,int>::iterator liter =
        (*iter).second->attachments.find(this);
      if((*liter).second != (*iter).first) { // if detached
        continue;
      } 
      printf("    <interface ip=\"%s\">\n", ip2str(ipaddr((*iter).first)));
    } else { // dangled interface
      if(this == peering_router && (*iter).first == peering_iface) {
        printf("    <interface ip=\"%s\">\n", ip2str(ipaddr((*iter).first)));
      } else {
        printf("    <interface ip=\"%s\">\n", 
               ip2str(ipaddr((*iter).first)+cluster_cidrsize-3));
      }
    }
    printf("      <bitrate>%.0lf</bitrate>\n", (*bitrate.find((*iter).first)).second);
    //printf("      <latency>%lg</latency>\n", (*latency.find((*iter).first)).second);
    printf("      <latency>0</latency>\n");
    printf("    </interface>\n");
  }
  if(is_host) printf("  </host>\n");
  else printf("  </router>\n");
}

void Router::print_iplist() {
  printf("%ld", idseq);
  for(map<int,Link*>::iterator iter = ifaces.begin();
      iter != ifaces.end(); iter++) {
    if((*iter).second) {
      map<Router*,int>::iterator liter =
        (*iter).second->attachments.find(this);
      if((*liter).second != (*iter).first) { // if detached
        continue;
      } 
      printf(" %s", ip2str(ipaddr((*iter).first)));
    } else { // dangled interface
      if(this == peering_router && (*iter).first == peering_iface) {
        printf(" %s", ip2str(ipaddr((*iter).first)));
      } else {
        printf(" %s", ip2str(ipaddr((*iter).first)+cluster_cidrsize-3));
      }
    }
  }
  printf("\n");
}

char NO_NAME[20]="no_name";

void Router::print_envdml(int sp, string& align)
{
  if(!i_nhi) return;

  if (strlen(name) == 0)
    name = sstrdup(NO_NAME);

  printsp(sp);
#ifdef S3FNET_ALIGN_ROUTER
  printf("host [ name %s uid %ld mapto %s alignment %s]\n",
         name, unique_device_id++, i_nhi, align_str.c_str());
#else
  printf("host [ name %s uid %ld mapto %s alignment %s]\n",
         name, unique_device_id++, i_nhi, align.c_str());
#endif
}

char* Router::get_full_id(Net* relnet) {
  if(owner == relnet) {
    sprintf(idbuf, "%ld", id);
    return idbuf;
  } else {
    char* str = owner->get_full_id();
    sprintf(str, "%s:%ld", str, id);
    return str;
  }
}

char* Router::get_full_id(int iface_id, Net* relnet) {
  char* str = get_full_id(relnet);
  sprintf(str, "%s(%d)", str, iface_id);
  return str;
}
void Router::assign_new_ifaces() {
  newid = owner->nrouters++;
  for(map<int,Link*>::iterator iter = ifaces.begin();
      iter != ifaces.end(); iter++) {
    if(!(*iter).second) { // if dangled
      int newif = nifaces++;
      if (newif != (*iter).first) {
        fprintf(stderr, "(dangling) newif %d != old id %d\n",
                newif, (*iter).first);
      }
      ifaces_newid.insert(make_pair((*iter).first, newif));

      // attach cluster to the dangling interface
      map<int,string>::iterator iiter = iftype.find((*iter).first);
      if(iiter == iftype.end() || STRCASECMP((*iiter).second.c_str(), "peering")) {
        owner->clusters.insert
          (make_pair(make_pair(this, (*iter).first), total_routers));
        total_routers += cluster_nnodes;
        total_links += cluster_nlinks+1;
      }
    } else {
      map<Router*,int>::iterator liter =
        (*iter).second->attachments.find(this);
      assert(liter != (*iter).second->attachments.end());
      if((*liter).second == (*iter).first) { // if not detached
        int newif = nifaces++;
	/*
        if (newif != (*iter).first) {
          fprintf(stderr, "Host %s: newif %d != old id %d\n",
                  get_full_id(), newif, (*iter).first);
        }
	*/
        ifaces_newid.insert(make_pair((*iter).first, newif));
      }
    }
  }
}

char* Router::get_new_full_id(Net* relnet) {
  if(owner == relnet) {
    sprintf(idbuf, "%ld", newid);
    return idbuf;
  } else {
    char* str = owner->get_new_full_id();
    sprintf(str, "%s:%ld", str, newid);
    return str;
  }
}

Link* Router::add_link(int lnkid, Link* lnk) {
  map<int,string>::iterator xiter = iftype.find(lnkid);
  if(xiter != iftype.end() && !STRCASECMP((*xiter).second.c_str(), "peering")) {
    fprintf(stderr, "ERROR: %s(%d) is peering point, but used in link\n", 
            get_full_id(), lnkid);
    exit(1);
  }

  map<int,Link*>::iterator iter = ifaces.find(lnkid);
  if(iter == ifaces.end()) {
    fprintf(stderr, "ERROR: %s(%d) not found\n", get_full_id(), lnkid);
    exit(1);
  }
  Link* old = (*iter).second;
  (*iter).second = lnk;
  
  return old;
}

unsigned Router::ipaddr(int iface) {
#ifdef USE_FORWARDING_TABLE
  string nhi = get_full_id();
  GlobalIfacesMap::iterator iter = all_ifaces.find(nhi);
  if (iter == all_ifaces.end())
    assert(0);
  else {
    IfacesMap* imap = iter->second;
    assert(imap);
    IfacesMap::iterator iiter = imap->find(iface);
    if (iiter == imap->end())
      assert(0);
    else return iiter->second;
  }

  fprintf(stderr, "Error, ip for interface %d in host %s not found.\n",
          iface, nhi.c_str());
  return 0; // should never reach here.
#else
  map<int,pair<cidrBlock*,int> >::iterator iter = if_ip.find(iface);
  if(iter == if_ip.end()) {
    if(peering_router) {
      // this is the peering point which hasn't been given an ip address
      return topnet->prefix()+topnet->reserved()-2;
    } else {
      fprintf(stderr, "ERROR: ip address not resolved for %s(%d)\n",
              get_full_id(), iface);
      exit(1);
    }
  } else {
    cidrBlock* cb = (*iter).second.first;
    int seq = (*iter).second.second;
    return cb->prefix()+seq; 
  }
#endif
}

// unsigned Router::ippref(int iface) {
//   map<int,pair<cidrBlock*,int> >::iterator iter = if_ip.find(iface);
//   if(iter == if_ip.end()) {
//     fprintf(stderr, "ERROR: ip address not resolved for %s(%d)\n",
//             get_full_id(), iface);
//     exit(1);
//   }
//   return (*iter).second.first->prefix();
// }

// unsigned Router::ippreflen(int iface) {
//   map<int,pair<cidrBlock*,int> >::iterator iter = if_ip.find(iface);
//   if(iter == if_ip.end()) {
//     fprintf(stderr, "ERROR: ip address not resolved for %s(%d)\n",
//             get_full_id(), iface);
//     exit(1);
//   }
//   return (*iter).second.first->ip_prefix_length;
// }

void Router::generate_environment_info()
{
#ifdef S3FNET_ALIGN_ROUTER
  map<string, EnvironmentMap*>::iterator miter = envMaps.find(align_str);
#else
  map<string, EnvironmentMap*>::iterator miter = envMaps.find(owner->align_str);
#endif

  EnvironmentMap* amap = NULL;
  if (miter == envMaps.end()) {
    fprintf(stderr, "Error::generate_environment_info() alignment %s not found in envMaps.\n",
            owner->align_str.c_str());
    exit(1);
  }
  amap = (*miter).second;

  for(map<int,Link*>::iterator iter = ifaces.begin();
      iter != ifaces.end(); iter++) {
    if((*iter).second) {
      Link* ll = (*iter).second;
      map<Router*,int>::iterator liter =
        ll->attachments.find(this);
      if((*liter).second != (*iter).first) { // if detached
        fprintf(stderr, "mismatched iface attached, got id %d, expect %d.\n",
               (*liter).second, (*iter).first);
        exit(-1);
      }
      // get nhi
      string nhi = get_full_id((*iter).first);
      //fprintf(stderr, "process nhi: %s\n", nhi.c_str());
      // create record.
      EnvironmentInfoLocal* l_rec = new EnvironmentInfoLocal();
      l_rec->src_ip = ipaddr((*iter).first);
      l_rec->delay = ll->delay;
      l_rec->link_prefix_len = ll->ip_prefix_length;
      for (liter = ll->attachments.begin();
           liter != ll->attachments.end();
           liter++) {
        Router* peer_r = (*liter).first; // get peer router
        if (peer_r == this) {
//           if (strlen(name) > 0) {
//             // it has a name, try to assign a interface nhi this should map to
            if (!i_nhi)
              i_nhi = sstrdup(nhi.c_str());
//           }
        }
#ifdef S3FNET_ALIGN_ROUTER
        if (peer_r->align_str == align_str)
#else
        if (peer_r->owner->align_str == owner->align_str)
#endif
        {
          //it's a local peer.
          string peer_nhi = peer_r->get_full_id((*liter).second); // get peer nhi.
          l_rec->local_peers.push_back(peer_nhi);
        } else {
          // a remote peer, record its align_str if needed.
          bool new_align = true;
            for (unsigned i=0; i<l_rec->remote_align.size(); i++) {
#ifdef S3FNET_ALIGN_ROUTER
              if (l_rec->remote_align[i] == peer_r->align_str)
#else
              if (l_rec->remote_align[i] == peer_r->owner->align_str)
#endif
              {
                new_align = false;
                break;
              }
            }
            if (new_align) { // a new alignement met
#ifdef S3FNET_ALIGN_ROUTER
              l_rec->remote_align.push_back(peer_r->align_str);
              miter = envMaps.find(peer_r->align_str);
#else
              l_rec->remote_align.push_back(peer_r->owner->align_str);
              miter = envMaps.find(peer_r->owner->align_str);
#endif
              // create a remote record for the remote alignment
              EnvironmentInfoRemote* r_rec = new EnvironmentInfoRemote();
              r_rec->src_ip = ipaddr((*iter).first);
              r_rec->delay = ll->delay;
              
              // wireless stuff
              r_rec->xpos = peer_r->xpos;
              r_rec->ypos = peer_r->ypos;
              r_rec->zpos = peer_r->zpos;
              
              for (map<Router*,int>::iterator rm_liter = ll->attachments.begin();
                   rm_liter != ll->attachments.end();
                   rm_liter++) {
#ifdef S3FNET_ALIGN_ROUTER
                if ((*rm_liter).first->align_str == peer_r->align_str)
#else
                if ((*rm_liter).first->owner->align_str == peer_r->owner->align_str)
#endif
                {
                  string r_nhi = (*rm_liter).first->get_full_id((*rm_liter).second);
                  r_rec->local_peers.push_back(r_nhi); // local-to-the-remote-align nhi
                }
              }
              (*miter).second->remoteMap.insert(make_pair(nhi, r_rec));
            }
        }
        //}
      }
      // insert record to localMap
      amap->localMap.insert(make_pair(nhi, l_rec));

    } else { // dangled interface, does nothing to it.
//       if(this == peering_router && (*iter).first == peering_iface) {
//         printf(" %s", ip2str(ipaddr((*iter).first)));
//       } else {
//         printf(" %s", ip2str(ipaddr((*iter).first)+cluster_cidrsize-3));
//       }
    }
  }  
}

//int Link::link_id_so_far = 0;

Link::Link(Net* _owner) :
  owner(_owner)
{
  total_links++;
  //id = link_id_so_far++;
}

Link::~Link() {}

void Link::config(Configuration* cfg) {
  char* str = (char*)cfg->findSingle("DELAY");
  if(str) delay = atof(str);
  else delay = 0.0;

  str = (char*)cfg->findSingle("BANDWIDTH");
  if(str) bandwidth = atof(str);
  else bandwidth = 0.0;

  set<Link*> todo;
  Enumeration* attenum = cfg->find("ATTACH");
  while(attenum->hasMoreElements()) {
    str = (char*)attenum->nextElement(); // format: n:h(i)
    Router* rt = owner->find_router(str);
    if(!rt) { 
      fprintf(stderr, "ERROR: can't locate link attachment point %s in NET id=%s\n",
              str, owner->get_full_id());
      exit(1);
    }
    char* p = str; while(*p++ != '('); // get to the interface id
    int iface = atoi(p);

    if(!attachments.insert(make_pair(rt,iface)).second) {
      fprintf(stderr, "ERROR: duplicate link attachment %s in NET id=%s\n",
              str, owner->get_full_id());
      exit(1);
    }

    Link* oldlnk = rt->add_link(iface, this);
    if(oldlnk) { // must be an established link
      assert(oldlnk != this);
      todo.insert(oldlnk);
    }
  }
  delete attenum;
  if(attachments.size() < 2) { 
    fprintf(stderr, "ERROR: two few link attachments in NET id=%s\n",
            owner->get_full_id());
    exit(1);
  }
  //printf("%d: ", id); print_dml(0);

  if(todo.size() > 0) {
    for(set<Link*>::iterator siter = todo.begin(); 
        siter != todo.end(); siter++) {
      //printf("### %d merged with %d\n", id, (*siter)->id);
      merge_with(*siter);
      //printf("%d: ", id); print_dml(0);
      (*siter)->owner->remove_link(*siter);
      delete (*siter);
      total_links--;
    }
  }

  // make consistent: router::bitrate and link::bandwidth
  double bitrate = bandwidth>0 ? bandwidth : 1e38;
  double latency = delay;
  for(map<Router*,int>::iterator iter = attachments.begin();
      iter != attachments.end(); iter++) {
    map<int,double>::iterator biter = 
      (*iter).first->bitrate.find((*iter).second);
    map<int,double>::iterator diter = 
      (*iter).first->latency.find((*iter).second);
    assert(biter != (*iter).first->bitrate.end() &&
           diter != (*iter).first->latency.end());
    if(bitrate > (*biter).second) bitrate = (*biter).second;
    if(latency < (*diter).second) latency = (*diter).second;
  }
  bandwidth = bitrate; delay = latency;
  for(map<Router*,int>::iterator iter = attachments.begin();
      iter != attachments.end(); iter++) {
    map<int,double>::iterator biter = 
      (*iter).first->bitrate.find((*iter).second);
    if(bandwidth != (*biter).second) 
      (*biter).second = bandwidth;
    map<int,double>::iterator diter = 
      (*iter).first->latency.find((*iter).second);
    if(delay != (*diter).second) 
      (*diter).second = delay;
  }
}

void Link::merge_with(Link* lnk) {
  /*
  if(delay != lnk->delay) {
    fprintf(stderr, "ERROR: inconsistent delays when merging links\n");
  }
  if(bandwidth != lnk->bandwidth) {
    fprintf(stderr, "ERROR: inconsistent bandwidth when merging links\n");
  }
  */
  
  int attachidx = 1;
  for(map<Router*,int>::iterator iter = lnk->attachments.begin();
      iter != lnk->attachments.end(); iter++) {
    map<Router*,int>::iterator fiter = attachments.find((*iter).first);
    if(fiter == attachments.end()) {
      if(!attachments.insert(make_pair((*iter).first, (*iter).second)).second)
        assert(0);
      Link* oldlnk = (*iter).first->add_link((*iter).second, this);
      assert(oldlnk == lnk);
    } else if((*fiter).second != (*iter).second) {
      // two interfaces of one router shares one link! 
      // since it's not allowed, we annul it.
      //fprintf(stderr, "WARNING: disconnecting duplicate interface: %s(%d,%d)\n",
      //      (*iter).first->get_full_id(), (*iter).second, (*fiter).second);
      detachments.insert(make_pair((*iter).first, (*iter).second));
      (*iter).first->add_link((*iter).second, this);
    }
  }

  for(set<pair<Router*,int> >::iterator iter = lnk->detachments.begin();
      iter != lnk->detachments.end(); iter++) {
    if(detachments.insert(*iter).second) {
      (*iter).first->add_link((*iter).second, this);
    }
  }
}

void Link::collect_stats(map<double,int>* bdwdist, 
                         map<double,int>* dlydist) {
  map<double,int>::iterator bdwiter = bdwdist->find(bandwidth);
  if(bdwiter != bdwdist->end()) (*bdwiter).second++;
  else bdwdist->insert(make_pair(bandwidth,1));
  map<double,int>::iterator dlyiter = dlydist->find(delay);
  if(dlyiter != dlydist->end()) (*dlyiter).second++;
  else dlydist->insert(make_pair(delay,1));
}

void Link::print_dml(int sp) {
  printsp(sp); printf("link [");
  //printf(" ip_prefix %s/%d", ip2str(prefix()), 32-ip_prefix_length);
  unsigned i = 0;
  for(map<Router*,int>::iterator r_iter = attachments.begin();
      r_iter != attachments.end(); r_iter++, i++) {
    map<int,int>::iterator it = (*r_iter).first->ifaces_newid.find((*r_iter).second);
    assert(it != (*r_iter).first->ifaces_newid.end());
    printf(" attach %s(%d)", (*r_iter).first->get_new_full_id(owner), (*it).second);
    if((i+1)%3 == 0 && i+1<attachments.size()) {
      printf("\n"); 
      printsp(sp+6);
    }
  }
  if(delay > 0) printf(" delay %.lg", delay);
  if(bandwidth > 0) printf(" bandwith %.0lf", bandwidth);
  printf(" ]\n");
}

void Link::print_xml() {
  printf("  <link delay=\"%.lg\" bandwidth=\"%.0lf\">\n", 
         delay, bandwidth);
  for(map<Router*,int>::iterator riter = attachments.begin();
      riter != attachments.end(); riter++) {
    printf("    <attach><node>%ld</node><ip>%s</ip></attach>\n",
           (*riter).first->idseq, ip2str((*riter).first->ipaddr((*riter).second)));
  }
  printf("  </link>\n");
}

void Link::compute_prefix() {
  unsigned required = 2;
  unsigned current = 1;

  // foreach attached interface
  for(map<Router*,int>::iterator iter = attachments.begin();
      iter != attachments.end(); iter++) {
    (*iter).first->if_ip.insert
      (make_pair((*iter).second, make_pair((cidrBlock*)this, current++)));
    required++;
  }
  
  if(required == 2) required = 3;

  // round up to the next power of two
  ip_prefix_length = 32;
  while(required > reserved()) {
    if(ip_prefix_length > 0) ip_prefix_length--;
    else { 
      fprintf(stderr, "Link::compute_prefix: ERROR: cidr space exhausted\n"); 
      exit(1); 
    }
  }
}

void Link::assign_prefix(unsigned baseaddr) {
  ip_prefix = baseaddr;
}

Net::Net(long _id, string align) : 
  owner(NULL), id(_id)
{
  total_nets++;
  topnet = this;
  nrouters = 0;
  align_str = align;
  if (alignStrSet.find(align_str) == alignStrSet.end()) {
    // a new alignment
    alignStrSet.insert(align_str);
  }
}

Net::Net(Net* _owner, long _id) : 
  owner(_owner), id(_id), align_str("")
{
  total_nets++;
  if(!owner) {
    fprintf(stderr, "Error: Net::Net(): topnet must use constructor wither \'align\' parameter.\n");
    exit(1);
  };
  nrouters = 0;
}

Net::~Net()
{
  for(map<long,Net*>::iterator niter = nets.begin();
      niter != nets.end(); niter++) {
    delete (*niter).second;
  }

  for(map<long,Router*>::iterator riter = routers.begin();
      riter != routers.end(); riter++) {
    delete (*riter).second;
  }

  for(set<Link*>::iterator liter = links.begin();
      liter != links.end(); liter++) {
    delete (*liter);
  }
}

void Net::config(Configuration* cfg) {

  if (this == topnet) {
    // get dns entries if this is top net
    Enumeration* denum = cfg->find("dns_entry");
    while (denum->hasMoreElements()) {
      Configuration* dcfg = (Configuration*)denum->nextElement();
      char* str = (char*)dcfg->findSingle("name");
      if (!str) {
        fprintf(stderr, "ERROR: Net::config(): \'name\' attribute missing for dns_entry.\n");
        exit(-1);
      }
      string host_name = str;
      str = (char*)dcfg->findSingle("nhi");
      if (!str) {
        fprintf(stderr, "ERROR: Net::config(): \'nhi\' attribute missing for dns_entry.\n");
        exit(-1);
      }
      string nhi = str;
      dns.insert(std::make_pair(nhi, host_name));
    }
    delete denum;
  }

  // get alignment info --Yougu
  char* str = (char*)cfg->findSingle("alignment");
  if (str) {
    align_str = str;  // either has its own alignment or inherit from the owner Net.
    if (alignStrSet.find(align_str) == alignStrSet.end()) {
      // a new alignment
      alignStrSet.insert(align_str);
    }
  } else if (this != topnet)
    align_str = owner->align_str;

  // get cidr_prefix_len --Yougu
  cidr_prefix_len = 0;
  str = (char*)cfg->findSingle("cidr_prefix_len");
  if (str) {
    cidr_prefix_len = (unsigned)atoi(str);
    if (cidr_prefix_len > 32) {
      fprintf(stderr, "Warning: dmlenv: Net::config(), cidr_prefix_len %d > 32.\n",
              cidr_prefix_len);
    }
  }

  // get all sub-nets
  Enumeration* ss = cfg->find("Net");
  while(ss->hasMoreElements()) {
    Configuration* ncfg = (Configuration*)ss->nextElement();
    char* str = (char*)ncfg->findSingle("id");
    if(str) { 
      long nid = atol(str);
      Net* net = new Net(this, nid);
      net->config(ncfg);
      if(!nets.insert(make_pair(nid, net)).second) {
        if(owner) fprintf(stderr, "ERROR: duplicate subnets id=%ld in NET id=%s\n",
                          nid, get_full_id());
        else fprintf(stderr, "ERROR: duplicate subnets id=%ld\n", nid);
        exit(1);
      }
    } else {
      Configuration* idcfg = (Configuration*)ncfg->findSingle("idrange");
      if(!idcfg) {
        if(owner) fprintf(stderr, "ERROR: missing NET.ID or NET.IDRANGE in NET id=%s\n",
                          get_full_id());
        else fprintf(stderr, "ERROR: missing NET.ID or NET.IDRANGE\n");
        exit(1);
      }
      str = (char*)idcfg->findSingle("FROM");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.FROM\n"); exit(1); }
      long idfrom = atol(str);
      str = (char*)idcfg->findSingle("TO");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.TO\n"); exit(1); }
      long idto = atol(str);
      if(idfrom > idto) { fprintf(stderr, "ERROR: invalid IDRANGE\n"); exit(1); }

      for(long idd=idfrom; idd<=idto; idd++) {
        Net* net = new Net(this, idd);
        net->config(ncfg);
        if(!nets.insert(make_pair(idd, net)).second) {
          if(owner) fprintf(stderr, "ERROR: duplicate subnets id=%ld in NET id=%s\n",
                            idd, get_full_id());
          else fprintf(stderr, "ERROR: duplicate subnets id=%ld\n", idd);
          exit(1);
        }
      }
    }
  }
  delete ss;
  
  // get all hosts/routers
  ss = cfg->find("ROUTER");
  while(ss->hasMoreElements()) {
    Configuration* rcfg = (Configuration*)ss->nextElement();

    char* str = (char*)rcfg->findSingle("id");
    if(str) { 
      long rid = atol(str);
      Router* rt = new Router(this, rid);
      rt->config(rcfg);
      if(!routers.insert(make_pair(rid, rt)).second) {
        if(owner) fprintf(stderr, "ERROR: duplicate router id=%ld in NET id=%s\n",
                          rid, get_full_id());
        else fprintf(stderr, "ERROR: duplicate router id=%ld\n", rid);
        exit(1);
      }
    } else {
      Configuration* idcfg = (Configuration*)rcfg->findSingle("idrange");
      if(!idcfg) {
        if(owner) fprintf(stderr, "ERROR: missing ROUTER.ID or ROUTER.IDRANGE in NET id=%s\n",
                          get_full_id());
        else fprintf(stderr, "ERROR: missing ROUTER.ID or ROUTER.IDRANGE\n");
        exit(1);
      }
      str = (char*)idcfg->findSingle("FROM");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.FROM\n"); exit(1); }
      long idfrom = atol(str);
      str = (char*)idcfg->findSingle("TO");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.TO\n"); exit(1); }
      long idto = atol(str);
      if(idfrom > idto) { fprintf(stderr, "ERROR: invalid IDRANGE\n"); exit(1); }

      for(long idd=idfrom; idd<=idto; idd++) {
        Router *rt = new Router(this, idd);
        rt->config(rcfg);
        if(!routers.insert(make_pair(idd, rt)).second) {
          if(owner) fprintf(stderr, "ERROR: duplicate router id=%ld in NET id=%s\n",
                            idd, get_full_id());
          else fprintf(stderr, "ERROR: duplicate router id=%ld\n", idd);
          exit(1);
        }
      }
    }
  }
  delete ss;

  ss = cfg->find("HOST");
  while(ss->hasMoreElements()) {
    Configuration* rcfg = (Configuration*)ss->nextElement();

    char* str = (char*)rcfg->findSingle("id");
    if(str) { 
      long rid = atol(str);
      Router* rt = new Router(this, rid); rt->is_host = 1;
      rt->config(rcfg);
      if(!routers.insert(make_pair(rid, rt)).second) {
        if(owner) fprintf(stderr, "ERROR: duplicate router id=%ld in NET id=%s\n",
                          rid, get_full_id());
        else fprintf(stderr, "ERROR: duplicate router id=%ld\n", rid);
        exit(1);
      }
    } else {
      Configuration* idcfg = (Configuration*)rcfg->findSingle("idrange");
      if(!idcfg) {
        if(owner) fprintf(stderr, "ERROR: missing ROUTER.ID or ROUTER.IDRANGE in NET id=%s\n",
                          get_full_id());
        else fprintf(stderr, "ERROR: missing ROUTER.ID or ROUTER.IDRANGE\n");
        exit(1);
      }
      str = (char*)idcfg->findSingle("FROM");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.FROM\n"); exit(1); }
      long idfrom = atol(str);
      str = (char*)idcfg->findSingle("TO");
      if(!str) { fprintf(stderr, "ERROR: missing IDRANGE.TO\n"); exit(1); }
      long idto = atol(str);
      if(idfrom > idto) { fprintf(stderr, "ERROR: invalid IDRANGE\n"); exit(1); }

      for(long idd=idfrom; idd<=idto; idd++) {
        Router* rt = new Router(this, idd); rt->is_host = 1;
        rt->config(rcfg);
        if(!routers.insert(make_pair(idd, rt)).second) {
          if(owner) fprintf(stderr, "ERROR: duplicate router id=%ld in NET id=%s\n",
                            idd, get_full_id());
          else fprintf(stderr, "ERROR: duplicate router id=%ld\n", idd);
          exit(1);
        }
      }
    }
  }
  delete ss;

  // get all links
  ss = cfg->find("LINK");
  while(ss->hasMoreElements()) {
    Link* lnk = new Link(this);
    Configuration* lcfg = (Configuration*)ss->nextElement();
    lnk->config(lcfg);
    links.insert(lnk);
  }
  delete ss;
}

void Net::nhistr2vec(string nhi, vector<int>& nhivec)
{
  char* mynhi = sstrdup(nhi.c_str());
  char* p = mynhi;
  while(p) {
    char* q = strchr(p, ':');
    if(q) { *q = 0; q++; }
    if(!strcmp(p, "*")) nhivec.push_back(-1);
    else nhivec.push_back(atoi(p));
    p = q;
  }
  delete[] mynhi;
}

bool Net::nhiset_contains(set<string>* dstset, string id)
{
  if(!dstset) return true;

  vector<int> hostnhi;
  nhistr2vec(id, hostnhi);

  for(set<string>::iterator iter = dstset->begin();
      iter != dstset->end(); iter++) {
    vector<int> dsthostnhi;
    nhistr2vec(*iter, dsthostnhi);
    
    if(hostnhi.size() >= dsthostnhi.size()) {
      int i;
      for(i=0; i<(int)dsthostnhi.size(); i++) {
	if(dsthostnhi[i] == -1) continue;
	else if(dsthostnhi[i] == hostnhi[i]) continue;
	else break;
      }
      if(i == (int)dsthostnhi.size()) return true;
    }
  }
  return false;
}

void Net::populate_routables(set<string>* dstset, set<Router*>& routables, set<Router*>& allrts)
{
  for(map<long,Net*>::iterator n_iter = nets.begin(); 
      n_iter != nets.end(); n_iter++) {
    (*n_iter).second->populate_routables(dstset, routables, allrts);
  }
  for(map<long,Router*>::iterator r_iter = routers.begin();
      r_iter != routers.end(); r_iter++) {
    Router* r = (*r_iter).second;

    // Note that if the router has nhi_routes, unless it's part of the
    // routable set, it's not included in the shortest path
    // calculation. SO PROVIDE ROUTES ONLY TO THE END HOSTS/ROUTERS!!!
    if(nhiset_contains(dstset, r->get_full_id())) {
      routables.insert(r);
      allrts.insert(r);
    } else if(!r->has_routes) allrts.insert(r);
    else continue;

    r->single_iface = -1;
    for(map<int,Link*>::iterator iter2 = r->ifaces.begin();
	iter2 != r->ifaces.end(); iter2++) {
      Link* link = (*iter2).second;
      if(!link) continue;
      if(link->attachments.size() > 2 ||
	 r->single_iface >= 0) { r->single_iface = -1; break; }
      else r->single_iface = (*iter2).first;
    }
  }
}

void Net::remove_link(Link* lnk)
{
  links.erase(lnk);
}

void Net::collect_stats(map<double,int>* bdwdist, 
                        map<double,int>* dlydist) {
  for(map<long,Net*>::iterator n_iter = nets.begin(); 
      n_iter != nets.end(); n_iter++) {
    (*n_iter).second->collect_stats(bdwdist, dlydist);
  }
  for(set<Link*>::iterator l_iter = links.begin(); 
      l_iter != links.end(); l_iter++) {
    (*l_iter)->collect_stats(bdwdist, dlydist);
  }
  for(map<pair<Router*,int>,long>::iterator citer = clusters.begin();
      citer != clusters.end(); citer++) {
    double bdw = (*(*citer).first.first->bitrate.find((*citer).first.second)).second;
    map<double,int>::iterator bdwiter = bdwdist->find(bdw);
    if(bdwiter != bdwdist->end()) (*bdwiter).second++;
    else bdwdist->insert(make_pair(bdw,1));

    map<double,int>::iterator dlyiter = dlydist->find(cluster_delay);
    if(dlyiter != dlydist->end()) (*dlyiter).second++;
    else dlydist->insert(make_pair(cluster_delay,1));
  }
}

void Net::print_dml(int sp) {
  printsp(sp); printf("Net [");
  if(owner) printf(" id %ld", newid);
  //printf(" ip_prefix %s/%d", ip2str(prefix()), 32-ip_prefix_length);
  printf("\n");
  for(map<long,Router*>::iterator r_iter = routers.begin(); 
      r_iter != routers.end(); r_iter++) {
    (*r_iter).second->print_dml(sp+2);
  }
  for(map<long,Net*>::iterator n_iter = nets.begin(); 
      n_iter != nets.end(); n_iter++) {
    (*n_iter).second->print_dml(sp+2);
  }
  for(set<Link*>::iterator l_iter = links.begin(); 
      l_iter != links.end(); l_iter++) {
    (*l_iter)->print_dml(sp+2);
  }
  for(map<pair<Router*,int>,long>::iterator citer = clusters.begin();
      citer != clusters.end(); citer++) {
    unsigned ippref = (*citer).first.first->ipaddr((*citer).first.second);
    long nid = nrouters++;
    printsp(sp+2);
    printf("Net [ id %ld ip_prefix %s/%d _extends localnet.Net ]\n", 
           nid, ip2str(ippref), cluster_prefix_length);   // bug here, nid is not net id. --Yougu
    printsp(sp+2);
    printf("link [ attach %ld:%s attach %ld(%d) delay %lg ]\n",
           nid, cluster_attach_nhi, (*citer).first.first->newid, 
           (*(*citer).first.first->ifaces_newid.find((*citer).first.second)).second,
           cluster_delay);
  }
  printsp(sp); printf("]\n");
}

void Net::generate_environment_info()
{
  for(map<long,Router*>::iterator r_iter = routers.begin(); 
      r_iter != routers.end(); r_iter++) {
    (*r_iter).second->generate_environment_info();
  }

  for(map<long,Net*>::iterator n_iter = nets.begin(); 
      n_iter != nets.end(); n_iter++) {
    (*n_iter).second->generate_environment_info();
  }
}


bool print_cidr_prefix_len = false;

void Net::print_envdml(int sp) {
  // only can be called for topnet.
  assert(topnet == this);
  printsp(sp);
  printf("environment_info [\n");
  // print cidr prefix info if needed.
  if (print_cidr_prefix_len)
    print_net_cidr_envdml(sp+2);
  // print global host info
  print_host_envdml(sp+2);

  for (map<string, EnvironmentMap*>::iterator iter = envMaps.begin();
       iter != envMaps.end(); iter++) {
    if((*iter).second->localMap.size()+(*iter).second->remoteMap.size() == 0)
      continue; // to prevent empty default alignment

    printsp(sp+2);
    printf("alignment [\n");
    printsp(sp+4);
    printf("name %s\n",(*iter).first.c_str());
    EnvironmentMap* cur_map = (*iter).second;
    cur_map->display(sp+4);
    printsp(sp+2);
    printf("]\n");
  }
  printsp(sp);
  printf("]\n");
}

void Net::print_net_cidr_envdml(int sp)
{
  printsp(sp);
//   if (this == topnet)
//     printf("Net [ nhi top prefix %s cidr_prefix_len %u ]\n", ip2str(ip_prefix), ip_prefix_length);
//   else
//     printf("Net [ nhi %s prefix %s cidr_prefix_len %u ]\n", get_full_id(), ip2str(ip_prefix), ip_prefix_length);

  if (this == topnet)
    printf("Net [");
  else printf ("Net [ id %lu prefix %s prefix_int %u prefix_len %u ",
               id, ip2str(ip_prefix), ip_prefix, ip_prefix_length);
  if (nets.size() == 0)
    printf("]\n");
  else {
    printf("\n");
    // recursively print all subnets cidr prefix info
    for (map<long,Net*>::iterator n_iter = nets.begin();
         n_iter != nets.end(); n_iter++) {
      (*n_iter).second->print_net_cidr_envdml(sp+2);
    }
    printsp(sp);
    printf("]\n");
  }
}

void Net::print_host_envdml(int sp)
{
  for(map<long,Router*>::iterator r_iter = routers.begin(); 
      r_iter != routers.end(); r_iter++) {
    (*r_iter).second->print_envdml(sp, align_str);
  }
  for(map<long,Net*>::iterator n_iter = nets.begin(); 
      n_iter != nets.end(); n_iter++) {
    (*n_iter).second->print_host_envdml(sp);
  }
}

// void Net::print_envdml(int sp, string align) {
//   printsp(sp);
//   cout<<align<<" [\n";

//   printsp(sp);
//   cout<<"]\n";
// }

void Net::print_xml_attaches() {
  for(map<pair<Router*,int>,long>::iterator citer = clusters.begin();
      citer != clusters.end(); citer++) {
    unsigned ippref = (*citer).first.first->ipaddr((*citer).first.second);
    long idbase = (*citer).second;
    double bdw = (*(*citer).first.first->bitrate.find((*citer).first.second)).second;
    //if(bdw == 0) bdw = cluster_bandwidth;
    printf("  <import ip_prefix=\"%s/%d\" id_base=\"%ld\">%s</import>\n",
           ip2str(ippref), cluster_prefix_length, idbase, cluster_fname);
    printf("  <link delay=\"%.lg\" bandwidth=\"%.0lf\">\n", 
           cluster_delay, bdw);
    printf("    <attach><node>%ld</node><ip>%s</ip></attach>\n",
           (*citer).first.first->idseq, ip2str(ippref+cluster_cidrsize-3));
    printf("    <attach><node>%ld</node><ip>%s</ip></attach>\n",
           idbase+cluster_attach_id, ip2str(ippref+cluster_cidrsize-2));
    printf("  </link>\n");
  }
  for(map<long,Net*>::iterator niter = nets.begin(); 
      niter != nets.end(); niter++) {
    (*niter).second->print_xml_attaches();
  }
}

void Net::print_xml_routers() {
  for(map<long,Router*>::iterator riter = routers.begin();
      riter != routers.end(); riter++) {
    (*riter).second->print_xml();
  }
  for(map<long,Net*>::iterator niter = nets.begin(); 
      niter != nets.end(); niter++) {
    (*niter).second->print_xml_routers();
  }
}

void Net::print_xml_links() {
  for(set<Link*>::iterator liter = links.begin(); 
      liter != links.end(); liter++) {
    (*liter)->print_xml();
  }
  for(map<long,Net*>::iterator niter = nets.begin(); 
      niter != nets.end(); niter++) {
    (*niter).second->print_xml_links();
  }
}

void Net::print_xml() {
  printf("<!DOCTYPE topology SYSTEM \"topology.dtd\">\n\n");
  printf("<topology nnodes=\"%d\" nlinks=\"%d\">\n", 
         total_routers, total_links);
  print_xml_routers();
  print_xml_links();
  print_xml_attaches();
  printf("</topology>\n");
}

void Net::print_iplist() {
  for(map<long,Router*>::iterator riter = routers.begin();
      riter != routers.end(); riter++) {
    (*riter).second->print_iplist();
  }
  for(map<pair<Router*,int>,long>::iterator citer = clusters.begin();
      citer != clusters.end(); citer++) {
    unsigned ippref = (*citer).first.first->ipaddr((*citer).first.second);
    long idbase = (*citer).second;
    printf("#include \"%s\" %s %ld\n", cluster_fname, ip2str(ippref), idbase);
  }
  for(map<long,Net*>::iterator niter = nets.begin();
      niter != nets.end(); niter++) {
    (*niter).second->print_iplist();
  }
}

Router* Net::find_router(char* str) {
  assert(str);
  if(*str == 0) return 0;
  long myid = atol(str);
  while(*str && isdigit(*str)) str++;
  if(*str == '(') { // this router id
    map<long,Router*>::iterator iter = routers.find(myid);
    if(iter == routers.end()) return 0;
    else return (*iter).second;
  } else if(*str == ':') {
    map<long,Net*>::iterator iter = nets.find(myid);
    if(iter == nets.end()) return 0;
    else return (*iter).second->find_router(str+1);
  } else return 0;
}

char* Net::get_full_id(Net* relnet) {
  if(owner == relnet) {
    sprintf(idbuf, "%ld", id);
    return idbuf;
  } else {
    if(!owner) {
      fprintf(stderr, "ERROR: link attachment outside network!\n");
      exit(1);
    }
    char* str = owner->get_full_id();
    sprintf(str, "%s:%ld", str, id);
    return str;
  }
}

void Net::assign_new_ifaces() {
  if(owner) newid = owner->nrouters++;
  for(map<long,Router*>::iterator r_iter = routers.begin(); 
      r_iter != routers.end(); r_iter++) {
    (*r_iter).second->assign_new_ifaces();
  }
  for(map<long,Net*>::iterator n_iter = nets.begin(); 
      n_iter != nets.end(); n_iter++) {
    (*n_iter).second->assign_new_ifaces();
  }
}

char* Net::get_new_full_id(Net* relnet) {
  if(owner == relnet) {
    sprintf(idbuf, "%ld", newid);
    return idbuf;
  } else {
    if(!owner) {
      fprintf(stderr, "ERROR: link attachment outside network!\n");
      exit(1);
    }
    char* str = owner->get_new_full_id();
    sprintf(str, "%s:%ld", str, newid);
    return str;
  }
}

void Net::compute_prefix() {
  unsigned required = 2;

  // foreach subnet
  for(map<long,Net*>::iterator niter = nets.begin();
      niter != nets.end(); niter++) {
    (*niter).second->compute_prefix();
    required += (*niter).second->reserved();
    myvec.push_back((*niter).second);
  }

  // foreach link
  for(set<Link*>::iterator liter = links.begin();
      liter != links.end(); liter++) {
    (*liter)->compute_prefix();
    required += (*liter)->reserved();
    myvec.push_back(*liter);
  }

  // unattached interfaces introduce subnets
  required += clusters.size()*cluster_cidrsize;
  if(!owner && peering_router) required += 4;

  if(required == 2) required = 3;

  // round up to the next power of two
  ip_prefix_length = 32;
  while(required > reserved()) {
    if(ip_prefix_length > 0) ip_prefix_length--;
    else {
      if (ip_prefix_length == 0 && required <= (unsigned)(0xFFFFFFFF)
          && owner == NULL)
        break; // it is fine, just top net.
      fprintf(stderr, "Net::compute_prefix: ERROR: cidr space exhausted,"
              " required is %u, reserved is %u, ip_prefix_len is %d\n", 
              required, reserved(), ip_prefix_length); 
      exit(1); 
    }
  }

  // if cidr_prefix_len specified, choose the min of the two.
  if (cidr_prefix_len > 0) {
    if (cidr_prefix_len <= ip_prefix_length)
      ip_prefix_length = cidr_prefix_len;
    else {
      fprintf(stderr, "Error, specified cidr_prefix_len %d is not enough,"
              " please change it to be %d\n", cidr_prefix_len, ip_prefix_length);
      exit(-1);
    }
  }
}

struct more_for_descend {
  bool operator()(cidrBlock* const& x, cidrBlock* const& y) {
    return x->reserved() > y->reserved();
  }
};

void Net::assign_prefix(unsigned baseaddr) {
  ip_prefix = baseaddr;

  sort(myvec.begin(), myvec.end(), more_for_descend());
  int metCLUSTER_CIDRSIZE = 0;
  for(vector<cidrBlock*>::iterator viter = myvec.begin();
      viter != myvec.end(); viter++) {
    if(!metCLUSTER_CIDRSIZE && (*viter)->reserved()<=cluster_cidrsize) {
      metCLUSTER_CIDRSIZE = 1;
      for(map<pair<Router*,int>,long>::iterator iiter = clusters.begin();
          iiter != clusters.end(); iiter++) {
        (*iiter).first.first->if_ip.insert
          (make_pair((*iiter).first.second, 
                     make_pair((cidrBlock*)this, baseaddr-ip_prefix)));
        baseaddr += cluster_cidrsize;
      }
    }
    (*viter)->assign_prefix(baseaddr);
    baseaddr += (*viter)->reserved();
    //current += (*viter)->reserved();
  }

  if(!metCLUSTER_CIDRSIZE) {
    for(map<pair<Router*,int>,long>::iterator iiter = clusters.begin();
        iiter != clusters.end(); iiter++) {
      (*iiter).first.first->if_ip.insert
        (make_pair((*iiter).first.second, 
                   make_pair((cidrBlock*)this, baseaddr-ip_prefix)));
      baseaddr += cluster_cidrsize;
    }
  }
}

typedef pair<Router*,double> dijkstra_rank;

struct dijkstra_less : public less<dijkstra_rank> {
  bool operator()(dijkstra_rank t1, dijkstra_rank t2) {
    return t1.second > t2.second; // smaller distance means higher priority
  }
};

typedef priority_queue<dijkstra_rank, vector<dijkstra_rank>, 
		       dijkstra_less> dijkstra_pqueue;

enum { OUTPUT_DML, OUTPUT_XML, OUTPUT_IPLIST, OUTPUT_ENVDML, OUTPUT_VPNCONF, OUTPUT_OSPFROUTES };
static char* execname;

static void usage() {
  fprintf(stderr, "Usage: %s [OPTIONS] <dmlfile> ...\n", execname);
  fprintf(stderr, "-b <net_prefix> : network ip prefix for ip address assignment (default: 10.0.0.0).\n");
  fprintf(stderr, "-h : print out this message.\n");
  fprintf(stderr, "-s : don't print out the cidr prefix info.\n");
  fprintf(stderr, "-r <nhi_1>,..,<nhi_n> : precalculate shortest-path routes to given destinations.\n");
  fprintf(stderr, "-d <distance> : cut-off distance for the shortest path calculation (infinite if not provided).\n");

  /*
  fprintf(stderr, "-t {dml|xml|envdml|iplist} : output format: XML or DML or ENVDML or IPLIST\n");
  fprintf(stderr, "-s <cname> : name of the sub-cluster\n");
  fprintf(stderr, "-b <bandwidth> : bandwidth of the connection to the sub-cluster\n");
  fprintf(stderr, "-d <delay> : delay of the connection to the sub-cluster\n");
  fprintf(stderr, "-n <nnodes> : number of nodes in the sub-cluster\n");
  fprintf(stderr, "-l <nlinks> : number of links in the sub-cluster\n");
  fprintf(stderr, "-c <cidrsize> : cidr block size of the sub-cluster\n");
  fprintf(stderr, "-i <attach_id> : peering router id of the sub-cluster\n");
  fprintf(stderr, "-h <attach_nhi> : peering router nhi of the sub-cluster\n");
  fprintf(stderr, "-r : output router name in DML\n");
  */
  exit(1);
}

int main(int argc, char** argv)
{
  execname = argv[0];

  //  int output_type = OUTPUT_XML;
  int output_type = OUTPUT_ENVDML;
  char* netprefix = "10.0.0.0";
  print_cidr_prefix_len = true;
  char* dst_list = 0;
  double cutoff_dist = 1e38;

  for(;;) {
    int c = getopt(argc, argv, "b:hsr:d:");
    if(c == -1) break;
    switch(c) {
    case 'b': netprefix = optarg; break;
    case 'h': usage(); break;
    case 's': print_cidr_prefix_len = false; break;
    case 'r': 
      output_type = OUTPUT_OSPFROUTES; 
      if(strcasecmp(optarg, "all")) dst_list = optarg; 
      break;
    case 'd': 
      cutoff_dist = atof(optarg); 
      break;
    case '?': break;
    }
  }
  if(argc <= optind) usage();
      
  /*
  for(;;) {
    //int c = getopt(argc, argv, "t:s:b:d:n:l:c:i:h:r");
    int c = getopt(argc, argv, "t:s:d:n:l:c:i:h:r");
    if(c == -1) break;
    switch(c) {
    case 't': {
      if(!strcmp(optarg, "dml")) { output_type = OUTPUT_DML; }
      else if (!strcmp(optarg, "xml")) { output_type = OUTPUT_XML; }
      else if (!strcmp(optarg, "iplist")) { output_type = OUTPUT_IPLIST; }
      else if (!strcmp(optarg, "envdml")) { output_type = OUTPUT_ENVDML; }
      else usage();
      break;
    }
    case 's' : delete[] cluster_fname; cluster_fname = sstrdup(optarg); break; 
    //case 'b': cluster_bandwidth = atof(optarg); break;
    case 'd': cluster_delay = atof(optarg); break;
    case 'n': cluster_nnodes = atoi(optarg); break;
    case 'l': cluster_nlinks = atoi(optarg); break;
    case 'c': cluster_cidrsize = atoi(optarg); break;
    case 'i': cluster_attach_id = atoi(optarg); break;
    case 'h' : delete[] cluster_attach_nhi; cluster_attach_nhi = sstrdup(optarg); break; 
    case 'r': rtname = 1; break;
    case '?': break;
    }
  }
  if(argc <= optind) usage();
  */

  //int optind = 1;
  //if(argc <= optind) usage();

  unsigned cidrsize = cluster_cidrsize>>1;
  cluster_prefix_length = 32;
  while(cidrsize > 0) {
    cluster_prefix_length--;
    cidrsize >>= 1;
  }

  struct in_addr myaddr;
  if(inet_pton(AF_INET, netprefix, &myaddr) <= 0) {
    perror("invalid -b option");
    return 1;
  }
  unsigned ipbase = ntohl(myaddr.s_addr);

  char** dmlfiles = new char*[argc-optind+1];
  for(int i=optind; i<argc; i++) dmlfiles[i-optind] = argv[i];
  dmlfiles[argc-optind] = 0;
  dmlConfig* cfg = new dmlConfig(dmlfiles);
  delete[] dmlfiles;

  Configuration* ncfg = (Configuration*)cfg->findSingle("Net");
  if(ncfg) {
    string default_align;
    char* str = (char*)ncfg->findSingle("alignment");
    if (!str) {
      default_align = "default";
    } else {
      default_align = str;
    }
    alignStrSet.insert(default_align);

    Net* net = new Net(0, default_align);
    net->config(ncfg);

    net->assign_new_ifaces();
    net->compute_prefix();
    net->assign_prefix(ipbase);

#ifdef USE_FORWARDING_TABLE
    Enumeration* fenum = cfg->find("forwarding_table");
    while (fenum->hasMoreElements()) {
      read_in_forwarding_table((Configuration*)
                               fenum->nextElement());
    }
    delete fenum;
#endif

#if 0
    fprintf(stderr, "summary for ip assignment:\n");
    fprintf(stderr, "nnodes = %d\n", total_routers);
    fprintf(stderr, "nlinks = %d\n", total_links);
    fprintf(stderr, "cidrsize = %u\n", topnet->reserved());
    if(peering_router) {
      fprintf(stderr, "attach_id = %ld\n", peering_router->idseq);
      fprintf(stderr, "attach_nhi = %s(%d)\n",
              peering_router->get_new_full_id(), 
              (*peering_router->ifaces_newid.find(peering_iface)).second);
      fprintf(stderr, "attach_ip = %s\n", 
              ip2str(peering_router->ipaddr(peering_iface)));
    }
#endif

    switch (output_type) {
    case OUTPUT_DML: {
      printf("#nnodes = %d\n", total_routers);
      printf("#nlinks = %d\n", total_links);
      printf("#cidrsize = %d\n", topnet->reserved());
      if(peering_router) {
        printf("#attach_id = %ld\n", peering_router->idseq);
        printf("#attach_nhi = %s(%d)\n",
                peering_router->get_new_full_id(), 
                (*peering_router->ifaces_newid.find(peering_iface)).second);
        printf("#attach_ip = %s\n", 
                ip2str(peering_router->ipaddr(peering_iface)));
      }
      net->print_dml(0);
      break;
    }
    case OUTPUT_XML: {
      net->print_xml();
      break;
    }
    case OUTPUT_IPLIST: {
      net->print_iplist();
      break;
    }
    case OUTPUT_ENVDML: {
      // create one envmap for each timeline.
      for (set<string>::iterator iter = alignStrSet.begin();
           iter != alignStrSet.end(); iter++) {
        EnvironmentMap* am = new EnvironmentMap();
        envMaps.insert(make_pair((*iter), am));
      }
      // fill the env info.
      net->generate_environment_info();
      // print it out
      net->print_envdml(0);
      break;
    }

    case OUTPUT_OSPFROUTES: {
      set<Router*> routables;
      set<Router*> allrouters;
      if(dst_list) {
	set<string> nhi_list;
	char* p = dst_list;
	while(p) {
	  char* q = strchr(p, ',');
	  if(q) { *q = 0; q++; }
	  nhi_list.insert(p);
	  p = q;
	}
	net->populate_routables(&nhi_list, routables, allrouters);
      } else net->populate_routables(0, routables, allrouters);

      for(set<Router*>::iterator iter0 = routables.begin();
	  iter0 != routables.end(); iter0++) {
	Router* s = (*iter0);
	fprintf(stdout, "#routable destination: %s\n", s->get_full_id());
	dijkstra_pqueue pqueue;

	for(set<Router*>::iterator iter1 = allrouters.begin();
	    iter1 != allrouters.end(); iter1++) {
	  if((*iter1) == s) {
	    (*iter1)->distance.insert(make_pair(s, 0));
	    (*iter1)->nexthop.insert(make_pair(s, (Router::next_hop_t*)0));
	    pqueue.push(make_pair(*iter1, 0));
	    (*iter1)->visited = false;
	  } else {
	    if((*iter1)->single_iface >= 0) (*iter1)->visited = true;
	    else {
	      (*iter1)->distance.insert(make_pair(s, 1e38));
	      (*iter1)->nexthop.insert(make_pair(s, new Router::next_hop_t(0, 0, 0)));
	      pqueue.push(make_pair(*iter1, 1e32));
	      (*iter1)->visited = false;
	    }
	  }
	}
	
	while(!pqueue.empty()) {
	  Router* u;
	  do {
	    u = pqueue.top().first;
	    pqueue.pop();
	  } while(u->visited && !pqueue.empty());
	  if(u->visited) break;
	  else u->visited = true;
	  if(u->distance[s] >= cutoff_dist) break;

	  for(map<int,Link*>::iterator iter2 = u->ifaces.begin();
	      iter2 != u->ifaces.end(); iter2++) {
	    Link* link = (*iter2).second;
	    if(!link) continue;
	    for(map<Router*,int>::iterator iter3 = link->attachments.begin();
		iter3 != link->attachments.end(); iter3++) {
	      Router* v = (*iter3).first;
	      if(!v->visited && v->distance[s] > u->distance[s]+link->delay) {
		v->distance[s] = u->distance[s]+link->delay;
		v->nexthop[s]->next_router = u;
		v->nexthop[s]->myiface = (*iter3).second;
		v->nexthop[s]->youriface = (*iter2).first;
		pqueue.push(make_pair(v, v->distance[s]));
	      }
	    }
	  }
	}
      }

      for(set<Router*>::iterator iter4 = allrouters.begin();
	  iter4 != allrouters.end(); iter4++) {
	Router* u = (*iter4);

	// no more routes for routers with nhi_routes supplied by the user
	if(u->has_routes) continue;

	if(u->single_iface >= 0) {
	  printf("forwarding_table [ node_nhi %s\n", u->get_full_id());
	  printf("  nhi_route [ dest default interface %d ]\n]\n", u->single_iface);
	  continue;
	}
	
	bool allsame = true;
	int nentries = 0;
	Router::next_hop_t* nxthop = 0;
	for(set<Router*>::iterator iter5 = routables.begin();
	    iter5 != routables.end(); iter5++) {
	  Router* s = (*iter5);
	  if(s == u) continue;
	  if(u->distance[s] >= cutoff_dist) continue; // u is disconnected from s

	  nentries++;
	  if(!nxthop) nxthop = u->nexthop[s];
	  else if(nxthop->next_router != u->nexthop[s]->next_router) {
	    allsame = false; break;
	  }
	}

	if(nentries > 0) {
	  if(allsame) {
	    printf("forwarding_table [ node_nhi %s\n", u->get_full_id());
	    printf("  nhi_route [ dest default interface %d next_hop :%s(%d) ]\n]\n", 
		   nxthop->myiface, nxthop->next_router->get_full_id(),
		   nxthop->youriface);
	  } else {
	    bool firstime = true;
	    for(set<Router*>::iterator iter6 = routables.begin();
		iter6 != routables.end(); iter6++) {
	      Router* s = (*iter6);
	      if(s == u) continue;
	      if(u->distance[s] >= cutoff_dist) continue; // u is disconnected from s
	      
	      for(map<int,Link*>::iterator iter7 = s->ifaces.begin();
		  iter7 != s->ifaces.end(); iter7++) {
		if(!(*iter7).second) continue; // if dangled
		if(firstime) {
		  printf("forwarding_table [ node_nhi %s\n", u->get_full_id());
		  firstime = false;
		}
		printf("  nhi_route [ dest :%s(%d) interface %d ", 
		       s->get_full_id(), (*iter7).first, u->nexthop[s]->myiface);
		printf("next_hop :%s(%d) ]\n", u->nexthop[s]->next_router->get_full_id(),
		       u->nexthop[s]->youriface);
	      }
	    }
	    if(!firstime) printf("]\n");
	  }
	}
      }
      break;
    }
    default: 
      fprintf(stderr, "Error: unknown output type.\n");
      exit(-1);
    } // end of switch(output_type)

#if 0
    map<double,int> bdwdist;
    map<double,int> dlydist;
    net->collect_stats(&bdwdist, &dlydist);
    /*sort(bdwdist.begin(), bdwdist.end());*/
    /*sort(dlydist.begin(), dlydist.end());*/
    fprintf(stderr, "link bandwidth distribution:\n");
    for(map<double,int>::iterator bdwiter = bdwdist.begin();
        bdwiter != bdwdist.end(); bdwiter++) {
      fprintf(stderr, "  %10.0f (bps) : %d\n", (*bdwiter).first, (*bdwiter).second);
    }
    fprintf(stderr, "link delay distribution:\n");
    for(map<double,int>::iterator dlyiter = dlydist.begin();
        dlyiter != dlydist.end(); dlyiter++) {
      fprintf(stderr, "  %.5f (sec) : %d\n", (*dlyiter).first, (*dlyiter).second);
    }
#endif
  }
  delete cfg;
  delete topnet;

  delete[] cluster_fname;
  delete[] cluster_attach_nhi;

  // release memory used for env info
  if (output_type == OUTPUT_ENVDML) {
    for (map<string, EnvironmentMap*>::iterator iter = envMaps.begin();
         iter != envMaps.end(); iter++) {
      delete (*iter).second;
    }
    envMaps.clear();
  }
  alignStrSet.clear();

#ifdef USE_FORWARDING_TABLE
  for (GlobalIfacesMap::iterator iter = all_ifaces.begin();
       iter != all_ifaces.end();
       iter++) {
    delete iter->second;
  }
#endif
    
  return 0;
}

#ifdef USE_FORWARDING_TABLE
void read_in_forwarding_table(Configuration* cfg)
{
  char* str = (char*)cfg->findSingle("node_nhi");
  string nhi = str;
  IfacesMap* ifmap = new IfacesMap();
  all_ifaces.insert(std::make_pair(str, ifmap));

  Enumeration* ienum = cfg->find("interface");
  while (ienum->hasMoreElements()) {
    Configuration* icfg = (Configuration*)ienum->nextElement();
    char* ss = (char*)icfg->findSingle("id");
    assert(ss);
    int id = atoi(ss);

    int len = 32;
    ss= (char*)icfg->findSingle("ip_addr");
    unsigned addr = txt2ip(ss, &len);
    assert(len == 32);

    if (!(ifmap->insert(std::make_pair(id, addr))).second) {
      fprintf(stderr, "Error, duplicate id %d used for host %s\n",
              id, nhi.c_str());
      assert(0); // there is a conflict.
    };
  }
  delete ienum;
}
#endif
