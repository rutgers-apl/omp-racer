// This code is part of the Problem Based Benchmark Suite (PBBS)
// Copyright (c) 2010 Guy Blelloch and the PBBS team
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

#include "gettime.h"
#include "parallel.h"
#include "sequenceIO.h"
#include "parseCommandLine.h"
#include "sequence.h"
#include "utils.h"
#include <iostream>
#include <algorithm>
using namespace std;
using namespace benchIO;

typedef pair<uintT,intT> uintPair;

template <class T, class OT>
void timeIntegerSort(void* In, intT n, int rounds, char* outFile) {
  T* A = (T*) In;
  T* B = new T[n];
  //OMPP_INIT
  for (int j = 0; j < rounds; j++) {
    parallel_for (intT i=0; i < n; i++) B[i] = A[i];
    startTime();
    integerSort(B,n);
    nextTimeN();
  }
  //OMPP_FINI
  cout << endl;
  if (outFile != NULL) 
    writeSequenceToFile((OT*) B, n, outFile);
  delete A;
  delete B;
}

int parallel_main(int argc, char* argv[]) {
  //initialize profiler
  OMPP_INIT
  commandLine P(argc,argv,"[-o <outFile>] [-r <rounds>] <inFile>");
  char* iFile = P.getArgument(0);
  char* oFile = P.getOptionValue("-o");
  int rounds = P.getOptionIntValue("-r",1);
  startTime();
  seqData D = readSequenceFromFile(iFile);
  elementType dt = D.dt;

  switch (dt) {
  case intType: 
    timeIntegerSort<uintT,intT>(D.A, D.n, rounds, oFile); break;
  case intPairT: 
    timeIntegerSort<uintPair,intPair>(D.A, D.n, rounds, oFile);  break;
  default:
    cout << "integer Sort: input file not of right type" << endl;
    return(1);
  }
  //finish profiler
  OMPP_FINI
  const double MULTIPLIER = 4096.0/(1024.0*1024.0); // 4kB page size, 1024*1024 bytes per MB,                                                                
  FILE* proc_file;
  int total_size_in_pages = 0;
  int res_size_in_pages = 0;

  proc_file = fopen("/proc/self/statm", "r");
  fscanf(proc_file, "%d %d", &total_size_in_pages, &res_size_in_pages);

  fprintf(stderr, "memory_total = %lf MB\n", total_size_in_pages*MULTIPLIER);
  fprintf(stderr, "memory_resident = %lf MB\n", res_size_in_pages*MULTIPLIER);

  return 0;
}

