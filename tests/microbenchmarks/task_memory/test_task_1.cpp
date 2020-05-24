#include "omprace.h"
#include <omp.h>
#include <stdio.h>

int main() {
omprace_init();
  int x;
  int y;
    printf("x master addr = %p\n", &x);
    #pragma omp parallel num_threads(2)
    {
        printf("x implicit addr = %p\n", &x);
    #pragma omp master
        {
        #pragma omp task private(y)
            { 
                x++; 
                
                printf("x task1 addr = %p\n", &x);
                printf("y task1 addr = %p\n", &y);
            }
        #pragma omp task firstprivate(x)
            { 
                x++; 
                printf("x task2 first private addr = %p\n", &x);
            }
        }
    }
  omprace_fini();
  return 0;
}