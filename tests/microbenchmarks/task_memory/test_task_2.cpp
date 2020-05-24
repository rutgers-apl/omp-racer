#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
omprace_init();
  int x;
    printf("x master addr = %p\n", &x);
    #pragma omp parallel num_threads(2)
    {
        printf("x implicit addr = %p\n", &x);
    #pragma omp master
        {
        #pragma omp task firstprivate(x)
            { 
                int* ptr = (int*)malloc(sizeof(int));
                x++; 
                *ptr = x;
                printf("x task1 addr = %p\n", &x);
                printf("malloc ptr addr = %p\n", ptr);
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