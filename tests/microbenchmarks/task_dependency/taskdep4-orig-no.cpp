
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
#pragma omp task depend (out:j)
    {
      i = 1;    
    }
#pragma omp task depend (in:j) depend(out:k)
    {
      j = 4;
    }
    
#pragma omp task depend (in:k)
    {
      i+=1;
    }
  }
  printf ("i=%d j=%d\n", i, j);
  assert (i==2 && j==4);
  omprace_fini();
  return 0;
} 
