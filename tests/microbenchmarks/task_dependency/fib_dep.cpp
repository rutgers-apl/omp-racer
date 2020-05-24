//#include "omprace.h"
#include <omp.h>
#include<stdio.h>
#include <assert.h> 

int fib(int n){
    int x, y, s;
    if (n < 2){
        return n;
    } 
    else{
    #pragma omp task shared(x) depend(out:x)
    {
        x = fib(n-1);
    }
    #pragma omp task shared(y) depend(out:y)
    {
        y = fib(n-2);
    }
    #pragma omp task shared(x,y,s) depend(in:x,y)
    {
        s = x+y;
    }
    //#pragma omp taskwait
    //s = x+y;
    return s; 
    }
}

int main()
{
  //omprace_init();
  int res=0;

#pragma omp parallel  
{
#pragma omp single 
  {
      res = fib(4);
      printf("fib(4) = %d\n",res);
  }
}

  
  //omprace_fini();
  return 0;
} 
