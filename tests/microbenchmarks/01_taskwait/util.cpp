#include "util.h"

volatile static double ompp_a = 0.5, ompp_b = 2.2;

void dummy( void *array )
{
/* Confuse the compiler so as not to optimize
   away the flops in the calling routine    */
/* Cast the array as a void to eliminate unused argument warning */
	( void ) array;
}

void serialwork( int n )
{
	int i;
	double c = 0.11;

	for ( i = 0; i < n; i++ ) {
		c += ompp_a * ompp_b;
	}
	dummy( ( void * ) &c );
}
