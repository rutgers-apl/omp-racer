#include "omprace.h"
#include <omp.h>
#include <stdio.h>
//#include "causalProfiler.h"
#include "util.h"

#define NUM_THREADS 16 

int main(int argc, char* argv[])
{
  omprace_init();
    serialwork(10000000);
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        printf("parallel region, tid=%d\n", omp_get_thread_num());
        serialwork(10000000);
        #pragma omp for 
          for(int i=0; i<21; i++){
            printf("[in program] par for i=%d , tid=%d\n", i ,omp_get_thread_num());
            serialwork(10000000);
          }

    }

        serialwork(10000000);
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        printf("parallel region, tid=%d\n", omp_get_thread_num());
        serialwork(10000000);
        #pragma omp for 
          for(int i=0; i<21; i++){
            printf("[in program] par for i=%d , tid=%d\n", i ,omp_get_thread_num());
            serialwork(10000000);
          }

    }


        serialwork(10000000);
        #pragma omp parallel num_threads(NUM_THREADS)
    {
        printf("parallel region, tid=%d\n", omp_get_thread_num());
        serialwork(10000000);
        #pragma omp for 
          for(int i=0; i<21; i++){
            printf("[in program] par for i=%d , tid=%d\n", i ,omp_get_thread_num());
            serialwork(10000000);
          }

    }

        serialwork(10000000);
        #pragma omp parallel num_threads(NUM_THREADS)
    {
        printf("parallel region, tid=%d\n", omp_get_thread_num());
        serialwork(10000000);
        #pragma omp for 
          for(int i=0; i<21; i++){
            printf("[in program] par for i=%d , tid=%d\n", i ,omp_get_thread_num());
            serialwork(10000000);
          }

    }

    
  serialwork(10000000);
  omprace_fini();
  return 0;
}
