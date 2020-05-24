#include "omprace.h"
#include <omp.h>

#include <assert.h> 


int main()
{
  omprace_init();
  int i=0;
#pragma omp parallel num_threads(2)
#pragma omp single
  {
#pragma omp task depend (out:i)
    i = 1;    
#pragma omp task depend (in:i)
    i = 2;    
  }

  assert (i==2);
  omprace_fini();
  return 0;
} 
