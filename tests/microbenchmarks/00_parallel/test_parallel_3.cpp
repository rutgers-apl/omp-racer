#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include "util.h"

#define NUM_THREADS 4

int main(int argc, char* argv[])
{
  omprace_init();
    serialwork(10000000);
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        printf("parallel region, tid=%d\n", omp_get_thread_num());
        serialwork(10000000);

        #pragma omp single
        //#pragma omp master
        {
          #pragma omp task
            serialwork(20000000);
          #pragma omp task
            serialwork(30000000);
        }
    }
  serialwork(10000000);
  omprace_fini();
  return 0;
}
