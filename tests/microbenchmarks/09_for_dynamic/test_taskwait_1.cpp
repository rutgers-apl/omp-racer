#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include "util.h"

#define NUM_THREADS 2 
#define GRAINSIZE 1
#define IT_END 4
//two parallel for loops with barriers in between
int main(int argc, char* argv[])
{

  omprace_init();
  int arr[IT_END] = {0};
  #pragma omp parallel num_threads(NUM_THREADS)
  {
        #pragma omp single
	{
		#pragma omp parallel for schedule(dynamic, GRAINSIZE)
            	for(long i=0; i<IT_END; i++){
            		if (i==0){
				#pragma omp task
				{
					#pragma omp task
					{
					}
					#pragma omp taskwait
				}	
				
			}
			
		
		}

	}
  }

  omprace_fini();
    
  return 0;
}
