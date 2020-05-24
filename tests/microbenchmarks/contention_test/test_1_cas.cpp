#include <omp.h>
#include <stdio.h>
#include <stdlib.h>



#define NUM_THREADS 16
#define IT_END 134217728
#define GRAINSIZE 1024

//two parallel for loops with barriers in between
int main(int argc, char* argv[])
{
  int* arr =(int*)malloc(sizeof(int) * IT_END);
  
  for(int i=0; i<IT_END; i++){
      arr[i] = i;
  }
  for(int j=0; j<100; j++){
    #pragma omp parallel num_threads(NUM_THREADS)
    {
        #pragma omp for schedule(dynamic, GRAINSIZE)
            for(long i=0; i<IT_END; i++){
                int old_val = arr[i];
                __sync_bool_compare_and_swap(&(arr[i]), old_val, old_val+1);
      
            }
        
    }
   
   }

   printf("arr[0]+arr[N-1] = %d\n", arr[0]+arr[IT_END-1]); 
  return 0;
}
