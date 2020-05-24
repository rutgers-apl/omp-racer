#include "omprace.h"
#include <omp.h>

#include <stdio.h>
#include <assert.h>
//unsigned int input = 30;
unsigned int input = 20;
int fib(unsigned int n)
{
  if (n<2)
    return n;
  else
  {
    int i, j;
#pragma omp task shared(i)
    i=fib(n-1);
#pragma omp task shared(j)
    j=fib(n-2);
#pragma omp taskwait
    return i+j;
  }
}
int main()
{
  omprace_init();
  int result = 0;
//#pragma omp parallel
#pragma omp parallel num_threads(4)
  {
   #pragma omp single
    {
      result = fib(input);
    }
  }
  printf ("Fib(%d)=%d\n", input, result);
  omprace_fini();
  return 0;
}
