/**
 * dml2xml :- transform network DML into other formats. It is used as
 *            a test example for the dml library.
 *
 * Author: Jason Liu <liux@cis.fiu.edu>
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#define STRCASECMP _stricmp
#else
#include <strings.h>
#define STRCASECMP strcasecmp
#endif

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "dml.h"

using namespace std;
using namespace s3f;
using namespace dml;

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
static char* cluster_attach_nhi = strdup(CLUSTER_ATTACH_NHI);
static char* cluster_fname = strdup(CLUSTER_FNAME);
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

static void printsp(int sp) {
  for(int i=0; i<sp; i++) printf(" ");
}

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

#define IPLENGTH 100
static char ipbuf[IPLENGTH];

static char* ip2str(unsigned ip) {
  unsigned a1 = ip>>24;
  unsigned a2 = (ip>>16)&0xff;
  unsigned a3 = (ip>>8)&0xff;
  unsigned a4 = ip&0xff;
  sprintf(ipbuf, "%u.%u.%u.%u", a1, a2, a3, a4);
  //fprintf(stderr, "### %u => %s\n", ip, ipbuf);
  return ipbuf;
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

class Router {
public:
  int is_host;

  Net* owner; // owner network (not NULL)
  long id;    // relative id
  char* name; // name of the router

  map<int,Link*>  ifaces;
  map<int,double> bitrate;
  map<int,double> latency;
  map<int,string> iftype;

  map<int,pair<cidrBlock*,int> > if_ip;

  unsigned ipaddr(int);
  unsigned ippref(int);
  unsigned ippreflen(int);

  dmlConfig* graphcfg; // used when output DML
  dmlConfig* nhicfg;

  Router(Net* _owner, long _id);
  virtual ~Router();

  void config(Configuration* cfg);

  void print_dml(int sp);
  void print_xml();
  void print_iplist();

  char* get_full_id(Net* relnet = topnet);

  Link* add_link(int lnkid, Link* lnk);

  long newid; // newly assigned router id
  long idseq; // this enumerates all routers defined
  int nifaces; // used to derive new interface ids
  map<int,int> ifaces_newid; // map from old to new interface id
  void assign_new_ifaces();
  char* get_new_full_id(Net* relnet = topnet);
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

  map<long,Net*> nets; // list of subnets
  map<long,Router*> routers; // list of routers
  set<Link*> links; // list of links

  map<pair<Router*,int>,long> clusters; // used for attaching hosts

  Net(Net* _owner, long _id);
  virtual ~Net();

  void config(Configuration* cfg);

  void collect_stats(map<double,int>* bdwdist, map<double,int>* dlydist);

  void print_dml(int sp);
  void print_xml();
  void print_xml_attaches();
  void print_xml_routers();
  void print_xml_links();
  void print_iplist();

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


static int total_routers = 0;
static int total_links = 0;
static int total_nets = 0;

Router::Router(Net* _owner, long _id) : is_host(0), owner(_owner), id(_id) {
  idseq = total_routers++;
  nifaces = 0;
}

Router::~Router() { free(name); }

void Router::config(Configuration* cfg) {
  graphcfg = (dmlConfig*)cfg->findSingle("GRAPH");
  nhicfg = (dmlConfig*)cfg->findSingle("NHI_ROUTE");

  char* str = (char*)cfg->findSingle("NAME");
  if(str) name = strdup(str);
  else name = strdup("");

  Enumeration* ss = cfg->find("INTERFACE");
  while(ss->hasMoreElements()) {
    Configuration* icfg = (Configuration*)ss->nextElement();
    
    double bps, lat; bps = lat = 0.0;
    str = (char*)icfg->findSingle("TYPE");
    char* typstr = str ? strdup(str) : 0;
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
    if(typstr) free(typstr);
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

void Router::assign_new_ifaces() {
  newid = owner->nrouters++;
  for(map<int,Link*>::iterator iter = ifaces.begin();
      iter != ifaces.end(); iter++) {
    if(!(*iter).second) { // if dangled
      int newif = nifaces++;
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
}

unsigned Router::ippref(int iface) {
  map<int,pair<cidrBlock*,int> >::iterator iter = if_ip.find(iface);
  if(iter == if_ip.end()) {
    fprintf(stderr, "ERROR: ip address not resolved for %s(%d)\n",
            get_full_id(), iface);
    exit(1);
  }
  return (*iter).second.first->prefix();
}

unsigned Router::ippreflen(int iface) {
  map<int,pair<cidrBlock*,int> >::iterator iter = if_ip.find(iface);
  if(iter == if_ip.end()) {
    fprintf(stderr, "ERROR: ip address not resolved for %s(%d)\n",
            get_full_id(), iface);
    exit(1);
  }
  return (*iter).second.first->ip_prefix_length;
}

//int Link::link_id_so_far = 0;

Link::Link(Net* _owner) : owner(_owner) {
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
      fprintf(stderr, "ERROR: cidr space exhausted\n"); 
      exit(1); 
    }
  }
}

void Link::assign_prefix(unsigned baseaddr) {
  ip_prefix = baseaddr;
}

Net::Net(Net* _owner, long _id) : owner(_owner), id(_id) {
  total_nets++;
  if(!owner) topnet = this;
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
  // get all sub-nets
  Enumeration* ss = cfg->find("NET");
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
           nid, ip2str(ippref), cluster_prefix_length);
    printsp(sp+2);
    printf("link [ attach %ld:%s attach %ld(%d) delay %lg ]\n",
           nid, cluster_attach_nhi, (*citer).first.first->newid, 
           (*(*citer).first.first->ifaces_newid.find((*citer).first.second)).second,
           cluster_delay);
  }
  printsp(sp); printf("]\n");
}

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
      fprintf(stderr, "ERROR: cidr space exhausted\n"); 
      exit(1); 
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

enum { OUTPUT_DML, OUTPUT_XML, OUTPUT_IPLIST };
static char* execname;

/*
static void usage() {
  fprintf(stderr, "Usage: %s [OPTIONS] <dmlfile> ...\n", execname);
  fprintf(stderr, "-t {dml|xml|iplist} : output format: XML or DML or IPLIST\n");
  fprintf(stderr, "-s <cname> : name of the sub-cluster\n");
  fprintf(stderr, "-b <bandwidth> : bandwidth of the connection to the sub-cluster\n");
  fprintf(stderr, "-d <delay> : delay of the connection to the sub-cluster\n");
  fprintf(stderr, "-n <nnodes> : number of nodes in the sub-cluster\n");
  fprintf(stderr, "-l <nlinks> : number of links in the sub-cluster\n");
  fprintf(stderr, "-c <cidrsize> : cidr block size of the sub-cluster\n");
  fprintf(stderr, "-i <attach_id> : peering router id of the sub-cluster\n");
  fprintf(stderr, "-h <attach_nhi> : peering router nhi of the sub-cluster\n");
  fprintf(stderr, "-r : output router name in DML\n");
  exit(1);
}
*/

int main(int argc, char** argv)
{
  execname = argv[0];

  int output_type = OUTPUT_XML;

  /*
  for(;;) {
    //int c = getopt(argc, argv, "t:s:b:d:n:l:c:i:h:r");
    int c = getopt(argc, argv, "t:s:d:n:l:c:i:h:r");
    if(c == -1) break;
    switch(c) {
    case 't': {
      if(!strcmp(optarg, "dml")) { output_type = OUTPUT_DML; }
      else if(!strcmp(optarg, "xml")) { output_type = OUTPUT_XML; }
      else if(!strcmp(optarg, "iplist")) { output_type = OUTPUT_IPLIST; }
      else usage();
      break;
    }
    case 's' : delete[] cluster_fname; cluster_fname = strdup(optarg); break; 
    //case 'b': cluster_bandwidth = atof(optarg); break;
    case 'd': cluster_delay = atof(optarg); break;
    case 'n': cluster_nnodes = atoi(optarg); break;
    case 'l': cluster_nlinks = atoi(optarg); break;
    case 'c': cluster_cidrsize = atoi(optarg); break;
    case 'i': cluster_attach_id = atoi(optarg); break;
    case 'h' : delete[] cluster_attach_nhi; cluster_attach_nhi = strdup(optarg); break; 
    case 'r': rtname = 1; break;
    case '?': break;
    }
  }
  if(argc <= optind) usage();
  */

  int optind = 1;
  unsigned cidrsize = cluster_cidrsize>>1;
  cluster_prefix_length = 32;
  while(cidrsize > 0) {
    cluster_prefix_length--;
    cidrsize >>= 1;
  }

  char** dmlfiles = new char*[argc-optind+1];
  for(int i=optind; i<argc; i++) dmlfiles[i-optind] = argv[i];
  dmlfiles[argc-optind] = 0;

  try {

  dmlConfig* cfg = new dmlConfig(dmlfiles);
  delete[] dmlfiles;

  Configuration* ncfg = (Configuration*)cfg->findSingle("Net");
  if(ncfg) {
    Net* net = new Net(0, 0);
    net->config(ncfg);

    net->assign_new_ifaces();
    net->compute_prefix();
    net->assign_prefix(0);

    fprintf(stderr, "nnodes = %d\n", total_routers);
    fprintf(stderr, "nlinks = %d\n", total_links);
    fprintf(stderr, "cidrsize = %d\n", topnet->reserved());
    if(peering_router) {
      fprintf(stderr, "attach_id = %ld\n", peering_router->idseq);
      fprintf(stderr, "attach_nhi = %s(%d)\n",
              peering_router->get_new_full_id(), 
              (*peering_router->ifaces_newid.find(peering_iface)).second);
      fprintf(stderr, "attach_ip = %s\n", 
              ip2str(peering_router->ipaddr(peering_iface)));
    }

    if(output_type == OUTPUT_DML) {
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
    } else if(output_type == OUTPUT_XML) {
      net->print_xml();
    } else { 
      net->print_iplist(); 
    }

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
  }
  delete cfg;
  delete topnet;

  } catch(dml_exception& e) { fprintf(stderr, "%s\n", e.what()); }
  

  free(cluster_fname);
  free(cluster_attach_nhi);

  return 0;
}
