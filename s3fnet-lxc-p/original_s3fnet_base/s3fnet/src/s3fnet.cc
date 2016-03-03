/**
 * \file s3fnet.cc
 *
 * \brief Main function in S3FNet.
 *
 * authors : Dong (Kevin) Jin
 */

#include "s3fnet.h"
#include <ctype.h>
#include <unistd.h>
#include "net/net.h"
#include "util/errhandle.h"

namespace s3f {
namespace s3fnet {

#ifdef MAIN_DEBUG
#define MAIN_DUMP(x) printf("MAIN: "); x
#else
#define MAIN_DUMP(x)
#endif

static void show_usage(char* prognam)
{
  fprintf(stderr, "USAGE: %s [S3FNET-OPTIONS] <dml-file> [<dml-file> ...]\n", prognam);
  fprintf(stderr, "  S3FNET-OPTIONS are command-line arguments for the network simulator.\n");
  fprintf(stderr, "  Available S3FNET-OPTIONS:\n");
  fprintf(stderr, "    -h: show this message\n");
  fprintf(stderr, "    -q: quiet mode (no system messages)\n");
  fprintf(stderr, "  <dml-file> [<dml-file>...]: a list of DML files that altogether define\n"
	  "    the network model (including intermediate DMLs created by utility programs).\n");
  fprintf(stderr, "  e.g., %s test.dml test-env.dml test-rt.dml \n", prognam);
  fprintf(stderr, "  Refer to s3fnet/test for more examples. \n");
}

static void strsub(S3FNET_STRING& cp, S3FNET_STRING oldstr, S3FNET_STRING newstr, int num_times = -1)
{
  int startpos = 0;
  for(int i=0; i!=num_times; i++)
  {
    int loc = cp.find(oldstr, startpos);
    if(loc >= 0)
    {
      cp.replace(loc, oldstr.length(), newstr);
      startpos = loc + newstr.length();
    }
    else
    	break;
  }
}

SimInterface::~SimInterface()
{
	delete topnet;
}

void SimInterface::BuildModel( s3f::dml::dmlConfig* cfg )
{
  int i, j;

  /* initialize the RNG class mutex. This needed to
     make this class thread safe---every new instantiation
     modifies a state that creates the state of the next
     instantiation
  */
  pthread_mutex_init(&RNGS::RngStream::nextState_lock,NULL);

  // create top net
  MAIN_DUMP(printf("creating the top net.\n"));
  topnet = new Net(this);

  MAIN_DUMP(printf("configuring the top net.\n"));
  topnet->config(cfg);

  MAIN_DUMP(printf("initializing the top net.\n"));
  topnet->init();

  // now it's safe to reclaim the dml tree
  delete cfg;
}

}; // namespace s3fnet
}; // namespace s3f

using namespace s3f;
using namespace s3f::s3fnet;

int main(int argc, char** argv)
{
  bool showuse = false;
  bool silent = false;

  for(;;)
  {
    int c = getopt(argc, argv, "hq");
    if(c == -1) break;
    switch(c)
    {
    	case 'h': showuse = true; break;
    	case 'q': silent = true; break;
    	case '?': break;
    }
  }

  if(showuse)
  {
    show_usage(argv[0]); 
    return -1;
  }

  if(argc < optind+1)
  {
    fprintf(stderr, "ERROR: incorrect command-line arguments!\n\n");
    show_usage(argv[0]); 
    return -1;
  }

  MAIN_DUMP(printf("parsing dml files.\n"));
  char** dmlfiles = new char*[argc-optind+1];
  for(int i=optind; i<argc; i++)
  {
    S3FNET_STRING mystr(argv[i]);
    dmlfiles[i-optind] = sstrdup(mystr.c_str());
    //printf("=> %s\n", mystr.c_str());
  }
  dmlfiles[argc-optind] = 0;
  s3f::dml::dmlConfig* dml_cfg = new s3f::dml::dmlConfig(dmlfiles);
  if(silent == false)
  {
    if(argc == optind+1)
    {
    	printf("Input DML file: %s\n", dmlfiles[0]);
    }
    else
    {
      printf("Input DML files:\n");
      for(int k=0; dmlfiles[k]; k++)
      {
    	  printf("  %s\n", dmlfiles[k]);
      }
    }
  }
  for(int j=0; dmlfiles[j]; j++)
  {
	  delete[] dmlfiles[j];
  }
  delete[] dmlfiles;

  //main body
  ltime_t clock;
  char* str;
  int total_timeline;
  int tick_per_second;
  ltime_t sim_single_run_time;
  int seed;

  // parse DML inputs
  str = (char*)dml_cfg->findSingle("total_timeline");
  if(!str) total_timeline = 1; //default is one timeline
  else
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: invalid total_timeline attribute.\n");
    total_timeline = atoi(str);
    if(total_timeline < 1)
      error_quit("ERROR: total_timeline attribute must be a positive integer.\n");
  }

  str = (char*)dml_cfg->findSingle("tick_per_second");
  if(!str) tick_per_second = 0; //default smallest unit is second
  else
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: invalid tick_per_second attribute.\n");
    tick_per_second = atoi(str);
    if(tick_per_second < 0)
      error_quit("ERROR: tick_per_second attribute must be a non-negative integer.\n");
  }

  double run_time_double;
  str = (char*)dml_cfg->findSingle("run_time");
  if(!str)
  {
      error_quit("ERROR: no run_time attribute in DML to specify how long to run the simulation.\n");
  }
  else
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: invalid run_time attribute.\n");
    run_time_double = atof(str);
    if(run_time_double <= 0)
    {
        error_quit("ERROR: run_time attribute must be greater than 0.\n");
    }
  }
  sim_single_run_time = long(run_time_double * pow(10.0, tick_per_second) );

  str = (char*)dml_cfg->findSingle("seed");
  if(!str) seed = 0; //default seed
  else
  {
    if(s3f::dml::dmlConfig::isConf(str))
      error_quit("ERROR: invalid seed attribute.\n");
    seed = atoi(str);
    if(seed < 0)
      error_quit("ERROR: seed attribute must be a non-negative integer.\n");
  }

  MAIN_DUMP(printf("total_timeline = %d, tick_per_second= %d, sim_single_run_time = %ld, seed = %d\n",
		  total_timeline, tick_per_second, sim_single_run_time, seed));

  /* Generate the first random number stream according the seed value.
   * Seed i means that we use the ith random number stream as the first one. */
  for (int i=0; i<seed; i++)
  {
	  Random::RNG* rng = new Random::RNG();
	  if(rng) delete rng;
  }

  // create total timelines and timescale
  SimInterface sim_inf( total_timeline, tick_per_second );

  // build and configure the simulation model
  sim_inf.BuildModel( dml_cfg );

  // initialize the entities (hosts)
  sim_inf.InitModel();

   // run it some window increments
  int num_epoch = 1; //number of epoch to run, currently epoch is set to 1
  for(int i=1; i<=num_epoch; i++)
  {
    cout << "enter epoch window " << i << endl;
    clock = sim_inf.advance(STOP_BEFORE_TIME, sim_single_run_time);
    cout << "completed epoch window, advanced time to " << clock << endl;
  }
  cout << "Finished" << endl;

  // simulation runtime speed measurement
  sim_inf.runtime_measurements();

  return 0;
}
