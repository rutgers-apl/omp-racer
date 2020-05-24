#include "parallel.h"
#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include "IO.h"
#include <fstream>
#include <iostream>
#include <string.h>


using namespace std;

#define NUM_THREADS 2
#define MAX_LEN 10000

int main(int argc, char* argv[])
{
  omprace_init();
  char arr[MAX_LEN];
  omp_set_num_threads(NUM_THREADS);
  ifstream in("data/2DinSphere_10");
  string f_string((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
  //cout << f_string << endl;
  const char* f_char = f_string.c_str();
  long n = f_string.length();
  strcpy(arr,f_char);
  benchIO::stringToWords(arr, n);
  omprace_fini();
    
  return 0;
}
