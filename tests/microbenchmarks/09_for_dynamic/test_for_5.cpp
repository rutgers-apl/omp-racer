#include "omprace.h"
#include <omp.h>
#include <stdio.h>
#include "util.h"

#define NUM_THREADS 2
#define IT_END 12
#define GRAINSIZE 1

//two parallel for loops with barriers in between
int main(int argc, char* argv[])
{

	omprace_init();
	char arr[IT_END] = {0};
#pragma omp parallel for num_threads(NUM_THREADS)
	{
//#pragma omp for //schedule(static, GRAINSIZE)
		for(long i=0; i<IT_END; i++){
			//printf("par for i=%d , tid=%d\n", i ,omp_get_thread_num());
			serialwork(1000000);
			arr[i] = arr[i] * arr[i];
		}

	}
#pragma omp parallel for num_threads(NUM_THREADS)
	{
		for(long i=0; i<IT_END; i++){
			//printf("par for i=%d , tid=%d\n", i ,omp_get_thread_num());
			serialwork(1000000);
			arr[i] = arr[i] * arr[i];
		}

	}

#pragma omp parallel for num_threads(NUM_THREADS)
	{
		for(long i=0; i<IT_END; i++){
			//printf("par for i=%d , tid=%d\n", i ,omp_get_thread_num());
			serialwork(1000000);
			arr[i] = arr[i] * arr[i];
		}

	}
	omprace_fini();

	return 0;
}
