#include "omprace.h"
#include <omp.h>

#include <stdlib.h>
#include <stdio.h>
double b[1000][1000];

int main(int argc, char* argv[]) 
{
  omprace_init();
  int i,j;
  int n=2, m=100;
omp_set_num_threads(2);
for (i=0;i<n;i++){
  
#pragma omp parallel for
    for (j=1;j<m;j++)
      b[i][j]=b[i][j] * 2;
  }
  printf("b[500][500]=%f\n", b[500][500]);
  omprace_fini();
  return 0;
}
