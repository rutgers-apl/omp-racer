#include <omp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//shared global variable

/*
The example illustrates a read/write data race
on variable x that may be missed by utilizing 
2 w-nodes for read metadata labeled as R1 and R2.

Tasks t1, t2, and t3 are logically parallel. however,
based on the program's task depedencies the following
happens before ordering between t1, t3, and t4
t1 < t4
t3 < t4
Let's assume that the w-node associated with each task 
is labeled w1 to w4 respectively

Now consider the program trace where the tasks excecute 
in the following order:
t1 < t2 < t3 < t4

Before entring the single constract at line 44

x = 1 
R1 = null
R2 = null

after task 1
x = 1
R1 = W1
R2 = null

after task 2
x = 1
R1 = W1
R2 = W2

after task 3
x = 1
R1 = W1
R2 = W3

after task 4
x = 5
R1 = W1
R2 = W3

at task 4, the race detector checks
1) isParallel(R1, W4) which returns false // since t1<t4
1) isParallel(R3, W4) which returns false // since t3<t4

Thus the datarace detector doesn't report the read/write race
between (W2,W4)

*/

int x;
int main() {

  int y;
  x=1;
  #pragma omp parallel
  {  
    #pragma omp single
    {        
      //t1
      #pragma omp task depend (out:y)
      {
        printf("t1 x = %d\n", x);      
      } 
      //t2
      #pragma omp task
      {
        //output may be due to data race 5 or 1
        printf("t2 x = %d\n", x);
      }
      //t3
      #pragma omp task depend (out:y)
      {
        printf("t3 x = %d\n", x);
      }
      //t4
      #pragma omp task depend (in:y)
      {
        //data race between read x:35 and write x:46 
        x+=4;
        printf("t4 x = %d\n", x);
      }
            
    }

  }
  printf("x = %d\n", x);
  return 0;
}