/**
 * dumpdml :- read dml file(s) and then print it out. It is used as
 *            a test example for the dml library.
 *
 * Author: Jason Liu <liux@cis.fiu.edu>
 */

#include <stdio.h>

#include "dml.h"
using namespace s3f;
using namespace dml;

int main(int argc, char** argv)
{
  if(argc < 2) {
    fprintf(stderr, "Usage: %s <dmlfile> ...\n", argv[0]);
    return 1;
  }

  int ndmls = argc-1;
  char** fnames = new char*[ndmls+1];
  for(int k=0; k<ndmls; k++) fnames[k] = argv[1+k];
  fnames[ndmls] = 0;

  try {
    dmlConfig* cfg = new dmlConfig(fnames);
    delete[] fnames;

    cfg->print(stdout, 0);

    delete cfg;
  } 
  catch(dml_exception& e) { fprintf(stderr, "%s\n", e.what()); }

  return 0;
}
