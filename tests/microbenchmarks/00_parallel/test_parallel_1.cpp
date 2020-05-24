#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include "util.h"

#define NUM_THREADS 2

int main(int argc, char* argv[])
{
  omprace_init();
    serialwork(10000000);
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        printf("parallel region, tid=%d\n", omp_get_thread_num());
        serialwork(10000000);
    }
    printf("out of parallel region\n");
    //ompprof_sync();
  serialwork(10000000);
  omprace_fini();
  return 0;
}
