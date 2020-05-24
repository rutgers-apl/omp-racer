
#include "omprace.h"
#include <omp.h>

#include <stdio.h> 
#include <assert.h> 
#include <unistd.h>
int main()
{
  omprace_init();
  int i=0, j, k;
#pragma omp parallel
#pragma omp single
  {
#pragma omp task depend (out:i)
    {
      sleep(3);
      i = 1;    
    }
#pragma omp task depend (in:i)
    j =i; 
#pragma omp task depend (in:i)
    k =i; 
  }
  printf ("j=%d k=%d\n", j, k);
  assert (j==1 && k==1);
  omprace_fini();
  return 0;
} 
