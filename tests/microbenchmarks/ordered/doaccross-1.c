#include <omp.h>
#include "omprace.h"
#include <stdio.h>

float foo(int i){
    return (float)i*2;
}
float bar(float a, float b){
    return a*b;
}
float baz(float b){
    return 2*b + 2;
}

void work( int N, float *A, float *B, float *C )
{
  int i;
  #pragma omp parallel num_threads(2)
  {
  #pragma omp for ordered(1)
  for (i=1; i<N; i++)
  {
    A[i] = foo(i);

  #pragma omp ordered depend(sink: i-1)
    B[i] = bar(A[i], B[i-1]);
  #pragma omp ordered depend(source)
    //printf("b[%d] = %f\n", i, )
    C[i] = baz(B[i]);
  }
  }
}

#define NUM 3
int main(){
    omprace_init();
    float a[] = {1.0,3.0,5.0};
    float b[] = {2.0,2.0,2.0};
    float c[] = {4.0,5.0,6.0};
    
    work(NUM,a,b,c);
    printf("c[1] = %f\n", c[1]);    
    omprace_fini();
    return 0;
}