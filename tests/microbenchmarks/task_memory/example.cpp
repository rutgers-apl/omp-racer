#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define SIZE 4
int main() {
omprace_init();
  int psum[2];
  int sum;
  int a[4];
    #pragma omp parallel num_threads(2)
    {
        
    #pragma omp for schedule(dynamic, 1)
            for(int i=0; i<SIZE; i++){
              a[i] = i;
            }
    
    #pragma omp single nowait
        {
            //t1
            #pragma omp task 
            { 
              //t11
              #pragma omp task
              {
                psum[0] = a[0]+a[1];
              }
              //t12
              #pragma omp task
              {
                psum[1] = a[2]+a[3];
              }

            }
            #pragma omp taskwait
            //t2
            #pragma omp task
            {
              sum = psum[0] + psum[1];
            }  
            
        }

    }
    printf("sum = %d\n", sum);
  omprace_fini();
  return 0;
}