/*
 * dmlpart :- partitioning the network for distributed simulation
 * according to the machine description.
 */


#include <map>
#include <set>
#include <string>
#include <stdio.h>
#include <stdlib.h>
#include "dml.h"

#include "extra/getopt.h"

using namespace std;
using namespace s3f::dml;

// METIS can either do recursive or k-way. Doesn't really matter.
#define METIS_USE_RECURSIVE

extern "C" {
#include "metis.h"
}

// return an edge weight for the graph; 0 if all the same
idxtype fx(double min_delay, double max_delay, double delay)
{
  idxtype ret;
  if(max_delay == min_delay) ret = 0;
  else {
    double x = (max_delay-delay)/(max_delay-min_delay);
    x = pow(x, 0.25); // make it favorable to large delays
    ret = (idxtype)(x*16384);
  }
  return ret;
}

static char* ssstrdup(char* str)
{
  assert(str);
  int len = strlen(str);
  char* newstr = new char[len+1];
  strcpy(newstr, str);
  return newstr;
}

static void usage(char* program) {
  fprintf(stderr, 
	  "Usage: %s [OPTIONS] <dmlenv_file> [<model_dml_file> ...]\n"
	  "  -h : print out this help message\n"
	  "  -m <m>:[<n0>,]<p0>:[<n1>,]<p1>:... : machine configuration\n"
	  "    <m> is the number of distributed-memory machines for simulation\n"
	  "    <n0>, <n1>, ... are the host names of these machines\n"
	  "    <p0>, <p1>, ... are the number of shared-memory processors on these machines\n"
	  "  -f <dml_filename> : set the base filename of the generated model dmls\n"
	  "For example, if you want to run on 8 single processor machines:\n"
	  "  %s -m 8 env.dml\n"
	  "Or if the simulation is to be run on two dual-processor machines named A and B:\n"
	  "  %s -m 2:A,2:B,2 env.dml\n"
	  "Or if you prefer to use anonymous machines:\n"
	  "  %s -m 2:2:2 env.dml\n",
	  program, program, program, program);
  exit(1);
}

class Node {
public:
  string name;
  int serialno;
  int nentities;
  int machid; 
  map<Node*,double> delays;

  Node(char* n, int id) : name(n), serialno(id), nentities(0) {}
};

int nmachs = 0;
map<string,Node*> timelines;

extern void generate_separate_dmls(char* basename, dmlConfig* modelcfg);

int main(int argc, char* argv[])
{
  int nprocs = 0;
  int* mach_nprocs = 0;
  char** mach_names = 0;
  char* base_filename = 0;
  /*long start_time = time(NULL);*/

  for(;;) {
    int c = getopt(argc, argv, "f:hm:");
    if(c == -1) break;
    switch(c) {
    case 'f': base_filename = optarg; break;
    case 'h': usage(argv[0]); break;
    case 'm': {
      char* str = optarg;
      char* colon = strchr(str, ':');
      if(!colon) {
	nmachs = nprocs = atoi(str);
	if(nmachs<=0) {
	  fprintf(stderr, "ERROR: invalid number of distributed machines: -m\n");
	  return 1;
	}
	mach_nprocs = new int[nmachs]; assert(mach_nprocs);
	mach_names = new char*[nmachs]; assert(mach_names);
	for(int k=0; k<nmachs; k++) {
	  mach_nprocs[k] = 1;
	  mach_names[k] = ssstrdup("localhost");
	}
      } else {
	*colon = 0;
	nmachs = atoi(str);
	str = colon+1;
	if(nmachs<=0) {
	  fprintf(stderr, "ERROR: invalid number of distributed machines: -m\n");
	  return 1;
	}
	mach_nprocs = new int[nmachs]; assert(mach_nprocs);
	mach_names = new char*[nmachs]; assert(mach_names);
	for(int k=0; k<nmachs; k++) {
	  colon = strchr(str, ':');
	  char* substr = str;
	  if(k==nmachs-1) {
	    if(colon) {
	      fprintf(stderr, "ERROR: ill format argument: -m\n");
	      return 1;
	    }
	  } else { 
	    if(!colon) {
	      fprintf(stderr, "ERROR: ill format argument: -m\n");
	      return 1;
	    }
	    *colon = 0; 
	    str = colon+1;
	  }
	  char* comma = strchr(substr, ',');
	  if(comma) {
	    *comma = 0;
	    mach_names[k] = ssstrdup(substr);
	    substr = comma+1;
	  } else mach_names[k] = ssstrdup("localhost");
	  mach_nprocs[k] = atoi(substr);
	  if(mach_nprocs[k]<=0) {
	    fprintf(stderr, "ERROR: invalid number of processors: -m\n");
	    return 1;
	  }
	  nprocs += mach_nprocs[k];
	}
      }
      break;
    }
    case '?': break;
    }
  }
  if(optind > argc) usage(argv[0]);

  if(nmachs == 0) {
    nmachs = nprocs = 1;
    mach_nprocs = new int[1]; mach_nprocs[0] = 1;
    mach_names = new char*[1]; mach_names[0] = ssstrdup("localhost");
  }    

  int inter_timeline_links = 0;
  int nnodes = 0;
  double max_delay = 0;
  double min_delay = 1e308;

  dmlConfig* rootcfg = new dmlConfig(argv[optind]);
  dmlConfig* envcfg = (dmlConfig*)rootcfg->findSingle("environment_info");
  if(!envcfg || !dmlConfig::isConf(envcfg)) {
    fprintf(stderr, "ERROR: invalid ENVIRONMENT_INFO\n");
    return 2;
  }

  Enumeration* hostenum = envcfg->find("host");
  while(hostenum->hasMoreElements()) {
    dmlConfig* cfg = (dmlConfig*)hostenum->nextElement();
    if(!dmlConfig::isConf(cfg)) {
      fprintf(stderr, "ERROR: invalid ENVIRONMENT_INFO.HOST\n");
      return 2;
    }
    char* str = (char*)cfg->findSingle("alignment");
    if(!str || dmlConfig::isConf(str)) {
      fprintf(stderr, "ERROR: invalid ENVIRONMENT_INFO.HOST.ALIGNMENT\n");
      return 2;
    }
    //printf("host alignment: %s\n", str);
    map<string,Node*>::iterator iter = timelines.find(str);
    if(iter != timelines.end()) (*iter).second->nentities++;
    else {
      Node* node = new Node(str, nnodes++);
      node->nentities = 1;
      timelines.insert(make_pair(str, node));
    }
  }
  delete hostenum;

  Enumeration* alignenum = envcfg->find("alignment");
  while(alignenum->hasMoreElements()) {
    dmlConfig* cfg = (dmlConfig*)alignenum->nextElement();
    if(!dmlConfig::isConf(cfg)) {
      fprintf(stderr, "ERROR: invalid ENVIRONMENT_INFO.ALIGNMENT\n");
      return 2;
    }
    char* str = (char*)cfg->findSingle("name");
    if(!str || dmlConfig::isConf(str)) {
      fprintf(stderr, "ERROR: invalid ENVIRONMENT_INFO.ALIGNMENT.NAME\n");
      return 2;
    }
    //printf("alignment: %s\n", str);
    map<string,Node*>::iterator iter = timelines.find(str);
    if(iter == timelines.end()) continue;
    Node* node = (*iter).second;

    Enumeration* ifenum = cfg->find("interface");
    while(ifenum->hasMoreElements()) {
      dmlConfig* ifcfg = (dmlConfig*)ifenum->nextElement();
      str = (char*)ifcfg->findSingle("peer_align");
      if(!str) continue;
      if(dmlConfig::isConf(str)) {
	fprintf(stderr, "ERROR: invalid ENVIRONMENT_INFO.ALIGNMENT.INTERFACE.PEER_ALIGN\n");
	return 2;
      }
      iter = timelines.find(str);
      if(iter == timelines.end()) {
	fprintf(stderr, "ERROR: invalid peer_align %s\n", str);
	return 2;
      }
      Node* anode = (*iter).second;
      str = (char*)ifcfg->findSingle("delay");
      if(!str || dmlConfig::isConf(str)) {
	fprintf(stderr, "ERROR: invalid ENVIRONMENT_INFO.ALIGNMENT.INTERFACE.DELAY\n");
	return 2;
      }
      double delay = atof(str);
      if(delay > max_delay) max_delay = delay;
      if(delay < min_delay) min_delay = delay;
      map<Node*,double>::iterator diter = node->delays.find(anode);
      if(diter != node->delays.end()) {
	if((*diter).second > delay) (*diter).second = delay;
      } else {
	node->delays.insert(make_pair(anode, delay));
	inter_timeline_links++;
      }
      //printf("%s=>%s: %lf\n", node->name.c_str(), anode->name.c_str(), delay);
    }
    delete ifenum;
  }
  delete alignenum;

  delete rootcfg; // get rid of the dml tree

  int n = timelines.size();
  idxtype *xadj = new idxtype[n+1];
  xadj[0] = 0;
  idxtype* adjncy = inter_timeline_links ? new idxtype[inter_timeline_links] : 0;
  idxtype* adjwgt = inter_timeline_links ? new idxtype[inter_timeline_links] : 0;
  int wgtflag = 3;
  int numflag = 0;
  int nparts = nmachs;
  int options[5]; options[0] = 0;
  int edgecut;
  idxtype* vwgt = new idxtype[n];
  idxtype* part = new idxtype[n];

  int i;

  float* tpwgts = new float[nparts];
  for(i=0; i<nmachs; i++) { 
    tpwgts[i] = double(mach_nprocs[i])/nprocs;
    //printf("tgwgts[%d]=%f\n", i, tpwgts[i]);
  }

  if(nparts == 1) {
    for(map<string,Node*>::iterator iter = timelines.begin();
	iter != timelines.end(); iter++)
      (*iter).second->machid = 0;
  } else {
    int x = 0; i = 0; 
    for(map<string,Node*>::iterator iter = timelines.begin();
	iter != timelines.end(); iter++, i++) {
      vwgt[i] = 1+(*iter).second->nentities;
      for(map<Node*,double>::iterator citer = (*iter).second->delays.begin();
	  citer != (*iter).second->delays.end(); citer++) {
	idxtype edge = fx(min_delay, max_delay, (*citer).second);
	if(edge > 0) { // no need to do that if all edges weigh the same
	  adjncy[x] = (*citer).first->serialno;
	  adjwgt[x] = edge;
	  x++;
	}
      }
      xadj[i+1] = x;
    }
    assert(x == 0 || x == inter_timeline_links);

    if(x == 0) wgtflag = 2;
#ifdef METIS_USE_RECURSIVE
    if(nparts <= 8) {
      METIS_WPartGraphRecursive(&n, xadj, x>0?adjncy:0, vwgt, x>0?adjwgt:0, &wgtflag,
				&numflag, &nparts, tpwgts, options, &edgecut, part);
    } else {
#endif
      METIS_WPartGraphKway(&n, xadj, x>0?adjncy:0, vwgt, x>0?adjwgt:0, &wgtflag,
			     &numflag, &nparts, tpwgts, options, &edgecut, part);
#ifdef METIS_USE_RECURSIVE
    }
#endif

    i = 0;
    for(map<string,Node*>::iterator iter = timelines.begin();
	iter != timelines.end(); iter++, i++) {
      (*iter).second->machid = part[i];
    }
  }

  printf("map_info [\n  nnodes %d\n", (int)timelines.size());
  for(map<string,Node*>::iterator iter = timelines.begin();
      iter != timelines.end(); iter++) {
    printf("  map [ alignment \"%s\" machid %d ]\n", 
	   (*iter).first.c_str(), (*iter).second->machid);
  }
  printf("]\n");

  if(argc > optind+1) {
    char** dmlfiles = new char*[argc-optind+2];
    for(int i=optind+1; i<argc; i++) dmlfiles[i-optind-1] = argv[i];
    dmlfiles[argc-optind-1] = 0;
    if(!base_filename) base_filename = dmlfiles[0];
    dmlConfig* modelcfg = new dmlConfig(dmlfiles);
    delete[] dmlfiles;
    generate_separate_dmls(base_filename, modelcfg);
    delete modelcfg;
  }

  for(int i=0; i<nmachs; i++) delete[] mach_names[i];
  delete[] mach_nprocs;
  delete[] mach_names;
  for(map<string,Node*>::iterator iter = timelines.begin();
      iter != timelines.end(); iter++) delete (*iter).second;

  /*
  long end_time = time(NULL);
  fprintf(stderr, "run time for %s on nmachs %d nprocs %d is %lu\n", 
          argv[optind], nmachs, nprocs, end_time - start_time);
  */
  return 0;
}

void strsub(std::string& cp, std::string oldstr, std::string newstr, 
	    int num_times = -1)
{
  int startpos = 0;
  for(int i=0; i!=num_times; i++) {
    int loc = cp.find(oldstr, startpos);
    if(loc >= 0) {
      cp.replace(loc, oldstr.length(), newstr);
      startpos = loc + newstr.length();
    } else break;
  }
}

const char* stringification(std::string str)
{
  strsub(str, "\\", "\\\\");
  strsub(str, "\"", "\\\"");
  strsub(str, "\n", "\\n");
  strsub(str, "\t", "\\t");
  return str.c_str();
}

typedef std::map<std::string,FILE*> OPENFILE_MAP;
OPENFILE_MAP openfile_map;

char* verbatim(dmlConfig* cfg, char* buf, int bufsiz)
{
  FILE* mfile;
  OPENFILE_MAP::iterator iter = openfile_map.find(cfg->location().filename());
  if(iter != openfile_map.end()) mfile = (*iter).second;
  else {
    mfile = fopen(cfg->location().filename(), "r");
    if(!mfile) { 
      fprintf(stderr, "ERROR: can't open file: %s\n", 
	      cfg->location().filename());
      exit(1);
    }
    openfile_map.insert(std::make_pair(cfg->location().filename(), mfile));
  }
  fseek(mfile, cfg->location().startpos(), SEEK_SET);

  int sz = cfg->location().endpos() - cfg->location().startpos();
  if(sz > bufsiz-1) sz = bufsiz-1;
  fread(buf, 1, sz, mfile); 
  buf[sz] = 0;
  return buf;
}

int has_alignment(dmlConfig* cfg, set<string>& align_names, 
		  char* parent_alignment = "default")
{
  char* my_alignment = (char*)cfg->findSingle("alignment");
  if(!my_alignment) my_alignment = parent_alignment;

  bool has_align = false;
  bool has_alien = false;

  for(set<string>::iterator iter = align_names.begin();
      iter != align_names.end(); iter++)
    if(!strcasecmp(my_alignment, (*iter).c_str())) {
      has_align = true;
      break;
    }
  if(!has_align) has_alien = true;

  Enumeration* ss = cfg->find("net");
  while(ss->hasMoreElements()) {
    dmlConfig* ncfg = (dmlConfig*)ss->nextElement();
    if(has_alignment(ncfg, align_names, my_alignment)) has_align = true;
    else has_alien = true;
  }
  delete ss;

#ifdef S3FNET_ALIGN_ROUTER
  ss = cfg->find("router");
  while(ss->hasMoreElements()) {
    dmlConfig* rcfg = (dmlConfig*)ss->nextElement();
    if(has_alignment(rcfg, align_names, my_alignment)) has_align = true;
    else has_alien = true;
  }
  delete ss;

  ss = cfg->find("host");
  while(ss->hasMoreElements()) {
    dmlConfig* hcfg = (dmlConfig*)ss->nextElement();
    if(has_alignment(hcfg, align_names, my_alignment)) has_align = true;
    else has_alien = true;
  }
  delete ss;
#endif

  if(has_align) { if(has_alien) return 1; else return 2; }
  else { assert(has_alien); return 0; }
}

void generate_partitioned_dml(FILE* fptr, set<string>& align_names, dmlConfig* cfg, 
			      int sp = 0, char* parent_alignment = "default")
{
  char* my_alignment = (char*)cfg->findSingle("alignment");
  if(!my_alignment) my_alignment = parent_alignment;

  Enumeration* ss = cfg->find("*");
  while(ss->hasMoreElements()) {
    Configuration* kidcfg = (Configuration*)ss->nextElement();
    if(!DML_ISCONF(kidcfg)) {
      if(sp>0) fprintf(fptr, "%*c%s\t\"%s\"\n", sp, ' ', Configuration::getKey(kidcfg), 
		       stringification((char*)kidcfg));
      else fprintf(fptr, "%s\t\"%s\"\n", Configuration::getKey(kidcfg), 
		   stringification((char*)kidcfg));
      continue;
    } else {
      if(!strcasecmp(Configuration::getKey(kidcfg), "net")) {
	int aligned = has_alignment((dmlConfig*)kidcfg, align_names, my_alignment);
	if(aligned == 1) {
	  if(sp>0) fprintf(fptr, "%*cNET [\n", sp, ' ');
	  else fprintf(fptr, "NET [\n");
	  generate_partitioned_dml(fptr, align_names, (dmlConfig*)kidcfg, sp+1, my_alignment);
	  if(sp>0) fprintf(fptr, "%*c]\n", sp, ' ');
	  else fprintf(fptr, "]\n");
	  continue;
	} else if(!aligned) continue;
      }
#ifdef S3FNET_ALIGN_ROUTER
      else if(!strcasecmp(Configuration::getKey(kidcfg), "host")) {
	int aligned = has_alignment((dmlConfig*)kidcfg, align_names, my_alignment);
	if(aligned == 1) {
	  if(sp>0) fprintf(fptr, "%*cHOST [\n", sp, ' ');
	  else fprintf(fptr, "HOST [\n");
	  generate_partitioned_dml(fptr, align_names, (dmlConfig*)kidcfg, sp+1, my_alignment);
	  if(sp>0) fprintf(fptr, "%*c]\n", sp, ' ');
	  else fprintf(fptr, "]\n");
	  continue;
	} else if(!aligned) continue;
      } else if(!strcasecmp(Configuration::getKey(kidcfg), "router")){
	int aligned = has_alignment((dmlConfig*)kidcfg, align_names, my_alignment);
	if(aligned == 1) {
	  if(sp>0) fprintf(fptr, "%*cROUTER [\n", sp, ' ');
	  else fprintf(fptr, "ROUTER [\n");
	  generate_partitioned_dml(fptr, align_names, (dmlConfig*)kidcfg, sp+1, my_alignment);
	  if(sp>0) fprintf(fptr, "%*c]\n", sp, ' ');
	  else fprintf(fptr, "]\n");
	  continue;
	} else if(!aligned) continue;
      }
#endif
      char* vbuf = new char[102400]; // 100 KB is the limit
      verbatim((dmlConfig*)kidcfg, vbuf, 102400);
      if(sp>0) fprintf(fptr, "%*c%s\n", sp, ' ', vbuf);
      else fprintf(fptr, "%s\n", vbuf);
      delete[] vbuf;
    }
  }
  delete ss;
}

void generate_separate_dmls(char* basename, dmlConfig* modelcfg)
{
  for(int i=0; i<nmachs; i++) {
    set<string> align_names;
    for(map<string,Node*>::iterator iter = timelines.begin();
	iter != timelines.end(); iter++) {
      if((*iter).second->machid == i) 
	align_names.insert((*iter).first);
    }
    if(align_names.empty()) continue;
    char filename[256];
    sprintf(filename, "%d-%s", i, basename);
    FILE* fptr = fopen(filename, "w");
    generate_partitioned_dml(fptr, align_names, modelcfg);
    fclose(fptr);
  }

  for(OPENFILE_MAP::iterator iter = openfile_map.begin();
      iter != openfile_map.end(); iter++) 
    fclose((*iter).second);
}
