#include "omprace.h"
#include <omp.h>
#include <stdio.h>
//#include "util.h"

#include <unistd.h>
#include <stdlib.h>
#define NUM_THREADS 16
#define IT_END 32
#define GRAINSIZE 1


int main(int argc, char* argv[])
{
  omprace_init();
  int arr[IT_END] = {0};
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        #pragma omp for schedule(dynamic, GRAINSIZE) nowait
            for(long i=0; i<IT_END; i++){
                //printf("par for i=%d , tid=%d\n", i ,omp_get_thread_num());
                //serialwork(1000000);
                arr[i] = arr[i] + 1;
            }


     }

    omprace_fini();
//  int* test = (int*)0x000000700000;
  
  //printf("test is %d", *test);
  //*test = 42;
  //printf("test is %d", *test);
//  char buff[300];
//  sprintf(buff, "cat /proc/%d/maps", getpid());
//  system(buff);
  return 0;
}
