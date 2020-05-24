// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2011 Guy Blelloch and the PBBS team
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights (to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <iostream>
#include <algorithm>
#include "gettime.h"
#include "utils.h"
#include "graph.h"
#include "parallel.h"
#include "IO.h"
#include "graphIO.h"
#include "parseCommandLine.h"
#include "MIS.h"
using namespace std;
using namespace benchIO;

void timeMIS(graph<intT> G, int rounds, char* outFile) {
  graph<intT> H = G.copy(); //because MIS might modify graph
  char* flags = maximalIndependentSet(H);
  //OMPP_INIT
  for (int i=0; i < rounds; i++) {
    free(flags);
    H.del();
    H = G.copy();
    startTime();
    flags = maximalIndependentSet(H);
    nextTimeN();
  }
  //OMPP_FINI
  cout << endl;

  if (outFile != NULL) {
    int* F = newA(int, G.n);
    for (int i=0; i < G.n; i++) F[i] = flags[i];
    writeIntArrayToFile(F, G.n, outFile);
    free(F);
  }

  free(flags);
  G.del();
  H.del();
}

int parallel_main(int argc, char* argv[]) {
  //initialize profiler
  OMPP_INIT
  commandLine P(argc, argv, "[-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r",1);
  graph<intT> G = readGraphFromFile<intT>(iFile);
  timeMIS(G, rounds, oFile);
  //finish profiler
    const double MULTIPLIER = 4096.0/(1024.0*1024.0); // 4kB page size, 1024*1024 bytes per MB,                                                                
  FILE* proc_file;
  int total_size_in_pages = 0;
  int res_size_in_pages = 0;
  OMPP_FINI
  proc_file = fopen("/proc/self/statm", "r");
  fscanf(proc_file, "%d %d", &total_size_in_pages, &res_size_in_pages);

  fprintf(stderr, "memory_total = %lf MB\n", total_size_in_pages*MULTIPLIER);
  fprintf(stderr, "memory_resident = %lf MB\n", res_size_in_pages*MULTIPLIER);
  
  return 0;
}
