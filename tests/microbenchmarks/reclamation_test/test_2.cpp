#include "omprace.h"
#include <omp.h>

#include <stdlib.h>
#include <stdio.h>
double b[1000][1000];

int main(int argc, char* argv[]) 
{
  omprace_init();
  int i,j;
  int n=1000, m=1000;
//omp_set_num_threads(1);
  for (i=0;i<n;i++){
  //printf("i = %d\n", i);
#pragma omp parallel for
    for (j=1;j<m;j++)
      b[i][j]=b[i][j-1];
  }
  printf("b[500][500]=%f\n", b[500][500]);
  omprace_fini();
  return 0;
}
