#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include "util.h"

#define NUM_THREADS 4

int main(int argc, char* argv[])
{
  omprace_init();
    //serialwork(10000000);
    #pragma omp parallel num_threads(NUM_THREADS)
    {
	printf("parallel region, tid=%d\n", omp_get_thread_num());
	serialwork(10000000);
    }
   // ompprof_sync();
  serialwork(20000000);
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        printf("parallel region 2, tid=%d\n", omp_get_thread_num());
	      serialwork(10000000);
    }
//  ompprof_sync();
  omprace_fini();
  return 0;
}
