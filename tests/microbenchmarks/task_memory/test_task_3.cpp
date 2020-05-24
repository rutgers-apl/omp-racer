#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_TASKS 100

int main() {
omprace_init();
  int x;
    #pragma omp parallel num_threads(2)
    {
        
    #pragma omp master
        {
            for (int i=0; i<NUM_TASKS; i++){
                #pragma omp task firstprivate(x)
                { 
                int* ptr = (int*)malloc(sizeof(int));
                x++; 
                *ptr = x;
                printf("x task1 addr = %p\n", &x);
                printf("malloc ptr addr = %p\n", ptr);
                }
            }
        }
    }
  omprace_fini();
  return 0;
}