#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include <unistd.h>

#define NUM_THREADS 2 

int main()
{
    omprace_init();
  #pragma omp parallel num_threads(NUM_THREADS)
  #pragma omp single
  {
    #pragma omp task
    {
      #pragma omp critical
      printf ("Task 1\n");

      #pragma omp task
      {
        sleep(1);
        #pragma omp critical
        printf ("Task 2\n");
      }
    }

    #pragma omp taskwait

    #pragma omp task
    {
      #pragma omp critical
      printf ("Task 3\n");
    }
  }
omprace_fini();
  return 0;
}