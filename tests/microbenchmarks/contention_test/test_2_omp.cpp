#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include "omprace.h"

#define NUM_THREADS 16
#define IT_END (1024*16)
#define GRAINSIZE 1024

//two parallel for loops with barriers in between
int main(int argc, char* argv[])
{
  int* arr =(int*)malloc(sizeof(int) * IT_END);
  omprace_init();
  for(int i=0; i<IT_END; i++){
      arr[i] = i;
  }
  for(int j=0; j<100; j++){
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        #pragma omp for schedule(dynamic, GRAINSIZE)
            for(long i=0; i<IT_END; i++){
                arr[i] = arr[i] + 1;
            }
        
    }
   
   }
    printf("arr[0]+arr[N-1] = %d\n", arr[0]+arr[IT_END-1]); 
    omprace_fini();
    return 0;
}
