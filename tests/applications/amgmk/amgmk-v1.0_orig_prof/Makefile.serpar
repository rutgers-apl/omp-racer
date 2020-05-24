#BHEADER****************************************************************
# (c) 2007   The Regents of the University of California               *
#                                                                      *
# See the file COPYRIGHT_and_DISCLAIMER for a complete copyright       *
# notice and disclaimer.                                               *
#                                                                      *
#EHEADER****************************************************************
CC       = clang++
LDR      = clang++

#FLOOR    = /bgsys/drivers/ppcfloor

#INCDIR   = -I$(FLOOR) -I/bgsys/drivers/ppcfloor/spi/include/kernel/cnk

CFLAGS   = -c -g  -O3 -fopenmp #-qhot -qsmp=omp -qlanglvl=extc99 -qsimd=auto -qhot=novector -qalias=allp 

LDFLAGS  = -lm -fopenmp #-qsmp=omp -pg

LIBS     = 
#LIB_DIRS =  -L$(FLOOR)/spi/lib -lSPI_l1p -lSPI_cnk  -L/usr/local/tools/mpitrace/lib -lmpihpm -L/bgsys/drivers/ppcfloor/bgpm/lib -lbgpm -lmassv -lmass_simd

PROG     = AMGMk

OBJS     = main.o \
           csr_matrix.o   csr_matvec.o  \
           laplace.o relax.o \
           hypre_error.o hypre_memory.o \
           vector.o

all : $(PROG)

$(PROG) : $(OBJS)
	$(LDR)  $(LDFLAGS) -o $(PROG) $(OBJS) $(LIB_DIRS) $(LIBS)


clean :
	rm -f *.o $(PROG) core job.out *~ 


.SUFFIXES : .o  .c

#*****************************************************************
#* Rules for C object file dependencies
#*****************************************************************
.c.o :
	$(CC) $(CFLAGS) $*.c 







