#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include "util.h"

#define NUM_THREADS 4
#define IT_END 1024000
#define GRAINSIZE 256

//two parallel for loops with barriers in between
int main(int argc, char* argv[])
{

  omprace_init();
  int arr[IT_END] = {0};
  for(int j=0; j<10; j++){
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        #pragma omp for schedule(dynamic, GRAINSIZE)
            for(long i=0; i<IT_END; i++){
                //printf("par for i=%d , tid=%d\n", i ,omp_get_thread_num());
                //serialwork(1000000);
                arr[i] = arr[i] * 2;
            }
        //serialwork(10000000);    
        /*#pragma omp for schedule(dynamic, GRAINSIZE)
            for(long i=0; i<IT_END; i++){
                //printf("par for i=%d , tid=%d\n", i ,omp_get_thread_num());
                //serialwork(1000000);
                arr[i] = arr[i] * arr[i];
            }
        */
     }
}
    omprace_fini();
    
  return 0;
}
